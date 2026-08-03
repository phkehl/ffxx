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
 * @brief Stream nope
 */

/* LIBC/STL */
#include <cmath>

/* EXTERNAL */
#include <fpsdk_common/logging.hpp>

/* PACKAGE */
#include "nope.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

/*static*/ std::unique_ptr<StreamOptsNope> StreamOptsNope::FromPath(
    const std::string& path, std::vector<std::string>& errors)
{
    StreamOptsNope opts;

    bool ok = true;

    if (!path.empty()) {
        errors.push_back("bad spec");
        ok = false;
    }

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsNope>(opts);
}

/* ****************************************************************************************************************** */

StreamNope::StreamNope(const StreamOptsNope& opts) /* clang-format off */ :
    StreamBase(opts),
    opts_   { opts }  // clang-format on
{
    b_opts_ = &opts_;
    SetStateClosed();
    STREAM_TRACE_WARNING();
    dummy_.resize(write_queue_.Size(), 0);
}

StreamNope::~StreamNope()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamNope::Start()
{
    STREAM_TRACE("Start");
    if (started_) {
        return false;
    }
    started_ = true;
    SetStateConnected();
    return true;
}

void StreamNope::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    UNUSED(timeout);
    if (started_) {
        SetStateClosed();
        started_ = false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamNope::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX, size);
    return write_queue_.Read(dummy_.data(), std::min(size, dummy_.size()));  // should always return true...
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
