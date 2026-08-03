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
#ifndef __FFXX_STREAM_NOPE_HPP__
#define __FFXX_STREAM_NOPE_HPP__

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
struct StreamOptsNope : public StreamOpts  // clang-format off
{
    static std::unique_ptr<StreamOptsNope> FromPath(const std::string& path, std::vector<std::string>& errors);
};  // clang-format on

// loop stream implementation
class StreamNope : public StreamBase
{
   public:
    StreamNope(const StreamOptsNope& opts);
    ~StreamNope();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   private:
    StreamOptsNope opts_;
    bool started_ = false;
    std::vector<uint8_t> dummy_;
    bool ProcessWrite(const std::size_t size) final;
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_NOPE_HPP__
