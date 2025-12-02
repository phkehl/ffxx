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

/* LIBC/STL */
#include <cmath>

/* EXTERNAL */
#include <fpsdk_common/logging.hpp>

/* PACKAGE */
#include "loop.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

/*static*/ std::unique_ptr<StreamOptsLoop> StreamOptsLoop::FromPath(
    const std::string& path, std::vector<std::string>& errors)
{
    StreamOptsLoop opts;

    bool ok = true;

    // "[<delay>][:<rate>]"
    const auto parts = string::StrSplit(path, ":");

    if (parts.size() <= 2) {
        if (parts.size() >= 1) {
            if (!parts[0].empty() &&
                (!string::StrToValue(parts[0], opts.delay_) || (opts.delay_ < 0.0) || (opts.delay_ > opts.DELAY_MAX))) {
                errors.push_back("bad <delay>");
                ok = false;
            }
        }
        if (parts.size() >= 2) {
            if (!parts[1].empty() && !string::StrToValue(parts[1], opts.rate_)) {
                errors.push_back("bad <rate>");
                ok = false;
            }
        }

    } else {
        ok = false;
    }

    opts.path_ = string::Sprintf("%.3f:%" PRIu32, opts.delay_, opts.rate_);

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsLoop>(opts);
}

/* ****************************************************************************************************************** */

StreamLoop::StreamLoop(const StreamOptsLoop& opts) /* clang-format off */ :
    StreamBase(opts, opts_),
    opts_    { opts },
    delay_   { static_cast<int64_t>(std::floor(opts_.delay_ * 1e3)) },
    thread_  { opts_.name_, std::bind(&StreamLoop::Worker, this) }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();
}

StreamLoop::~StreamLoop()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamLoop::Start()
{
    STREAM_TRACE("Start");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }
    return thread_.Start();
}

void StreamLoop::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    UNUSED(timeout);
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamLoop::Worker()
{
    STREAM_TRACE("Worker");
    SetStateConnecting();
    SetStateConnected();

    const double sleep_per_byte = (opts_.rate_ > 0 ? 1.0 / (double)opts_.rate_ : 0.0);

    while (!thread_.ShouldAbort()) {
        // Next chunk time and size
        Chunk next;
        std::chrono::duration<double> dt;
        const auto& now = boost::asio::steady_timer::clock_type::now();
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (!queue_.empty()) {
                next = queue_.front();
                dt = next.tp_ - now;
            }
        }

        if (next.sz_ > 0) {
            STREAM_TRACE("Worker next sz %" PRIuMAX " dt %.3f", next.sz_, dt.count());

            // Delay
            if (dt.count() > 0.0) {
                if (thread_.Sleep(std::round(dt.count() * 1e3)) != thread::WaitRes::TIMEOUT) {
                    continue;
                }
            }

            // Write back
            double sleep_dur = 0.0;
            while (next.sz_ > 0) {
                const auto size = (opts_.rate_ > 0 ? 1 : std::min(next.sz_, sizeof(buf_)));
                next.sz_ -= size;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    write_queue_.Read(buf_, size);
                }
                sleep_dur += sleep_per_byte;
                STREAM_TRACE("Worker write %" PRIuMAX " rem %" PRIuMAX, size, next.sz_);
                ProcessRead(buf_, size);
                if (sleep_dur >= 0.01) {
                    if (thread_.Sleep(std::round(sleep_dur * 1e3)) != thread::WaitRes::TIMEOUT) {
                        break;
                    }
                    sleep_dur = 0.0;
                }
            }

            {
                std::unique_lock<std::mutex> lock(mutex_);
                queue_.pop_front();
            }

        } else {
            STREAM_TRACE("Worker idle");
            wait_.WaitFor(337);
        }
    }

    SetStateClosed();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamLoop::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_ongoing_));

    if (queue_.size() >= MAX_QUEUE) {
        return false;
    }

    const auto now = boost::asio::steady_timer::clock_type::now();
    queue_.push_back({ now + delay_, size });
    wait_.Notify();

    return true;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
