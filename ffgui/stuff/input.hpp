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

#ifndef __INPUT_HPP__
#define __INPUT_HPP__

//
#include "ffgui_inc.hpp"
//
#include <atomic>
#include <queue>
//
#include <ffxx/epoch.hpp>
using namespace ffxx;
#include <fpsdk_common/parser/types.hpp>
#include <fpsdk_common/thread.hpp>
using namespace fpsdk::common::parser;
using namespace fpsdk::common::thread;
//
// #include "receiver.hpp"
// #include "logfile.hpp"
// #include "database.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

enum InputType
{
    InputType_None = 0,
    InputType_Receiver = (1 << 0),
    InputType_Logfile = (1 << 1),
    InputType_Corr = (1 << 2),
};

inline InputType operator|(const InputType a, const InputType b)
{
    return static_cast<InputType>(static_cast<int>(a) | static_cast<int>(b));
}

// ---------------------------------------------------------------------------------------------------------------------

enum class InputDataType { MSG, EPOCH, DEBUG, EVENT };

const char* InputDataTypeStr(const InputDataType type);

struct InputData
{
    InputData() = delete;
    InputData(const InputDataType type) : type_ { type }, time_ { Time::FromClockRealtime() } { }
    virtual ~InputData() = default;

    InputDataType type_;
    Time time_;
};

struct InputDataMsg : public InputData
{
    enum Origin : char { UNKNOWN = '?', RCVD = '<', SENT = '>', VIRTUAL = 'V', };

    InputDataMsg(const ParserMsg& msg, const Origin origin) :
        InputData(InputDataType::MSG), msg_ { msg }, origin_ { origin } { msg_.MakeInfo(); }

    ParserMsg msg_;
    Origin origin_;

    static const char* OriginStr(const Origin origin);
};

struct InputDataEpoch : public InputData
{
    InputDataEpoch(const Epoch& epoch) : InputData(InputDataType::EPOCH), epoch_ { epoch } { }

    Epoch epoch_;
};

struct InputDataDebug : public InputData
{
    InputDataDebug(const LoggingLevel level, const std::string& str) : InputData(InputDataType::DEBUG), level_ { level }, str_ { str } { }

    LoggingLevel level_;
    std::string str_;
};

struct InputDataEvent : public InputData
{
    enum Event { DBUPDATE };
    InputDataEvent(const Event event) : InputData(InputDataType::EVENT), event_ { event } { }
    Event event_;
};

// clang-format off
using InputDataPtr      = std::shared_ptr<const InputData>;
using InputDataMsgPtr   = std::shared_ptr<const InputDataMsg>;
using InputDataEpochPtr = std::shared_ptr<const InputDataEpoch>;
using InputDataDebugPtr = std::shared_ptr<const InputDataDebug>;
using InputDataEventPtr = std::shared_ptr<const InputDataEvent>;

inline InputDataMsgPtr       DataPtrToDataMsgPtr(     const InputDataPtr&      data) { return std::dynamic_pointer_cast<const InputDataMsg>(data); }
inline const InputDataMsg&   DataPtrToDataMsg(        const InputDataPtr&      data) { return dynamic_cast<const InputDataMsg&>(*data); }
inline const InputDataMsg&   DataMsgPtrToDataMsg(     const InputDataMsgPtr&   data) { return dynamic_cast<const InputDataMsg&>(*data); }

inline InputDataEpochPtr     DataPtrToDataEpochPtr(   const InputDataPtr&      data) { return std::dynamic_pointer_cast<const InputDataEpoch>(data); }
inline const InputDataEpoch& DataPtrToDataEpoch(      const InputDataPtr&      data) { return dynamic_cast<const InputDataEpoch&>(*data); }
inline const InputDataEpoch& DataEpochPtrToDataMsg(   const InputDataEpochPtr& data) { return dynamic_cast<const InputDataEpoch&>(*data); }

inline InputDataDebugPtr     DataPtrToDataDebugPtr(   const InputDataPtr&      data) { return std::dynamic_pointer_cast<const InputDataDebug>(data); }
inline const InputDataDebug& DataPtrToDataDebug(      const InputDataPtr&      data) { return dynamic_cast<const InputDataDebug&>(*data); }
inline const InputDataDebug& DataDebugPtrToDataDebug( const InputDataDebugPtr& data) { return dynamic_cast<const InputDataDebug&>(*data); }

inline InputDataEventPtr     DataPtrToDataEventPtr(   const InputDataPtr&      data) { return std::dynamic_pointer_cast<const InputDataEvent>(data); }
inline const InputDataEvent& DataPtrToDataEvent(      const InputDataPtr&      data) { return dynamic_cast<const InputDataEvent&>(*data); }
inline const InputDataEvent& DataEventPtrToDataEvent( const InputDataEventPtr& data) { return dynamic_cast<const InputDataEvent&>(*data); }
// clang-format on

// ---------------------------------------------------------------------------------------------------------------------

using InputDataObserverFn = std::function<void(const InputDataPtr& data)>;
using InputDataObserverId = const void*;

struct InputDataObserver
{
    InputDataObserverId id_;
    InputDataObserverFn fn_;
};

// ---------------------------------------------------------------------------------------------------------------------

class Input;
class InputColl;

using InputPtr = std::shared_ptr<Input>;
using InputPtrMap = std::map<std::string, InputPtr>;

// forward declarations (for receiver.hpp, logfile.hpp, database.hpp)
class Receiver;
class Logfile;
class Database;

class Input
{
   public:
    Input() = delete;
    Input(const std::string& name, const InputType type);
    ~Input();

    std::string               name_;
    InputType                 type_;
    uint64_t                  uid_;
    std::unique_ptr<Receiver> receiver_;
    std::unique_ptr<Logfile>  logfile_;
    std::unique_ptr<Receiver> corr_;
    std::unique_ptr<Database> database_;

    static InputPtr Create(const std::string& name, const InputType type);
    static void Remove(const std::string& name);
    static const InputPtrMap& GetAll();

    bool AddObserver(InputDataObserverId id, InputDataObserverFn fn);
    bool RemoveObserver(InputDataObserverId id);

    inline bool operator<(const Input& rhs) const { return name_ < rhs.name_; }
    inline bool operator==(const Input& rhs) const { return (name_ == rhs.name_); }

    const std::string DebugStr() const;

   private:
    Thread thread_;
    std::mutex queue_mutex_;
    std::queue<InputDataPtr> queue_;
    static constexpr std::size_t MAX_QUEUE = 999;
    static constexpr std::size_t WAIT_QUEUE = MAX_QUEUE - 99;
    std::size_t queueMax_ = 0;
    std::atomic<std::size_t> queueSize_ = 0;
    BinarySemaphore queueSem_;
    std::mutex obs_mutex_;
    std::vector<InputDataObserver> obs_;
    bool _Worker(Thread& thread);
    static InputPtrMap inputs_;
    static uint64_t lastUid_;
    std::unique_ptr<EpochCollector> coll_;
    friend InputColl;
};

class InputColl
{
   public:
    InputColl(Input& input) : input_ { input } { }

    void WaitCollect();

    void CollectData(const InputDataPtr& data);
    bool CollectMsg(const InputDataMsgPtr& data);
    void CollectLog(const LoggingLevel level, const std::string& str);
    void CollectLog(const LoggingLevel level, const char* str);

    void Reset();

   private:
    Input& input_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __INPUT_HPP__
