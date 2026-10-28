/* ****************************************************************************************************************** */
// flipflip's gui (ffgui)
//
// Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
// https://oinkzwurgl.org/projaeggd/ffxx/
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>.

//
#include "ffgui_inc.hpp"
//
#include "input.hpp"
#include "receiver.hpp"
#include "logfile.hpp"
#include "database.hpp"
//

namespace ffgui {
/* ****************************************************************************************************************** */

const char* InputDataTypeStr(const InputDataType type)
{
    switch (type) {  // clang-format off
        case InputDataType::MSG:   return "MSG";
        case InputDataType::EPOCH: return "EPOCH";
        case InputDataType::DEBUG: return "DEBUG";
        case InputDataType::EVENT: return "EVENT";
    } // clang-format on

    return "?";
}

// ---------------------------------------------------------------------------------------------------------------------

Input::Input(const std::string& name, const InputType type) /* clang-format off */ :
    name_    { name },
    type_    { type },
    uid_     { ++lastUid_ },
    thread_  { name_, std::bind(&Input::_Worker, this, std::placeholders::_1) }  // clang-format on
{
    switch (type) {
        case InputType::InputType_Receiver:
        case InputType::InputType_Logfile:
            coll_ = std::make_unique<EpochCollector>(EpochCollector::Mode::RECEIVER);
            break;
        case InputType::InputType_Corr:
            coll_ = std::make_unique<EpochCollector>(EpochCollector::Mode::CORRECTIONS);
            break;
        case InputType::InputType_None:
            break;
    }

    thread_.Start();
}

Input::~Input()
{
    thread_.Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

const std::string Input::DebugStr() const
{
    return Sprintf("q: %" PRIuMAX "/%" PRIuMAX " r: %p l: %p c: %p d: %p", queue_.size(), queueMax_, receiver_.get(),
        logfile_.get(), corr_.get(), database_.get());
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ InputPtrMap Input::inputs_;
/*static*/ uint64_t Input::lastUid_ = 0;

/*static*/ InputPtr Input::Create(const std::string& name, const InputType type)
{
    DEBUG("Input(%s) create", name.c_str());

    auto input = std::make_shared<Input>(name, type);

    switch (type) {
        case InputType::InputType_Receiver:
            input->receiver_ = std::make_unique<Receiver>(name, InputColl(*input));
            input->database_ = std::make_unique<Database>(name, InputColl(*input));
            break;
        case InputType::InputType_Logfile:
            input->logfile_ = std::make_unique<Logfile>(name, InputColl(*input));
            input->database_ = std::make_unique<Database>(name, InputColl(*input));
            break;
        case InputType::InputType_Corr:
            input->corr_ = std::make_unique<Receiver>(name, InputColl(*input));
            break;
        case InputType::InputType_None:
            break;
    }

    return inputs_.emplace(name, input).first->second;
}

/*static*/ void Input::Remove(const std::string& name)
{
    auto entry = inputs_.find(name);
    if (entry != inputs_.end()) {
        DEBUG("Input(%s) remove", entry->second->name_.c_str());
        inputs_.erase(entry);
    }
}

/*static*/ const InputPtrMap& Input::GetAll()
{
    return inputs_;
}

// ---------------------------------------------------------------------------------------------------------------------

bool Input::AddObserver(InputDataObserverId id, InputDataObserverFn fn)
{
    std::unique_lock<std::mutex> lock(obs_mutex_);

    if (std::find_if(obs_.begin(), obs_.end(), [id](const auto& cand) { return cand.id_ == id; }) != obs_.end()) {
        WARNING("Input(%s) observer %p already registered", name_.c_str(), id);
        return false;
    }

    DEBUG("Input(%s) add observer %p", name_.c_str(), id);
    obs_.push_back({ id, fn });

    return true;
}

bool Input::RemoveObserver(InputDataObserverId id)
{
    std::unique_lock<std::mutex> lock(obs_mutex_);

    bool ok = false;
    for (auto it = obs_.begin(); it != obs_.end();) {
        if (it->id_ == id) {
            DEBUG("Input(%s) remove observer %p", name_.c_str(), id);
            it = obs_.erase(it);
            ok = true;
            break;
        } else {
            it++;
        }
    }
    if (!ok) {
        WARNING("Input(%s) observer %p not registered", name_.c_str(), id);
    }
    return ok;
}

// ---------------------------------------------------------------------------------------------------------------------

bool Input::_Worker(Thread& thread)
{
    DEBUG("Input(%s) worker start", name_.c_str());
    SetThreadName("in:" + name_, 0);

    while (!thread.ShouldAbort()) {
        std::size_t queueSize = 0;
        InputDataPtr data;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (!queue_.empty()) {
                data = std::move(queue_.front());
                queue_.pop();
                queueSize = queue_.size();
            }
        }
        if (data) {
            // DEBUG("Input(%s) data pop %s %s", name_.c_str(), InputDataTypeStr(data->type_), data->time_.StrIsoTime().c_str());
            {
                std::unique_lock<std::mutex> lock(obs_mutex_);
                for (auto& obs : obs_) {
                    obs.fn_(data);
                }
            }

            queueSize_ = queueSize;
            if (queueSize_ <= WAIT_QUEUE) {
                queueSem_.Notify();
            }
            continue;
        }

        thread.Sleep(1234);
    }
    DEBUG("Input(%s) worker stop", name_.c_str());
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

void InputColl::WaitCollect()
{
    while (true) {
        if (input_.queueSize_ <= Input::WAIT_QUEUE) {
            return;
        }
        input_.queueSem_.WaitFor(100);
    }
}

void InputColl::CollectData(const InputDataPtr& data)
{
    if (!data) {
        return;
    }

    std::unique_lock<std::mutex> lock(input_.queue_mutex_);
    // TRACE("InputCollect %s %-5s %p: %s", data->time_.StrIsoTime(3).c_str(), DataTypeStr(data->type_), data->srcId_, data->msg_.name_.c_str());
    if (input_.queueSize_ < input_.MAX_QUEUE) {
        //DEBUG("Input(%s) data push %s %s", name_.c_str(), InputDataTypeStr(data->type_), data->time_.StrIsoTime().c_str());
        input_.queue_.push(data);
        input_.queueSize_++;
        if (input_.queueSize_ > input_.queueMax_) {
            input_.queueMax_ = input_.queueSize_;
        }
    } else {
        WARNING_THR(1000, "Input %s queue ovfl", input_.name_.c_str());
    }
    input_.thread_.Wakeup();
}

bool InputColl::CollectMsg(const InputDataMsgPtr& data)
{
    if (!data) {
        return false;
    }

    if (!input_.coll_) {
        CollectData(data);
        return false;
    }


    bool atEpoch = false;
    auto epoch = input_.coll_->Collect(data->msg_);
    const bool triggerIsEoe = (epoch && (StrContains(data->msg_.name_, "EOE") || StrContains(data->msg_.name_, "ENDOF")));
    if (epoch) {
        atEpoch = true;
    }
    if (!triggerIsEoe && epoch) {
        CollectData(std::make_shared<InputDataEpoch>(*epoch));
    }
    CollectData(data);
    if (triggerIsEoe && epoch) {
        CollectData(std::make_shared<InputDataEpoch>(*epoch));
    }

    return atEpoch;
}

void InputColl::CollectLog(const LoggingLevel level, const std::string& str)
{
    CollectData(std::make_shared<InputDataDebug>(level, str));
    // Also output to console
    switch (level) {  // clang-format off
        case LoggingLevel::FATAL:   FATAL(  "%s", str.c_str()); break;
        case LoggingLevel::ERROR:   ERROR(  "%s", str.c_str()); break;
        case LoggingLevel::WARNING: WARNING("%s", str.c_str()); break;
        case LoggingLevel::NOTICE:  NOTICE( "%s", str.c_str()); break;
        case LoggingLevel::INFO:    INFO(   "%s", str.c_str()); break;
        case LoggingLevel::DEBUG:   DEBUG(  "%s", str.c_str()); break;
        case LoggingLevel::TRACE:   TRACE(  "%s", str.c_str()); break;
    }  // clang-format on
}

void InputColl::CollectLog(const LoggingLevel level, const char* str)
{
    CollectLog(level, std::string(str));
}

void InputColl::Reset()
{
    if (input_.coll_) {
        input_.coll_->Reset();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ const char* InputDataMsg::OriginStr(const Origin origin)
{
    switch (origin) { // clang-format off
        case Origin::UNKNOWN:   return "UNKNOWN";
        case Origin::RCVD:      return "RCVD";
        case Origin::SENT:      return "SENT";
        case Origin::VIRTUAL:   return "VIRTUAL";
    } // clang-format on
    return "?";
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
