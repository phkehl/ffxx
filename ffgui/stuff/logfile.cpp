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
#include <cerrno>
#include <fstream>

//
#include "logfile.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

Logfile::Logfile(const std::string& name, InputColl&& coll) /* clang-format off */ :
    name_     { name },
    coll_     { coll },
    worker_   { name, std::bind(&Logfile::_Worker, this, std::placeholders::_1) }  // clang-format on

{
}

Logfile::~Logfile()
{
}

// ---------------------------------------------------------------------------------------------------------------------

bool Logfile::Open(const std::string& path)
{
    if (!CanOpen()) {
        return false;
    }

    logfile_ = std::make_unique<InputFile>();

    bool ok = true;
    if (logfile_->Open(path)) {
        size_ = logfile_->Size();
        pos_ = 0;
        posRel_ = 0.0f;
        atEnd_ = false;
        coll_.Reset();
    } else {
        ok = false;
    }

    if (ok && worker_.Start()) {
        workerStarted_ = true;
    } else {
        ok = false;
    }

    if (!ok) {
        coll_.CollectLog(LoggingLevel::ERROR, Sprintf("Failed opening %s: %s", path.c_str(), logfile_->Error().c_str()));
    }

    return ok;
}

// ---------------------------------------------------------------------------------------------------------------------

void Logfile::Close()
{
    if (!CanClose()) {
        return;
    }

    state_ = State::CLOSED;
    if (workerStarted_) {
        worker_.Stop();
        logfile_.reset();
        workerStarted_ = false;
    }

    size_ = 0;
    parser_.Reset();
}

// ---------------------------------------------------------------------------------------------------------------------

void Logfile::Play()
{
    if (CanPlay()) {
        state_ = State::PLAYING;
        worker_.Wakeup();
        atEnd_ = false;
    }
}

void Logfile::Stop()
{
    if (CanStop()) {
        state_ = State::STOPPED;
        const auto path = logfile_->Path();
        logfile_->Close();
        logfile_->Open(path);
        pos_ = 0;
        posRel_ = 0.0f;
        atEnd_ = false;
        worker_.Wakeup();
    }
}

void Logfile::Pause()
{
    if (CanPause()) {
        state_ = State::PAUSED;
        worker_.Wakeup();
    }
}

void Logfile::StepMsg(const std::string &msgName)
{
    if (CanStep()) {
        stepMsg_ = true;
        stepMsgName_ = msgName;
        stepEpoch_ = false;
        Play();
    }
}

void Logfile::StepEpoch()
{
    if (CanStep()) {
        stepEpoch_ = true;
        stepMsg_ = false;
        stepMsgName_.clear();
        Play();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool Logfile::CanOpen() const
{
    return state_ == State::CLOSED;
}

bool Logfile::CanClose() const
{
    return state_ != State::CLOSED;
}

bool Logfile::CanPlay() const
{
    return ((state_ == State::STOPPED) || (state_ == State::PAUSED)) && !atEnd_;
}

bool Logfile::CanStop() const
{
    return (state_ == State::PLAYING) || (state_ == State::PAUSED);
}

bool Logfile::CanPause() const
{
    return (state_ == State::PLAYING); // || (state_ == STOPPED);
}

bool Logfile::CanStep() const
{
    return ((state_ == State::PLAYING) || (state_ == State::PAUSED) || (state_ == State::STOPPED)) && !atEnd_;
}

bool Logfile::CanSeek() const
{
    return (/*(state_ == State::PLAYING) ||*/ (state_ == State::PAUSED) || (state_ == State::STOPPED)) && logfile_ &&
           logfile_->CanSeek();
}

const char* Logfile::StateStr() const
{
    switch (state_) { // clang_format off
        case State::CLOSED:  return "Closed";
        case State::STOPPED: return "Stopped";
        case State::PLAYING: return "Playing";
        case State::PAUSED:  return "Paused";
    }
    return "?";
}  // clang_format on

// ---------------------------------------------------------------------------------------------------------------------

float Logfile::GetRelPos() const
{
    return posRel_;
}

void Logfile::SetRelPos(const float pos)
{
    if (CanSeek()) {
        // Set it already here, so that GetPlayPos() returns the new pos, so that the seekbar in GuiWinLogfile doesn't
        // jump...
        posRel_ = std::clamp(pos, 0.0f, 1.0f);

        if (posRel_ < FLT_MIN) {
            posRel_ = 0.0f;
            pos_ = 0;
        } else {
            if (posRel_ > (1.0 - FLT_EPSILON)) {
                posRel_ = 1.0f;
            }
            pos_ = (double)size_ * (double)posRel_;
            if (pos_ > size_) {
                pos_ = size_.load();
            }
        }
        logfile_->Seek(pos_);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void Logfile::SetSpeed(const SpeedMode mode, const float value)
{
    speedMode_ = mode;
    speedValue_ = std::clamp(value, SPEED_MIN, SPEED_MAX);
    // DEBUG("SetSpeed nolimit=%s %.1f", ToStr(speedMode_ == SpeedMode::NOLIMIT), speedValue_.load());
    worker_.Wakeup();
}

// ---------------------------------------------------------------------------------------------------------------------

bool Logfile::_Worker(Thread& thread)
{
    DEBUG("Logfile(%s) worker start", name_.c_str());
    SetThreadName("lf:" + name_, 0);

    bool done = false;
    ParserMsg msg;
    state_ = State::STOPPED;
    parser_.Reset();

    while (!thread.ShouldAbort() && !done) {
        switch (state_) {
            case State::CLOSED:
                done = true;
                break;
            case State::PAUSED:
            case State::STOPPED:
                thread.Sleep(123);
                break;
            case State::PLAYING: {
                // First process messages already read from file
                if (parser_.Process(msg)) {
                    coll_.WaitCollect();
                    const bool atEpoch = coll_.CollectMsg(std::make_shared<InputDataMsg>(msg, InputDataMsg::RCVD));
                    pos_ += msg.Size();
                    posRel_ = (double)pos_.load() /(double)size_.load();

                    // Step
                    if (atEpoch && stepEpoch_) {
                        stepEpoch_ = false;
                        state_ = State::PAUSED;
                        break;
                    }
                    if (stepMsg_ && (stepMsgName_.empty() || (stepMsgName_ == msg.name_))) {
                        stepMsg_ = false;
                        stepMsgName_.clear();
                        state_ = State::PAUSED;
                        break;
                    }

                    // Throttle
                    if (!stepEpoch_ && !stepMsg_ &&
                        ((speedMode_ == SpeedMode::MSG) || (atEpoch && (speedMode_ == SpeedMode::EPOCH)))) {
                            thread.Sleep(1000.0f / speedValue_);
                    }

                    break; // next message
                }
                // Read more data from file
                const std::size_t size = logfile_->Read(buf_.data(), buf_.size());
                if (size > 0) {
                    if (!parser_.Add(buf_.data(), size)) {
                        coll_.Reset();
                        coll_.CollectLog(LoggingLevel::WARNING, name_ + " parser ovfl");
                        parser_.Reset();
                        parser_.Add(buf_.data(), size);
                    }
                }
                // End of file reached
                else {
                    pos_ = size_.load();
                    posRel_ = 1.0f;
                    atEnd_ = true;
                    state_ = State::PAUSED;
                }
                break;
            }
        }
    }

    pos_ = 0;
    posRel_ = 0.0f;
    state_ = State::CLOSED;

    DEBUG("Logfile(%s) worker stop", name_.c_str());

    return true;
}
// ---------------------------------------------------------------------------------------------------------------------

void Logfile::Clear()
{
    coll_.Reset();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
