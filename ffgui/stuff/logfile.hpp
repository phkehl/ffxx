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

#ifndef _logfile__HPP__
#define _logfile__HPP__

//
#include "ffgui_inc.hpp"
//
#include <iostream>
#include <atomic>
//
#include <ffxx/epoch.hpp>
using namespace ffxx;
#include <fpsdk_common/path.hpp>
#include <fpsdk_common/parser.hpp>
#include <fpsdk_common/thread.hpp>
using namespace fpsdk::common::path;
using namespace fpsdk::common::parser;
using namespace fpsdk::common::thread;
//
#include "input.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class Logfile
{
   public:
    Logfile(const std::string& name, InputColl&& coll);
    ~Logfile();

    bool Open(const std::string& path);
    void Close();
    void Play();
    void Stop();
    void Pause();
    void StepMsg(const std::string &msgName = "");
    void StepEpoch();

    bool CanOpen() const;
    bool CanClose() const;
    bool CanPlay() const;
    bool CanStop() const;
    bool CanPause() const;
    bool CanStep() const;
    bool CanSeek() const;

    const char *StateStr() const;

    float GetRelPos() const; // 0.0 ... 1.0
    void SetRelPos(const float pos); // 0.0 ... 1.0 (= 0 ... 100%)

    enum class SpeedMode : int { MSG, EPOCH, NOLIMIT };
    static constexpr float SPEED_MIN =    0.1f;
    static constexpr float SPEED_MAX = 5000.0f;
    void SetSpeed(const SpeedMode mode, const float value);

    void Clear();

   private:
    std::string name_;
    InputColl coll_;
    Thread worker_;
    bool workerStarted_ = false;
    bool _Worker(Thread& thread);

    enum class State { CLOSED, STOPPED, PLAYING, PAUSED };
    std::atomic<State> state_ = State::CLOSED;
    std::unique_ptr<InputFile> logfile_;
    std::atomic<uint64_t> pos_ = 0;
    std::atomic<uint64_t> size_ = 0;
    std::atomic<float> posRel_ = 0.0f;
    std::atomic<SpeedMode> speedMode_ = SpeedMode::EPOCH;
    std::atomic<float> speedValue_ = 1.0f;
    Parser parser_;
    std::array<uint8_t, MAX_ADD_SIZE> buf_;
    std::atomic<bool> stepEpoch_ = false;
    std::atomic<bool> stepMsg_ = false;
    std::string stepMsgName_;
    std::atomic<bool> atEnd_ = false;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // _logfile__HPP__
