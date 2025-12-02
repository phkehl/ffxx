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
#include <functional>
//
#include <fpsdk_common/parser.hpp>
using namespace fpsdk::common::parser;
#include <ffxx/utils.hpp>
using namespace ffxx;
//
#include "receiver.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

Receiver::Receiver(const std::string& name, const DataSrcDst srcId) /* clang-format off */ :
    name_     { name },
    srcId_    { srcId },
    worker_   { name, std::bind(&Receiver::_Worker, this, std::placeholders::_1) }  // clang-format on
{
}

Receiver::~Receiver()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool Receiver::Start(const std::string& streamSpec)
{
    if (workerStarted_) {
        Stop();
    }

    streamState_ = StreamState::CLOSED;
    streamType_ = StreamType::UNSPECIFIED;
    streamMode_ = StreamMode::UNSPECIFIED;
    stream_ = Stream::FromSpec(streamSpec);
    if (!stream_) {
        return false;
    }
    streamType_ = stream_->GetType();
    streamMode_ = stream_->GetMode();
    epochColl_.Reset();

    if (worker_.Start()) {
        workerStarted_ = true;
        return true;
    } else {
        return false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void Receiver::Stop()
{
    if (workerStarted_) {
        worker_.Stop();
        stream_.reset();
        workerStarted_ = false;
    }
    streamState_ = StreamState::CLOSED;
    streamType_ = StreamType::UNSPECIFIED;
    streamMode_ = StreamMode::UNSPECIFIED;
}

// ---------------------------------------------------------------------------------------------------------------------

bool Receiver::_Worker(Thread& thread)
{
    if (!stream_) {
        return false;
    }

    DEBUG("Receiver(%s) worker start", name_.c_str());

    stream_->AddStateObserver([&](const StreamState oldState, const StreamState newState, const StreamError error,
                                  const std::string& info) {
        streamState_ = newState;

        std::string logStr = Sprintf("%-15s %s",
            newState != StreamState::ERROR ? StreamStateStr(newState) : StreamErrorStr(error), info.c_str());
        switch (newState) {  // clang-format off
                case StreamState::CONNECTING: CollectDebug(srcId_, LoggingLevel::INFO,   logStr); break;
                case StreamState::CLOSED:     /* FALLTHROUGH */
                case StreamState::CONNECTED:  CollectDebug(srcId_, LoggingLevel::NOTICE, logStr); break;
                case StreamState::ERROR:      CollectDebug(srcId_, LoggingLevel::ERROR,  logStr); break;
            }  // clang-format on

        baudrate_ = stream_->GetBaudrate();

        // if ((newState == StreamState::CONNECTED) && (stream_->GetBaudrate() != 0)) {
        //     NOTICE("new baudrate %u: %s", stream_->GetBaudrate(), stream_->GetOpts().spec_.c_str());
        // }
        if ((oldState != StreamState::CONNECTED) && (newState == StreamState::CONNECTED)) {
            _DetectReceiver();
        }
    });

    BinarySemaphore sem;
    stream_->AddReadObserver([&sem]() { sem.Notify(); });

    if (!stream_->Start()) {
        return false;
    }

    ParserMsg msg;
    while (!thread.ShouldAbort() && (streamState_ != StreamState::CLOSED)) {
        if (streamState_ == StreamState::CONNECTED) {
            while (stream_->Read(msg) /*&& !thread.ShouldAbort()*/) {
                CollectData(std::make_shared<DataMsg>(srcId_, msg, DataMsg::RCVD), epochColl_);
            }
        }
        sem.WaitFor(123);
    }

    stream_->Stop();

    DEBUG("Receiver(%s) worker stop", name_.c_str());

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

void Receiver::_DetectReceiver()
{
    Write(GetCommonMessage(CommonMessage::UBX_MON_VER).data_);
    Write(GetCommonMessage(CommonMessage::FP_B_VERSION).data_);
}

// ---------------------------------------------------------------------------------------------------------------------

bool Receiver::SetBaudrate(const uint32_t baudrate)
{
    if ((streamState_ == StreamState::CONNECTED) && stream_->SetBaudrate(baudrate)) {
        baudrate_ = baudrate;
        return true;
    } else {
        return false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool Receiver::Write(const ParserMsg& msg)
{
    if (Write(msg.data_)) {
        CollectData(std::make_shared<DataMsg>(srcId_, msg, DataMsg::SENT));
        return true;
    } else {
        return false;
    }
}

bool Receiver::Write(const std::vector<uint8_t>& data)
{
    if ((streamState_ == StreamState::CONNECTED) && (streamMode_ != StreamMode::RO) && stream_->Write(data)) {
        return true;
    } else {
        return false;
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
