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
 * @brief Stream GGA and STA
 */
#ifndef __FFXX_STREAM_GGA_HPP__
#define __FFXX_STREAM_GGA_HPP__

/* LIBC/STL */
#include <cstdint>

/* EXTERNAL */
#include <fpsdk_common/parser/rtcm3.hpp>
#include <fpsdk_common/parser/types.hpp>
#include <fpsdk_common/thread.hpp>

/* PACKAGE */
#include "base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

// GGA stream options
struct StreamOptsGga : public StreamOpts  // clang-format off
{
    static std::unique_ptr<StreamOptsGga> FromPath(const std::string& path, std::vector<std::string>& errors);

    double      lat_    = 0.0;   // Latitude [deg]
    double      lon_    = 0.0;   // Longitude [deg]
    double      height_ = 0.0;   // Height [m]
    uint32_t    period_ = 5000;  // Period [ms]
    std::string talker_ = "GN";  // Talker ID
};  // clang-format on

// GGA stream implementation
class StreamGga : public StreamBase
{
   public:
    StreamGga(const StreamOptsGga& opts);
    ~StreamGga();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   private:
    StreamOptsGga opts_;
    std::string nmea_;  // NMEA message
    thread::Thread thread_;
    bool Worker();
    bool ProcessWrite(const std::size_t size) final;
};

// ---------------------------------------------------------------------------------------------------------------------

// STA stream options
struct StreamOptsSta : public StreamOpts  // clang-format off
{
    static std::unique_ptr<StreamOptsSta> FromPath(const std::string& path, std::vector<std::string>& errors);

    double      ecef_x_   = 0.0;   // ECEF X coordinate [deg]
    double      ecef_y_   = 0.0;   // ECEF Y coordinate [deg]
    double      ecef_z_   = 0.0;   // ECEF Z coordinate [deg]
    uint32_t    period_   = 5000;  // Period [ms]
    uint16_t    sta_id_   = 0;     // Station ID
    uint16_t    type_     = parser::rtcm3::RTCM3_TYPE1005_MSGID;  // Message type
};  // clang-format on

// STA stream implementation
class StreamSta : public StreamBase
{
   public:
    StreamSta(const StreamOptsSta& opts);
    ~StreamSta();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   private:
    StreamOptsSta opts_;
    uint8_t msg_[parser::MAX_RTCM3_SIZE];  // RTCM3 message
    std::size_t size_;                     // Message size
    thread::Thread thread_;
    bool Worker();
    bool ProcessWrite(const std::size_t size) final;
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_GGA_HPP__
