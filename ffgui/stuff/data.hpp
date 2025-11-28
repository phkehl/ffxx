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

#ifndef __DATA_HPP__
#define __DATA_HPP__

//
#include "ffgui_inc.hpp"
//
#include <list>
//
#include <ffxx/epoch.hpp>
using namespace ffxx;
#include <fpsdk_common/parser/types.hpp>
using namespace fpsdk::common::parser;
//

namespace ffgui {
/* ****************************************************************************************************************** */

using DataSrcDst = void*;

enum class DataType
{
    MSG,
    EPOCH,
    DEBUG,
    EVENT,
};

const char* DataTypeStr(const DataType type);

// ---------------------------------------------------------------------------------------------------------------------

struct Data
{
    Data() = delete;
    Data(const DataType type, const DataSrcDst srcId) : /* clang-format off */
        srcId_   { srcId },
        type_    { type },
        time_    { Time::FromClockRealtime() }
    { } // clang-format off
    virtual ~Data() = default;

    DataSrcDst srcId_;
    DataType type_;
    Time time_;
};

struct DataMsg : public Data
{
    enum Origin : char // clang-format off
    {
        UNKNOWN = '?',
        RCVD    = '<',
        SENT    = '>',
        VIRTUAL = 'V',
        // EPOCH   = 'E',
        // USER    = 'U',
        // LOG     = 'L',
    };

    DataMsg(const DataSrcDst srcId, const ParserMsg& msg, const Origin origin) /* clang-format off */ :
        Data(DataType::MSG, srcId), msg_ { msg }, origin_ { origin } { msg_.MakeInfo(); } // clang-format on

    ParserMsg msg_;
    Origin origin_;

    static const char* OriginStr(const Origin origin);
};

struct DataEpoch : public Data
{
    DataEpoch(const DataSrcDst srcId, const Epoch& epoch) /* clang-format off */ :
        Data(DataType::EPOCH, srcId), epoch_ { epoch } { } // clang-format on
    Epoch epoch_;
};

struct DataDebug : public Data
{
    DataDebug(const DataSrcDst srcId, const LoggingLevel level, const std::string& str) /* clang-format off */ :
        Data(DataType::DEBUG, srcId), level_ { level }, str_ { str }  // clang-format on
    {

    }
    LoggingLevel level_;
    std::string str_;
};

struct DataEvent : public Data
{
    enum Event {
        DBUPDATE,
    };
    DataEvent(const DataSrcDst srcId, const Event event) /* clang-format off */ :
        Data(DataType::EVENT, srcId), event_ { event } { } // clang-format on
    Event event_;
};

// clang-format off
using DataPtr      = std::shared_ptr<const Data>;
using DataMsgPtr   = std::shared_ptr<const DataMsg>;
using DataEpochPtr = std::shared_ptr<const DataEpoch>;
using DataDebugPtr = std::shared_ptr<const DataDebug>;
using DataEventPtr = std::shared_ptr<const DataEvent>;
using DataList     = std::list<DataPtr>;

inline DataMsgPtr       DataPtrToDataMsgPtr(     const DataPtr&      data) { return std::dynamic_pointer_cast<const DataMsg>(data); }
inline const DataMsg&   DataPtrToDataMsg(        const DataPtr&      data) { return dynamic_cast<const DataMsg&>(*data); }
inline const DataMsg&   DataMsgPtrToDataMsg(     const DataMsgPtr&   data) { return dynamic_cast<const DataMsg&>(*data); }

inline DataEpochPtr     DataPtrToDataEpochPtr(   const DataPtr&      data) { return std::dynamic_pointer_cast<const DataEpoch>(data); }
inline const DataEpoch& DataPtrToDataEpoch(      const DataPtr&      data) { return dynamic_cast<const DataEpoch&>(*data); }
inline const DataEpoch& DataEpochPtrToDataMsg(   const DataEpochPtr& data) { return dynamic_cast<const DataEpoch&>(*data); }

inline DataDebugPtr     DataPtrToDataDebugPtr(   const DataPtr&      data) { return std::dynamic_pointer_cast<const DataDebug>(data); }
inline const DataDebug& DataPtrToDataDebug(      const DataPtr&      data) { return dynamic_cast<const DataDebug&>(*data); }
inline const DataDebug& DataDebugPtrToDataDebug( const DataDebugPtr& data) { return dynamic_cast<const DataDebug&>(*data); }

inline DataEventPtr     DataPtrToDataEventPtr(   const DataPtr&      data) { return std::dynamic_pointer_cast<const DataEvent>(data); }
inline const DataEvent& DataPtrToDataEvent(      const DataPtr&      data) { return dynamic_cast<const DataEvent&>(*data); }
inline const DataEvent& DataEventPtrToDataEvent( const DataEventPtr& data) { return dynamic_cast<const DataEvent&>(*data); }

// clang-format on

// ---------------------------------------------------------------------------------------------------------------------

using DataObserver = std::function<void(const DataPtr& data)>;
bool AddDataObserver(const DataSrcDst srcId, DataSrcDst dstId, DataObserver observer);
bool RemoveDataObserver(const DataSrcDst srcId, DataSrcDst dstId); // 0 = wildcard

// ---------------------------------------------------------------------------------------------------------------------

// Put messages/epochs into queue for later dispatch. Called from Receiver::_Worker() etc.
void CollectData(DataPtr data);
bool CollectData(DataMsgPtr data, EpochCollector& coll);

// Put event into queue
void CollectDebug(DataSrcDst srcId, const LoggingLevel level, const std::string& str);
void CollectDebug(DataSrcDst srcId, const LoggingLevel level, const char* str);

// Dispatch to observers, called from GuiApp::Loop() (main thread)
void DispatchData();

void DrawDataDebug();

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __DATA_HPP__
