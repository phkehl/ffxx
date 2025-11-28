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
    switch (msg.proto_) {  // clang-format off
        case Protocol::FP_A:   AnalyseFpa(msg, info);    break;
        case Protocol::FP_B:                             break;
        case Protocol::NMEA:                             break;
        case Protocol::UBX:    AnalyseUbx(msg, info);    break;
        case Protocol::RTCM3:  AnalyseRtcm3(msg, info);  break;
        case Protocol::UNI_B:                            break;
        case Protocol::NOV_B:                            break;
        case Protocol::SBF:                              break;
        case Protocol::SPARTN:                           break;
        case Protocol::OTHER:                            break;
    }  // clang-format on

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
    MsgStats& state = entry->second;

    // Latency and interval determined from data timestamp
    if (!info.data_ts_.IsZero()) {
        info.latency_ = (info.recv_ts_ - info.data_ts_);
        if (!state.last_data_ts_.IsZero()) {
            info.interval_ = (info.data_ts_ - state.last_data_ts_);
        }
    }
    // For interval we can fall back to received timestamps
    else {
        if (!state.last_recv_ts_.IsZero()) {
            info.interval_ = (info.recv_ts_ - state.last_recv_ts_);
        }
    }
    state.last_data_ts_ = info.data_ts_;  // can be zero
    state.last_recv_ts_ = info.recv_ts_;

    // Statistics
    state.count_++;
    state.bytes_ += msg.data_.size();
    if (!info.interval_.IsZero()) {
        const double interval = info.interval_.GetSec();
        if (!((interval < Stats::MsgStats::SANITY_INTERVAL_MIN) || (interval > Stats::MsgStats::SANITY_INTERVAL_MAX))) {
            state.hist_interval_(interval);
            state.acc_interval_(interval);
        }
        if (interval > Stats::MsgStats::SANITY_INTERVAL_MIN) {
            const double frequency = 1.0 / interval;
            if (!((frequency < Stats::MsgStats::SANITY_FREQUENCY_MIN) ||
                    (frequency > Stats::MsgStats::SANITY_FREQUENCY_MAX))) {
                state.hist_frequency_(frequency);
                state.acc_frequency_(frequency);
            }
        }
    }
    if (!info.latency_.IsZero()) {
        const double latency = info.latency_.GetSec();
        if (!((latency < Stats::MsgStats::SANITY_LATENCY_MIN) || (latency > Stats::MsgStats::SANITY_LATENCY_MAX))) {
            state.hist_latency_(latency);
            state.acc_latency_(latency);
        }
    }

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
    MsgStats& state = entry->second;

    // Latency and interval determined from data timestamp
    if (!info.data_ts_.IsZero()) {
        info.latency_ = (info.recv_ts_ - info.data_ts_);
        if (!state.last_data_ts_.IsZero()) {
            info.interval_ = (info.data_ts_ - state.last_data_ts_);
        }
    }
    // For interval we can fall back to received timestamps
    else {
        if (!state.last_recv_ts_.IsZero()) {
            info.interval_ = (info.recv_ts_ - state.last_recv_ts_);
        }
    }
    state.last_data_ts_ = info.data_ts_;  // can be zero
    state.last_recv_ts_ = info.recv_ts_;

    // Statistics
    state.count_++;
    if (!info.interval_.IsZero()) {
        const double interval = info.interval_.GetSec();
        if (!((interval < Stats::MsgStats::SANITY_INTERVAL_MIN) || (interval > Stats::MsgStats::SANITY_INTERVAL_MAX))) {
            state.hist_interval_(interval);
            state.acc_interval_(interval);
        }
        if (interval > Stats::MsgStats::SANITY_INTERVAL_MIN) {
            const double frequency = 1.0 / interval;
            if (!((frequency < Stats::MsgStats::SANITY_FREQUENCY_MIN) ||
                    (frequency > Stats::MsgStats::SANITY_FREQUENCY_MAX))) {
                state.hist_frequency_(frequency);
                state.acc_frequency_(frequency);
            }
        }
    }
    if (!info.latency_.IsZero()) {
        const double latency = info.latency_.GetSec();
        if (!((latency < Stats::MsgStats::SANITY_LATENCY_MIN) || (latency > Stats::MsgStats::SANITY_LATENCY_MAX))) {
            state.hist_latency_(latency);
            state.acc_latency_(latency);
        }
    }

    return info;
}

// ---------------------------------------------------------------------------------------------------------------------

inline void SetDataTsFromGpsTime(Time& data_ts, const FpaGpsTime& gps_time)
{
    if (gps_time.week.valid && gps_time.tow.valid) {
        data_ts.SetWnoTow({ gps_time.week.value, gps_time.tow.value, WnoTow::Sys::GPS });
    }
}

void Stats::AnalyseFpa(const fpsdk::common::parser::ParserMsg& msg, MsgInfo& info)
{
    if (msg.name_ == FpaEoePayload::MSG_NAME) {
        FpaEoePayload eoe;
        if (eoe.SetFromMsg(msg.data_.data(), msg.data_.size())) {
            info.unique_name_ = msg.name_ + "_" + FpaEpochStr(eoe.epoch);
            SetDataTsFromGpsTime(info.data_ts_, eoe.gps_time);
        }
    } else if ((msg.name_ == FpaOdometryPayload::MSG_NAME)) {
        FpaOdometryPayload odo;
        if (odo.SetFromMsg(msg.data_.data(), msg.data_.size())) {
            info.unique_name_ = msg.name_;
            SetDataTsFromGpsTime(info.data_ts_, odo.gps_time);
        }
    } else if ((msg.name_ == FpaOdomshPayload::MSG_NAME)) {
        FpaOdomshPayload odo;
        if (odo.SetFromMsg(msg.data_.data(), msg.data_.size())) {
            info.unique_name_ = msg.name_;
            SetDataTsFromGpsTime(info.data_ts_, odo.gps_time);
        }
    } else if ((msg.name_ == FpaOdomenuPayload::MSG_NAME)) {
        FpaOdomenuPayload odo;
        if (odo.SetFromMsg(msg.data_.data(), msg.data_.size())) {
            info.unique_name_ = msg.name_;
            SetDataTsFromGpsTime(info.data_ts_, odo.gps_time);
        }
    } else if ((msg.name_ == FpaOdomstatusPayload::MSG_NAME)) {
        FpaOdomstatusPayload status;
        if (status.SetFromMsg(msg.data_.data(), msg.data_.size())) {
            info.unique_name_ = msg.name_;
            SetDataTsFromGpsTime(info.data_ts_, status.gps_time);
        }
    } else if ((msg.name_ == FpaRawimuPayload::MSG_NAME)) {
        FpaRawimuPayload imu;
        if (imu.SetFromMsg(msg.data_.data(), msg.data_.size())) {
            info.unique_name_ = msg.name_;
            SetDataTsFromGpsTime(info.data_ts_, imu.gps_time);
        }
    } else if ((msg.name_ == FpaCorrimuPayload::MSG_NAME)) {
        FpaCorrimuPayload imu;
        if (imu.SetFromMsg(msg.data_.data(), msg.data_.size())) {
            info.unique_name_ = msg.name_;
            SetDataTsFromGpsTime(info.data_ts_, imu.gps_time);
        }
    } else if ((msg.name_ == FpaImubiasPayload::MSG_NAME)) {
        FpaImubiasPayload bias;
        if (bias.SetFromMsg(msg.data_.data(), msg.data_.size())) {
            info.unique_name_ = msg.name_;
            SetDataTsFromGpsTime(info.data_ts_, bias.gps_time);
        }
    } else if ((msg.name_ == FpaLlhPayload::MSG_NAME)) {
        FpaLlhPayload llh;
        if (llh.SetFromMsg(msg.data_.data(), msg.data_.size())) {
            info.unique_name_ = msg.name_;
            SetDataTsFromGpsTime(info.data_ts_, llh.gps_time);
        }
    } else if ((msg.name_ == FpaTfPayload::MSG_NAME)) {
        FpaTfPayload tf;
        if (tf.SetFromMsg(msg.data_.data(), msg.data_.size())) {
            info.unique_name_ = msg.name_ + "_" + tf.frame_a + "_" + tf.frame_b;
            SetDataTsFromGpsTime(info.data_ts_, tf.gps_time);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void Stats::AnalyseUbx(const fpsdk::common::parser::ParserMsg& msg, MsgInfo& info)
{
    // No payload messages are poll messages
    // @todo we could do more classification...
    if (msg.data_.size() == UBX_FRAME_SIZE) {
        info.unique_name_ = msg.name_ + "_POLL";
    }

    const uint8_t cls_id = UbxClsId(msg.data_.data());
    const uint8_t msg_id = UbxMsgId(msg.data_.data());
    const uint32_t no_itow = std::numeric_limits<uint32_t>::max();
    uint32_t itow = no_itow;
    switch (cls_id) {
        case UBX_NAV_CLSID:
        case UBX_NAV2_CLSID:
            switch (msg_id) {
                case UBX_NAV_CLOCK_MSGID:
                case UBX_NAV_COV_MSGID:
                case UBX_NAV_DOP_MSGID:
                case UBX_NAV_EELL_MSGID:
                case UBX_NAV_EOE_MSGID:
                case UBX_NAV_GEOFENCE_MSGID:
                case UBX_NAV_ORB_MSGID:
                case UBX_NAV_PL_MSGID:
                case UBX_NAV_POSECEF_MSGID:
                case UBX_NAV_POSLLH_MSGID:
                case UBX_NAV_PVAT_MSGID:
                case UBX_NAV_PVT_MSGID:
                case UBX_NAV_SAT_MSGID:
                case UBX_NAV_SBAS_MSGID:
                case UBX_NAV_SIG_MSGID:
                case UBX_NAV_SLAS_MSGID:
                case UBX_NAV_STATUS_MSGID:
                case UBX_NAV_TIMEBDS_MSGID:
                case UBX_NAV_TIMEGAL_MSGID:
                case UBX_NAV_TIMEGLO_MSGID:
                case UBX_NAV_TIMEGPS_MSGID:
                case UBX_NAV_TIMELS_MSGID:
                case UBX_NAV_TIMENAVIC_MSGID:
                case UBX_NAV_TIMEQZSS_MSGID:
                case UBX_NAV_TIMEUTC_MSGID:
                case UBX_NAV_VELECEF_MSGID:
                case UBX_NAV_VELNED_MSGID:
                    if (msg.data_.size() >= (UBX_FRAME_SIZE + 4)) {
                        std::memcpy(&itow, msg.data_.data() + UBX_HEAD_SIZE, sizeof(itow));
                    }
                    break;
                case UBX_NAV_HPPOSECEF_MSGID:
                case UBX_NAV_HPPOSLLH_MSGID:
                case UBX_NAV_ODO_MSGID:
                case UBX_NAV_RELPOSNED_MSGID:
                case UBX_NAV_SVIN_MSGID:
                case UBX_NAV_TIMETRUSTED_MSGID:
                    if (msg.data_.size() >= (UBX_FRAME_SIZE + 4 + 4)) {
                        std::memcpy(&itow, msg.data_.data() + UBX_HEAD_SIZE + 4, sizeof(itow));
                    }
                    break;
            }
            break;
        case UBX_RXM_CLSID:
            switch (msg_id) {
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
    if (itow != no_itow) {
        const auto now = Time::FromClockRealtime().GetWnoTow(WnoTow::Sys::GPS);
        const double tow = (double)itow * 1e-3;
        // @todo will this work correctly at tow rollover?
        info.data_ts_.SetWnoTow({ now.tow_ < tow ? now.wno_ - 1 : now.wno_, tow, WnoTow::Sys::GPS });
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void Stats::AnalyseRtcm3(const fpsdk::common::parser::ParserMsg& msg, MsgInfo& info)
{
    Rtcm3MsmHeader msm;
    if (Rtcm3GetMsmHeader(msg.data_.data(), msm)) {
        // @todo This doesn't work correctly at week (or for GLO, day) rollover
        switch (msm.gnss_) {  // clang-format off
            case Rtcm3MsmGnss::SBAS:
            case Rtcm3MsmGnss::QZSS:
            case Rtcm3MsmGnss::GPS:
                info.data_ts_.SetWnoTow({ info.recv_ts_.GetWnoTow(WnoTow::Sys::GPS).wno_, msm.gps_tow_, WnoTow::Sys::GPS });
                break;
            case Rtcm3MsmGnss::GAL:
                info.data_ts_.SetWnoTow({ info.recv_ts_.GetWnoTow(WnoTow::Sys::GAL).wno_, msm.gps_tow_, WnoTow::Sys::GAL });
                break;
            case Rtcm3MsmGnss::BDS:
                info.data_ts_.SetWnoTow({ info.recv_ts_.GetWnoTow(WnoTow::Sys::BDS).wno_, msm.gps_tow_, WnoTow::Sys::BDS });
                break;
            case Rtcm3MsmGnss::GLO: {
                auto gt = info.recv_ts_.GetGloTime();
                gt.TOD_ = std::fmod(msm.glo_tow_, SEC_IN_DAY_D);
                info.data_ts_.SetGloTime(gt);
                break;
            }
            case Rtcm3MsmGnss::NAVIC: break;
        }  // clang-format on
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffapps::streamtool
