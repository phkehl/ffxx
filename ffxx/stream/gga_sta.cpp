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

/* LIBC/STL */
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>

/* EXTERNAL */
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/parser/crc.hpp>
#include <fpsdk_common/parser/nmea.hpp>
#include <fpsdk_common/string.hpp>
#include <fpsdk_common/time.hpp>

/* PACKAGE */
#include "gga_sta.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

/*static*/ std::unique_ptr<StreamOptsGga> StreamOptsGga::FromPath(
    const std::string& path, std::vector<std::string>& errors)
{
    StreamOptsGga opts;
    bool ok = true;

    double lat = 0.0;
    double lon = 0.0;
    double height = 0.0;

    // "<lat>/<lon>/<height>[[:<interval>]:<talker>]"
    const auto parts = string::StrSplit(path, ":");
    if ((parts.size() >= 1) && (parts.size() <= 3)) {
        const auto llh = string::StrSplit(parts[0], "/");
        if ((llh.size() == 3) && string::StrToValue(llh[0], lat) && string::StrToValue(llh[1], lon) &&
            string::StrToValue(llh[2], height) && !(lat < StreamOpts::GGA_LAT_MIN) &&
            !(lat > StreamOpts::GGA_LAT_MAX) && !(lon < StreamOpts::GGA_LON_MIN) && !(lon > StreamOpts::GGA_LON_MAX) &&
            !(height < StreamOpts::GGA_HEIGHT_MIN) && !(height > StreamOpts::GGA_HEIGHT_MAX)) {
            opts.lat_ = lat;
            opts.lon_ = lon;
            opts.height_ = height;
        } else {
            errors.push_back("bad <lat>/<lon>/<height>");
            ok = false;
        }
        if (parts.size() >= 2) {
            double period = 0.0;
            if (string::StrToValue(parts[1], period) &&
                !((period < StreamOpts::GGA_PERIOD_MIN) || (period > StreamOpts::GGA_PERIOD_MAX))) {
                opts.period_ = static_cast<uint32_t>(std::round(period * 1e3));
            } else {
                errors.push_back("bad <period>");
                ok = false;
            }
        }
        if (parts.size() >= 3) {
            if (parts[2].size() == 2) {
                opts.talker_ = parts[2];
            } else {
                errors.push_back("bad <talker>");
                ok = false;
            }
        }
    } else {
        ok = false;
    }

    opts.path_ = string::Sprintf("%.8f/%.8f/%.1f:%.1f:%s", opts.lat_, opts.lon_, opts.height_,
        (double)opts.period_ * 1e-3, opts.talker_.c_str());

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsGga>(opts);
}

// ---------------------------------------------------------------------------------------------------------------------

StreamGga::StreamGga(const StreamOptsGga& opts) /* clang-format off */ :
    StreamBase(opts, opts_),
    opts_     { opts },
    thread_   { opts_.name_, std::bind(&StreamGga::Worker, this) }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();

    const parser::nmea::NmeaCoordinates clat(opts.lat_);
    const parser::nmea::NmeaCoordinates clon(opts.lon_);
    nmea_ = string::Sprintf("$%sGGA,000000.00,%02d%08.5f,%c,%03d%08.5f,%c,1,10,2.00,%.1f,M,0.0,M,,*00\r\n",
        opts.talker_.c_str(), clat.deg_, clat.min_, clat.sign_ ? 'N' : 'S', clon.deg_, clon.min_,
        clon.sign_ ? 'E' : 'W', opts.height_);
}

StreamGga::~StreamGga()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamGga::Start()
{
    STREAM_TRACE("Start");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }
    return thread_.Start();
}

void StreamGga::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    UNUSED(timeout);
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamGga::Worker()
{
    STREAM_TRACE("Worker");
    SetStateConnecting();
    SetStateConnected();
    while (!thread_.ShouldAbort()) {
        if (thread_.SleepUntil(opts_.period_, 0) == thread::WaitRes::TIMEOUT) {
            // We have this message template:
            //            1         2         3         4         5
            //  0123456789012345678901234567890123456789012345678901234...variable...
            // "$GPGGA,000000.00,4630.00000,N,00818.00000,E,1,10,2.00,100.0,M,0.0,M,,*00\r\n"
            //         hhmmss.ss                                                      ck
            uint8_t msg[100];
            const std::size_t size = std::min(nmea_.size(), sizeof(msg));
            std::memcpy(msg, nmea_.data(), size);

            // Update time
            const auto now = time::Time::FromClockRealtime().GetUtcTime(2);
            const int isec = static_cast<int>(std::floor(now.sec_));
            const int fsec = static_cast<int>(std::floor((now.sec_ - isec) * 100.0));
            // clang-format off
            msg[ 7] = '0' + (now.hour_ / 10); // h
            msg[ 8] = '0' + (now.hour_ % 10); // h
            msg[ 9] = '0' + (now.min_  / 10); // m
            msg[10] = '0' + (now.min_  % 10); // m
            msg[11] = '0' + (isec      / 10); // s
            msg[12] = '0' + (isec      % 10); // s
            msg[14] = '0' + (fsec      / 10); // s
            msg[15] = '0' + (fsec      % 10); // s
            // clang-format on

            // Update checksum
            uint8_t ck = 0;
            for (std::size_t ix = 1; ix < (size - 5); ix++) {
                ck ^= msg[ix];
            }
            const uint8_t c = (ck >> 4) & 0x0f;
            const uint8_t k = ck & 0x0f;
            // const uint8_t hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
            msg[size - 4] = (c < 10 ? '0' + c : 'A' + (c - 10));
            msg[size - 3] = (k < 10 ? '0' + k : 'A' + (k - 10));

            ProcessRead(msg, size);
        }
    }
    SetStateClosed();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamGga::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_ongoing_));
    UNUSED(size);
    return false;
}

/* ****************************************************************************************************************** */

/*static*/ std::unique_ptr<StreamOptsSta> StreamOptsSta::FromPath(
    const std::string& path, std::vector<std::string>& errors)
{
    StreamOptsSta opts;

    bool ok = true;

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    // "<ecef_x>/<ecef_y>/<ecef_z>[[[:<interval>]:<sta>]:<type>]"
    const auto parts = string::StrSplit(path, ":");
    if ((parts.size() >= 1) && (parts.size() <= 4)) {
        const auto xyz = string::StrSplit(parts[0], "/");
        if ((xyz.size() == 3) && string::StrToValue(xyz[0], x) && string::StrToValue(xyz[1], y) &&
            string::StrToValue(xyz[2], z)) {
            opts.ecef_x_ = x;
            opts.ecef_y_ = y;
            opts.ecef_z_ = z;
        } else {
            errors.push_back("bad <ecef_x>/<ecef_y>/<ecef_z>");
            ok = false;
        }
        if (parts.size() >= 2) {
            double period = 0.0;
            if (string::StrToValue(parts[1], period) && !((period < 1.0) || (period > 86400.0))) {
                opts.period_ = static_cast<uint32_t>(std::round(period * 1e3));
            } else {
                errors.push_back("bad <period>");
                ok = false;
            }
        }
        if (parts.size() >= 3) {
            uint16_t sta = 0;
            if (string::StrToValue(parts[2], sta) && (sta < 1024)) {
                opts.sta_id_ = sta;
            } else {
                errors.push_back("bad <sta>");
                ok = false;
            }
        }
        if (parts.size() >= 4) {
            uint16_t type = 0;
            if (string::StrToValue(parts[3], type) &&
                ((type == parser::rtcm3::RTCM3_TYPE1005_MSGID) || (type == parser::rtcm3::RTCM3_TYPE1006_MSGID) ||
                    (type == parser::rtcm3::RTCM3_TYPE1032_MSGID))) {
                opts.type_ = type;
            } else {
                errors.push_back("bad <type>");
                ok = false;
            }
        }
    } else {
        ok = false;
    }

    opts.path_ = string::Sprintf("%.2f/%.2f/%.2f:%.1f:%d:%d", opts.ecef_x_, opts.ecef_y_, opts.ecef_z_,
        (double)opts.period_ * 1e-3, opts.sta_id_, opts.type_);

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsSta>(opts);
}

// ---------------------------------------------------------------------------------------------------------------------

StreamSta::StreamSta(const StreamOptsSta& opts) /* clang-format off */ :
    StreamBase(opts, opts_),
    opts_     { opts },
    thread_   { opts_.name_, std::bind(&StreamSta::Worker, this) }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();

    using namespace fpsdk::common::parser::rtcm3;

    std::memset(msg_, 0x00, sizeof(msg_));
    msg_[0] = RTCM3_PREAMBLE;

    uint8_t* payload = &msg_[RTCM3_HEAD_SIZE];
    uint16_t payload_size = 0;

    Rtcm3SetUnsigned(payload, 0, 12, opts_.type_);
    Rtcm3SetUnsigned(payload, 12, 12, opts_.sta_id_);

    switch (opts_.type_) {
        case RTCM3_TYPE1006_MSGID:
            payload_size = 168 / 8;
            /* FALLTHROUGH */
        case RTCM3_TYPE1005_MSGID:
            payload_size = 152 / 8;
            Rtcm3SetSigned(payload, 34, 38, std::round(opts_.ecef_x_ * 1e4));
            Rtcm3SetSigned(payload, 74, 38, std::round(opts_.ecef_y_ * 1e4));
            Rtcm3SetSigned(payload, 114, 38, std::round(opts_.ecef_z_ * 1e4));
            Rtcm3SetUnsigned(payload, 30, 1, 1);
            Rtcm3SetUnsigned(payload, 31, 1, 1);
            Rtcm3SetUnsigned(payload, 32, 1, 1);
            Rtcm3SetUnsigned(payload, 72, 1, 1);
            break;
        case RTCM3_TYPE1032_MSGID:
            payload_size = 156 / 8;
            Rtcm3SetSigned(payload, 42, 38, std::round(opts_.ecef_x_ * 1e4));
            Rtcm3SetSigned(payload, 80, 38, std::round(opts_.ecef_y_ * 1e4));
            Rtcm3SetSigned(payload, 118, 38, std::round(opts_.ecef_z_ * 1e4));
            break;
    }

    Rtcm3SetUnsigned(msg_, 12, 12, payload_size);

    size_ = payload_size + RTCM3_FRAME_SIZE;
    uint32_t crc = parser::crc::Crc24rtcm3(msg_, size_ - 3);
    msg_[size_ - 1] = crc & 0xff;
    crc >>= 8;
    msg_[size_ - 2] = crc & 0xff;
    crc >>= 8;
    msg_[size_ - 3] = crc & 0xff;
}

StreamSta::~StreamSta()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamSta::Start()
{
    STREAM_TRACE("Start");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }
    return thread_.Start();
}

void StreamSta::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    UNUSED(timeout);
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamSta::Worker()
{
    STREAM_TRACE("Worker");
    SetStateConnecting();
    SetStateConnected();
    while (!thread_.ShouldAbort()) {
        if (thread_.SleepUntil(opts_.period_, 0) == thread::WaitRes::TIMEOUT) {
            ProcessRead(msg_, size_);
        }
    }
    SetStateClosed();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamSta::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_ongoing_));
    UNUSED(size);
    return false;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
