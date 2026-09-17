/**
 * \verbatim
 * flipflip's c++ library (ffxx)
 *
 * Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
 * https://oinkzwurgl.org/projaeggd/ffxx/
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 * \endverbatim
 *
 * @file
 * @brief Stream loopback
 */
#ifndef __FFXX_STREAM_LOOP_HPP__
#define __FFXX_STREAM_LOOP_HPP__

/* LIBC/STL */
#include <array>
#include <cstdint>
#include <deque>

/* EXTERNAL */
#include <fpsdk_common/thread.hpp>

/* PACKAGE */
#include "base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

// loop stream options
struct StreamOptsLoop : public StreamOpts  // clang-format off
{
    static std::unique_ptr<StreamOptsLoop> FromPath(const std::string& path, std::vector<std::string>& errors);

    double delay_ = 0.0;
    uint32_t rate_ = 0;

    static constexpr double DELAY_MAX = 60.0;
};  // clang-format on

// loop stream implementation
class StreamLoop : public StreamBase
{
   public:
    StreamLoop(const StreamOptsLoop& opts);
    ~StreamLoop();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   private:
    StreamOptsLoop opts_;
    std::chrono::milliseconds delay_;
    uint8_t buf_[parser::MAX_ADD_SIZE];
    thread::Thread thread_;
    thread::BinarySemaphore wait_;
    bool Worker();

    struct Chunk
    {
        boost::asio::chrono::steady_clock::time_point tp_;
        std::size_t sz_ = 0;
    };
    std::deque<Chunk> queue_;
    static constexpr std::size_t MAX_QUEUE = 100000;

    void DoWrite(const boost::system::error_code& ec);

    bool ProcessWrite(const std::size_t size) final;
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_LOOP_HPP__
