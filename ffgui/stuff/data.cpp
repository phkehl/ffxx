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
#include <mutex>
//
#include "data.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

#define DEBUG_PTRS 0

struct Observer
{
    DataSrcDst srcId_ = nullptr;
    DataSrcDst dstId_ = nullptr;
    DataObserver fn_;
#ifndef NDEBUG
    uint64_t count_ = 0;
#endif
};

static std::vector<Observer> gObservers;
static std::mutex gObserversMutex;

static DataList gCollectList;
static std::mutex gCollectMutex;

#if DEBUG_PTRS
static DataList gDebugList;
static std::mutex gDebugListMutex;
#endif

// ---------------------------------------------------------------------------------------------------------------------

bool AddDataObserver(const DataSrcDst srcId, DataSrcDst dstId, DataObserver observer)
{
    std::unique_lock<std::mutex> lock(gObserversMutex);

    for (auto& obs : gObservers) {
        if ((obs.srcId_ == srcId) && (obs.dstId_ == dstId)) {
            WARNING("AddDataObserver %p -> %p: already registered", srcId, dstId);
            return false;
        }
    }

    DEBUG("AddDataObserver %p -> %p", srcId, dstId);
    gObservers.push_back({ srcId, dstId, observer });
    return true;
}

bool RemoveDataObserver(const DataSrcDst srcId, const DataSrcDst dstId)
{
    std::unique_lock<std::mutex> lock(gObserversMutex);

    bool ok = false;
    for (auto it = gObservers.begin(); it != gObservers.end();) {
        if ( ((srcId == 0) || (it->srcId_ == srcId)) && ((dstId == 0) || (it->dstId_ == dstId)) ) {
            DEBUG("RemoveDataObserver %p -> %p", srcId, dstId);
            it = gObservers.erase(it);
            ok = true;
            break;
        } else {
            it++;
        }
    }
    if (!ok) {
        WARNING("RemoveDataObserver %p -> %p: no such observer", srcId, dstId);
    }
    return ok;
}

// ---------------------------------------------------------------------------------------------------------------------

void CollectData(DataPtr data)
{
    if (data) {
        std::unique_lock<std::mutex> lock(gCollectMutex);
        // TRACE("CollectData %s %-5s %p: %s", data->time_.StrIsoTime(3).c_str(), DataTypeStr(data->type_), data->srcId_, data->msg_.name_.c_str());
        gCollectList.push_back(data);
    }
}

bool CollectData(DataMsgPtr data, EpochCollector& coll)
{
    bool atEpoch = false;
    if (data) {
        auto epoch = coll.Collect(data->msg_);
        const bool triggerIsEoe = (epoch && StrContains(data->msg_.name_, "EOE"));
        if (epoch) {
            atEpoch = true;
        }
        if (!triggerIsEoe && epoch) {
            CollectData(std::make_shared<DataEpoch>(data->srcId_, *epoch));
        }
        CollectData(data);
        if (triggerIsEoe && epoch) {
            CollectData(std::make_shared<DataEpoch>(data->srcId_, *epoch));
        }
    }
    return atEpoch;
}

// ---------------------------------------------------------------------------------------------------------------------

void CollectDebug(DataSrcDst srcId, const LoggingLevel level, const std::string& str)
{
    CollectData(std::make_shared<DataDebug>(srcId, level, str));
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

void CollectDebug(DataSrcDst srcId, const LoggingLevel level, const char* str)
{
    CollectDebug(srcId, level, std::string(str));
}

// ---------------------------------------------------------------------------------------------------------------------

void DispatchData()
{
    DataList dispatchList;
    {
        std::unique_lock<std::mutex> lock(gCollectMutex);
        std::swap(dispatchList, gCollectList);  // or: dispatchList.swap(gCollectList);
        gCollectList.clear();
    }

    // DEBUG("dispatch %" PRIuMAX " %" PRIuMAX, dispatchList.size(), gCollectList.size());

    std::unique_lock<std::mutex> lock(gObserversMutex);
#if DEBUG_PTRS
    std::unique_lock<std::mutex> lock2(gDebugListMutex);
#endif
    for (const auto& data : dispatchList) {
#if DEBUG_PTRS
        gDebugList.push_back(data);
        // DEBUG("store %s count %" PRIuMAX " %p", DataTypeStr(data->type_), data.use_count(), data.get());
#endif
        for (auto& obs : gObservers) {
            if (obs.srcId_ == data->srcId_) {
                obs.fn_(data);
#ifndef NDEBUG
                obs.count_++;
#endif
            }
        }
    }
    dispatchList.clear();
}

// ---------------------------------------------------------------------------------------------------------------------

const char* DataTypeStr(const DataType type)
{
    switch (type) {  // clang-format off
        case DataType::MSG:   return "MSG";
        case DataType::EPOCH: return "EPOCH";
        case DataType::DEBUG: return "DEBUG";
        case DataType::EVENT: return "EVENT";
    } // clang-format on

    return "?";
}

// ---------------------------------------------------------------------------------------------------------------------

void DrawDataDebug()
{
#ifdef NDEBUG
    ImGui::Text("Disabled in release build");
#else
    std::size_t nMsg = 0;
    std::size_t nEpoch = 0;
    std::size_t nDebug = 0;
    std::size_t nEvent = 0;
    Time tMsg;
    Time tEpoch;
    Time tDebug;
    Time tEvent;

#if DEBUG_PTRS
    {
        std::unique_lock<std::mutex> lock(gDebugListMutex);

        for (auto it = gDebugList.begin(); it != gDebugList.end(); ) {
            const DataPtr& d = *it;
            DEBUG("Data %s %-5s %p %3" PRIuMAX " %s", d->time_.StrIsoTime(3).c_str(),
                DataTypeStr(d->type_), d->srcId_, d.use_count(), d.unique() ? "remove" : "keep");
            DEBUG("check %s count %" PRIuMAX " %p", DataTypeStr(d->type_), d.use_count(), d.get());
            if (it->unique()) {
                it = gDebugList.erase(it);
            } else {
                const auto& data = *it;
                switch (data->type_) { // clang-format off
                    case DataType::MSG:   nMsg++;   if (tMsg.IsZero()   || (data->time_ < tMsg))   { tMsg   = data->time_; } break;
                    case DataType::EPOCH: nEpoch++; if (tEpoch.IsZero() || (data->time_ < tEpoch)) { tEpoch = data->time_; } break;
                    case DataType::DEBUG: nDebug++; if (tDebug.IsZero() || (data->time_ < tDebug)) { tDebug = data->time_; } break;
                    case DataType::EVENT: nEvent++; if (tEvent.IsZero() || (data->time_ < tEvent)) { tEvent = data->time_; } break;
                }  // clang-format on
                it++;
            }
        }
    }
#endif

    const auto now = Time::FromClockRealtime();  // clang-format off
    Gui::TextTitle("Data");
    ImGui::Text("MSG:   count %3" PRIuMAX " oldest %s", nMsg,   tMsg.IsZero()   ? "(none)" : (now - tMsg).Stringify().c_str());
    ImGui::Text("EPOCH: count %3" PRIuMAX " oldest %s", nEpoch, tEpoch.IsZero() ? "(none)" : (now - tEpoch).Stringify().c_str());
    ImGui::Text("DEBUG: count %3" PRIuMAX " oldest %s", nDebug, tDebug.IsZero() ? "(none)" : (now - tDebug).Stringify().c_str());
    ImGui::Text("EVENT: count %3" PRIuMAX " oldest %s", nEvent, tEvent.IsZero() ? "(none)" : (now - tEvent).Stringify().c_str()); // clanf-format on

    Gui::TextTitle("Observers");
    {
        std::unique_lock<std::mutex> lock(gObserversMutex);
        for (const auto& obs : gObservers) {
            ImGui::Text("%p -> %p %10" PRIuMAX, obs.srcId_, obs.dstId_, obs.count_);
        }
    }

#endif
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ const char* DataMsg::OriginStr(const Origin origin)
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
