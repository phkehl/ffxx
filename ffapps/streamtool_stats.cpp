/**
 * \verbatim
 * flipflip's c++ apps (ffapps)
 *
 * Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
 * https://oinkzwurgl.org/projaeggd/ffxx/
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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
 * @brief streamtool message analysis and statistics
 */

/* LIBC/STL */

/* EXTERNAL */
#include <ffxx/utils.hpp>
#include <fpsdk_common/gnss.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/parser/fpa.hpp>
#include <fpsdk_common/parser/rtcm3.hpp>
#include <fpsdk_common/parser/ubx.hpp>
#include <fpsdk_common/string.hpp>

/* PACKAGE */
#include "streamtool_stats.hpp"

namespace ffapps::streamtool {
/* ****************************************************************************************************************** */

using namespace ffxx;
using namespace fpsdk::common::gnss;
using namespace fpsdk::common::parser;
using namespace fpsdk::common::parser::fpa;
using namespace fpsdk::common::parser::ubx;
using namespace fpsdk::common::parser::rtcm3;
using namespace fpsdk::common::time;
using namespace fpsdk::common::string;

// ---------------------------------------------------------------------------------------------------------------------

Stats::Stats()
{
}

// ---------------------------------------------------------------------------------------------------------------------

Stats::MsgInfo Stats::Update(const fpsdk::common::parser::ParserMsg& msg, const fpsdk::common::time::Time& recv_ts)
{
    // Get message info
    MsgInfo info;
    info.offs_ = offs_;
    offs_ += msg.data_.size();
    info.recv_ts_ = recv_ts;
    info.data_ts_ = GetMessageDataTime(msg, recv_ts);

    switch (msg.proto_) {
        case Protocol::FP_A:
            if (msg.name_ == FpaEoePayload::MSG_NAME) {
                FpaEoePayload eoe;
                if (eoe.SetFromMsg(msg.data_.data(), msg.data_.size())) {
                    info.unique_name_ = msg.name_ + "_" + FpaEpochStr(eoe.epoch);
                }
            } else if ((msg.name_ == FpaTfPayload::MSG_NAME)) {
                FpaTfPayload tf;
                if (tf.SetFromMsg(msg.data_.data(), msg.data_.size())) {
                    info.unique_name_ = msg.name_ + "_" + tf.frame_a + "_" + tf.frame_b;
                }
            }
            break;
        case Protocol::UBX:
            // No payload messages are poll messages
            // @todo we could do more classification...
            if (msg.data_.size() == UBX_FRAME_SIZE) {
                info.unique_name_ = msg.name_ + "_POLL";
            }
            switch (UbxClsId(msg.data_.data())) {
                case UBX_RXM_CLSID:
                    switch (UbxMsgId(msg.data_.data())) {
                        case UBX_RXM_SFRBX_MSGID:
                            if ((msg.data_.size() >= UBX_RXM_SFRBX_V2_MIN_SIZE)) {
                                UBX_RXM_SFRBX_V2_GROUP0 i;
                                std::memcpy(&i, msg.data_.data() + UBX_HEAD_SIZE, sizeof(i));
                                const auto sat = UbxGnssIdSvIdToSat(i.gnssId, i.svId);
                                const auto sig = UbxGnssIdSigIdToSignal(i.gnssId, i.sigId);
                                info.unique_name_ = msg.name_ + "_" + sat.GetStr() + "_" + SignalStr(sig, true);
                            }
                            break;
                    }
                    break;
            }
            break;
        case Protocol::FP_B:
        case Protocol::NMEA:
        case Protocol::RTCM3:
        case Protocol::UNI_B:
        case Protocol::NOV_B:
        case Protocol::SBF:
        case Protocol::QGC:
        case Protocol::SPARTN:
        case Protocol::OTHER:
            break;
    }

    if (info.unique_name_.empty()) {
        info.unique_name_ = msg.name_;
    }

    // Get state info for this (unique) message
    auto entry = msg_stats_.find(info.unique_name_);
    if (entry == msg_stats_.end()) {
        entry = msg_stats_.emplace(info.unique_name_, MsgStats()).first;
        entry->second.protocol_name_ = ProtocolStr(msg.proto_);
        entry->second.message_name_ = msg.name_;
        entry->second.unique_name_ = info.unique_name_;
        if (msg.proto_ == Protocol::RTCM3) {
            entry->second.msg_desc_ = Rtcm3GetTypeDesc(Rtcm3Type(msg.data_.data()));
        }
    }
    MsgStats& stats = entry->second;

    // Statistics
    stats.count_++;
    stats.bytes_ += msg.data_.size();

    UpdateEx(info, stats);

    return info;
}

// ---------------------------------------------------------------------------------------------------------------------

Stats::MsgInfo Stats::Update(const ffxx::Epoch& epoch, const fpsdk::common::time::Time& recv_ts)
{
    // Get message info
    MsgInfo info;
    info.recv_ts_ = recv_ts;
    info.data_ts_ = epoch.time;
    static constexpr const char* unique_name = "EPOCH";

    if (info.unique_name_.empty()) {
        info.unique_name_ = unique_name;
    }

    // Get state info for this (unique) message
    auto entry = msg_stats_.find(info.unique_name_);
    if (entry == msg_stats_.end()) {
        entry = msg_stats_.emplace(info.unique_name_, MsgStats()).first;
        entry->second.protocol_name_ = "-";
        entry->second.message_name_ = "-";
        entry->second.unique_name_ = unique_name;
    }
    MsgStats& stats = entry->second;

    // Statistics
    stats.count_++;

    UpdateEx(info, stats);

    return info;
}

// ---------------------------------------------------------------------------------------------------------------------

void Stats::UpdateEx(MsgInfo& info, MsgStats& stats)
{
    // Latency and interval determined from data timestamp
    if (!info.data_ts_.IsZero()) {
        info.latency_ = (info.recv_ts_ - info.data_ts_);
        if (!stats.last_data_ts_.IsZero()) {
            info.interval_ = (info.data_ts_ - stats.last_data_ts_);
        }
    }
    // For interval we can fall back to received timestamps
    else {
        if (!stats.last_recv_ts_.IsZero()) {
            info.interval_ = (info.recv_ts_ - stats.last_recv_ts_);
        }
    }
    stats.last_data_ts_ = info.data_ts_;  // can be zero
    stats.last_recv_ts_ = info.recv_ts_;

    if (!info.interval_.IsZero()) {
        const double interval = info.interval_.GetSec();
        if (!((interval < Stats::MsgStats::SANITY_INTERVAL_MIN) || (interval > Stats::MsgStats::SANITY_INTERVAL_MAX))) {
            stats.hist_interval_(interval);
            stats.acc_interval_(interval);
        }
        if (interval > Stats::MsgStats::SANITY_INTERVAL_MIN) {
            const double frequency = 1.0 / interval;
            if (!((frequency < Stats::MsgStats::SANITY_FREQUENCY_MIN) ||
                    (frequency > Stats::MsgStats::SANITY_FREQUENCY_MAX))) {
                stats.hist_frequency_(frequency);
                stats.acc_frequency_(frequency);
            }
        }
    }
    if (!info.latency_.IsZero()) {
        const double latency = info.latency_.GetSec();
        if (!((latency < Stats::MsgStats::SANITY_LATENCY_MIN) || (latency > Stats::MsgStats::SANITY_LATENCY_MAX))) {
            stats.hist_latency_(latency);
            stats.acc_latency_(latency);
        }
    }

}


/* ****************************************************************************************************************** */
}  // namespace ffapps::streamtool
