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

#ifndef __RECEIVER_HPP__
#define __RECEIVER_HPP__

//
#include "ffgui_inc.hpp"
//
#include <atomic>
//
#include <ffxx/epoch.hpp>
#include <ffxx/stream.hpp>
using namespace ffxx;
#include <fpsdk_common/thread.hpp>
using namespace fpsdk::common::thread;
//

namespace ffgui {
/* ****************************************************************************************************************** */

class Receiver
{
   public:
    Receiver(const std::string& name, const DataSrcDst srcId);
    ~Receiver();

    bool Start(const std::string& streamSpec);
    void Stop();

    // clang-format off
    StreamState  GetState()    const { return streamState_; }
    StreamType   GetType()     const { return streamType_; }
    StreamMode   GetMode()     const { return streamMode_; }
    uint32_t     GetBaudrate() const { return baudrate_; }
    StreamPtr&   GetStream()         { return stream_; }
    // clang-format on
    bool SetBaudrate(const uint32_t baudrate);
    bool Write(const ParserMsg& msg);
    bool Write(const std::vector<uint8_t>& data);

   private:
    std::string name_;
    const DataSrcDst srcId_ = nullptr;
    StreamPtr stream_;
    std::atomic<StreamState> streamState_ = StreamState::CLOSED;
    StreamType streamType_ = StreamType::UNSPECIFIED;
    StreamMode streamMode_ = StreamMode::UNSPECIFIED;
    Thread worker_;
    bool workerStarted_ = false;
    bool _Worker(Thread& thread);
    std::atomic<uint32_t> baudrate_ = 0;
    EpochCollector epochColl_;

    void _DetectReceiver();
};

using ReceiverPtr = std::shared_ptr<Receiver>;

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __RECEIVER_HPP__
