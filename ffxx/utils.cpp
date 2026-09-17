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
 * @brief Utilities
 */

/* LIBC/STL */
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <map>
#include <vector>

/* EXTERNAL */
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/parser.hpp>
#include <fpsdk_common/parser/fpa.hpp>
#include <fpsdk_common/parser/nmea.hpp>
#include <fpsdk_common/parser/rtcm3.hpp>
#include <fpsdk_common/parser/ubx.hpp>
#include <fpsdk_common/string.hpp>

/* PACKAGE */
#include "utils.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;
using namespace fpsdk::common::time;
using namespace fpsdk::common::string;
using namespace fpsdk::common::parser;
using namespace fpsdk::common::parser::ubx;
using namespace fpsdk::common::parser::nmea;
using namespace fpsdk::common::parser::fpa;
using namespace fpsdk::common::parser::rtcm3;

const char* GetVersionString()
{
    return FF_VERSION_STRING;  // "0.0.0", "0.0.0-heads/feature/bla-g123456-dirty"
}

// ---------------------------------------------------------------------------------------------------------------------

const char* GetCopyrightString()
{
    return "Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)";
}

// ---------------------------------------------------------------------------------------------------------------------

const char* GetLicenseString()
{
    return "Licenses: see the LICENSE files included in the source distribution";
}

// ---------------------------------------------------------------------------------------------------------------------

std::string GetUserAgentStr()
{
    return "ffxx/" + StrSplit(GetVersionString(), "-")[0];
}

// ---------------------------------------------------------------------------------------------------------------------

static std::map<CommonMessage, ParserMsg> gCommonMessages;

static const ParserMsg& sGetCommonMessage(CommonMessage which)
{
    auto entry = gCommonMessages.find(which);
    if (entry != gCommonMessages.end()) {
        return entry->second;
    } else {
        static ParserMsg empty;
        return empty;
    }
}

static void sMakeCommonMessage(CommonMessage which, const std::vector<uint8_t>& data)
{
    Parser parser;
    ParserMsg msg;
    parser.Add(data);
    parser.Process(msg);
    msg.MakeInfo();
    gCommonMessages.emplace(which, std::move(msg));
}

const ParserMsg& GetCommonMessage(CommonMessage which)
{
    // We alredy initialised all common messages
    if (!gCommonMessages.empty()) {
        return sGetCommonMessage(which);
    }

    // Initialise common messages
    sMakeCommonMessage(CommonMessage::UBX_MON_VER, { 0xb5, 0x62, 0x0a, 0x04, 0x00, 0x00, 0x0e, 0x34 });
    sMakeCommonMessage(CommonMessage::FP_A_VERSION, StrToBuf("$FP,VERSION*60\r\n"));
    sMakeCommonMessage(
        CommonMessage::FP_B_VERSION, { 0x66, 0x21, 0xfd, 0x08, 0x00, 0x00, 0x00, 0x00, 0x70, 0x20, 0xe0, 0x49 });

    // Unfortunately UBX-CFG-VALDEL doesn't support wildcards, use the deprecated UBX-CFG-CFG interface instead
    {
        const UBX_CFG_CFG_V0_GROUP0 payload0 = { // clang-format off
            .clearMask = UBX_CFG_CFG_V0_CLEAR_ALL,
            .saveMask  = UBX_CFG_CFG_V0_SAVE_NONE,
            .loadMask  = UBX_CFG_CFG_V0_LOAD_NONE
        };
        const UBX_CFG_CFG_V0_GROUP1 payload1 = {
            .deviceMask = (UBX_CFG_CFG_V0_DEVICE_BBR | UBX_CFG_CFG_V0_DEVICE_FLASH)
        };  // clang-format on
        uint8_t payload[UBX_CFG_CFG_V0_MAX_SIZE];
        std::memcpy(&payload[0], &payload0, sizeof(payload0));
        std::memcpy(&payload[sizeof(payload0)], &payload1, sizeof(payload1));
        const std::size_t payloadSize = sizeof(payload0) + sizeof(payload1);
        std::vector<uint8_t> msg;
        UbxMakeMessage(msg, UBX_CFG_CLSID, UBX_CFG_CFG_MSGID, (const uint8_t*)&payload, payloadSize);
        sMakeCommonMessage(CommonMessage::UBX_RESET_DEFAULT_1, msg);
        sMakeCommonMessage(CommonMessage::UBX_RESET_FACTORY_1, msg);
    }

    for (auto which_reset :
        { CommonMessage::UBX_RESET_SOFT, CommonMessage::UBX_RESET_HARD, CommonMessage::UBX_RESET_HOT,
            CommonMessage::UBX_RESET_WARM, CommonMessage::UBX_RESET_COLD, CommonMessage::UBX_RESET_GNSS_STOP,
            CommonMessage::UBX_RESET_GNSS_START, CommonMessage::UBX_RESET_GNSS_RESTART,
            CommonMessage::UBX_RESET_DEFAULT_2, CommonMessage::UBX_RESET_FACTORY_2 }) {
        UBX_CFG_RST_V0_GROUP0 payload;
        payload.reserved = UBX_CFG_RST_V0_RESERVED;
        switch (which_reset) {
            case CommonMessage::UBX_RESET_SOFT:
                payload.navBbrMask = UBX_CFG_RST_V0_NAVBBR_NONE;
                payload.resetMode = UBX_CFG_RST_V0_RESETMODE_SW;
                break;
            case CommonMessage::UBX_RESET_HARD:
                payload.navBbrMask = UBX_CFG_RST_V0_NAVBBR_NONE;
                payload.resetMode = UBX_CFG_RST_V0_RESETMODE_HW_CONTROLLED;
                break;
            case CommonMessage::UBX_RESET_HOT:
                payload.navBbrMask = UBX_CFG_RST_V0_NAVBBR_HOTSTART;
                payload.resetMode = UBX_CFG_RST_V0_RESETMODE_GNSS;
                break;
            case CommonMessage::UBX_RESET_WARM:
                payload.navBbrMask = UBX_CFG_RST_V0_NAVBBR_WARMSTART;
                payload.resetMode = UBX_CFG_RST_V0_RESETMODE_GNSS;
                break;
            case CommonMessage::UBX_RESET_COLD:
                payload.navBbrMask = UBX_CFG_RST_V0_NAVBBR_COLDSTART;
                payload.resetMode = UBX_CFG_RST_V0_RESETMODE_GNSS;
                break;
            case CommonMessage::UBX_RESET_DEFAULT_2:
                payload.navBbrMask = UBX_CFG_RST_V0_NAVBBR_NONE;
                payload.resetMode = UBX_CFG_RST_V0_RESETMODE_HW_FORCED;
                break;
            case CommonMessage::UBX_RESET_FACTORY_2:
                payload.navBbrMask = UBX_CFG_RST_V0_NAVBBR_COLDSTART;
                payload.resetMode = UBX_CFG_RST_V0_RESETMODE_HW_CONTROLLED;
                break;
            case CommonMessage::UBX_RESET_GNSS_STOP:
                payload.navBbrMask = UBX_CFG_RST_V0_NAVBBR_NONE;
                payload.resetMode = UBX_CFG_RST_V0_RESETMODE_GNSS_STOP;
                break;
            case CommonMessage::UBX_RESET_GNSS_START:
                payload.navBbrMask = UBX_CFG_RST_V0_NAVBBR_NONE;
                payload.resetMode = UBX_CFG_RST_V0_RESETMODE_GNSS_START;
                break;
            case CommonMessage::UBX_RESET_GNSS_RESTART:
                payload.navBbrMask = UBX_CFG_RST_V0_NAVBBR_NONE;
                payload.resetMode = UBX_CFG_RST_V0_RESETMODE_GNSS;
                break;
            default:
                break;
        }
        std::vector<uint8_t> msg;
        UbxMakeMessage(msg, UBX_CFG_CLSID, UBX_CFG_RST_MSGID, (const uint8_t*)&payload, sizeof(payload));
        sMakeCommonMessage(which_reset, msg);
    }

    {
        std::vector<uint8_t> msg;
        UbxMakeMessage(msg, UBX_UPD_CLSID, UBX_UPD_SAFEBOOT_MSGID, nullptr, 0);
        sMakeCommonMessage(CommonMessage::UBX_RESET_SAFEBOOT, msg);
    }

    sMakeCommonMessage(CommonMessage::UBX_TRAINING, { 0x55, 0x55 });

    // Quectel LC29H

#define _EVAL(_which_, _sentence_)                       \
    {                                                    \
        std::vector<uint8_t> msg;                        \
        NmeaMakeMessage(msg, _sentence_);                \
        sMakeCommonMessage(CommonMessage::_which_, msg); \
    }

    _EVAL(QUECTEL_LC29H_HOT, "PAIR001,004,0")
    _EVAL(QUECTEL_LC29H_WARM, "PAIR001,005,0")
    _EVAL(QUECTEL_LC29H_COLD, "PAIR001,006,0")
    _EVAL(QUECTEL_LC29H_REBOOT, "PAIR023")
    _EVAL(QUECTEL_LC29H_VERNO, "PQTMVERNO")
    _EVAL(QUECTEL_LG290P_HOT, "PQTMHOT")
    _EVAL(QUECTEL_LG290P_WARM, "PQTMWARM")
    _EVAL(QUECTEL_LG290P_COLD, "PQTMCOLD")
    _EVAL(QUECTEL_LG290P_REBOOT, "PQTMSRR")
    _EVAL(QUECTEL_LG290P_VERNO, "PQTMVERNO")

#undef _EVAL

    // for (const auto& entry : gCommonMessages) {
    //     auto& msg = sGetCommonMessage(entry.first);
    //     DEBUG("%-20s %2" PRIuMAX " %s", msg.name_.c_str(), msg.data_.size(), msg.info_.c_str());
    // }

    // Now we should have it
    return sGetCommonMessage(which);
}

// ---------------------------------------------------------------------------------------------------------------------

inline void SetDataTsFromFpaGpsTime(Time& data_ts, const FpaGpsTime& gps_time)
{
    if (gps_time.week.valid && gps_time.tow.valid) {
        data_ts.SetWnoTow({ gps_time.week.value, gps_time.tow.value, WnoTow::Sys::GPS });
    }
}

Time GetMessageDataTime(const ParserMsg& msg, const fpsdk::common::time::Time& recv_ts)
{
    Time data_ts;
    // FP_A
    if (msg.proto_ == Protocol::FP_A) {
        if (msg.name_ == FpaEoePayload::MSG_NAME) {
            FpaEoePayload eoe;
            if (eoe.SetFromMsg(msg.data_.data(), msg.data_.size())) {
                SetDataTsFromFpaGpsTime(data_ts, eoe.gps_time);
            }
        } else if ((msg.name_ == FpaOdometryPayload::MSG_NAME)) {
            FpaOdometryPayload odo;
            if (odo.SetFromMsg(msg.data_.data(), msg.data_.size())) {
                SetDataTsFromFpaGpsTime(data_ts, odo.gps_time);
            }
        } else if ((msg.name_ == FpaOdomshPayload::MSG_NAME)) {
            FpaOdomshPayload odo;
            if (odo.SetFromMsg(msg.data_.data(), msg.data_.size())) {
                SetDataTsFromFpaGpsTime(data_ts, odo.gps_time);
            }
        } else if ((msg.name_ == FpaOdomenuPayload::MSG_NAME)) {
            FpaOdomenuPayload odo;
            if (odo.SetFromMsg(msg.data_.data(), msg.data_.size())) {
                SetDataTsFromFpaGpsTime(data_ts, odo.gps_time);
            }
        } else if ((msg.name_ == FpaOdomstatusPayload::MSG_NAME)) {
            FpaOdomstatusPayload status;
            if (status.SetFromMsg(msg.data_.data(), msg.data_.size())) {
                SetDataTsFromFpaGpsTime(data_ts, status.gps_time);
            }
        } else if ((msg.name_ == FpaRawimuPayload::MSG_NAME)) {
            FpaRawimuPayload imu;
            if (imu.SetFromMsg(msg.data_.data(), msg.data_.size())) {
                SetDataTsFromFpaGpsTime(data_ts, imu.gps_time);
            }
        } else if ((msg.name_ == FpaCorrimuPayload::MSG_NAME)) {
            FpaCorrimuPayload imu;
            if (imu.SetFromMsg(msg.data_.data(), msg.data_.size())) {
                SetDataTsFromFpaGpsTime(data_ts, imu.gps_time);
            }
        } else if ((msg.name_ == FpaImubiasPayload::MSG_NAME)) {
            FpaImubiasPayload bias;
            if (bias.SetFromMsg(msg.data_.data(), msg.data_.size())) {
                SetDataTsFromFpaGpsTime(data_ts, bias.gps_time);
            }
        } else if ((msg.name_ == FpaLlhPayload::MSG_NAME)) {
            FpaLlhPayload llh;
            if (llh.SetFromMsg(msg.data_.data(), msg.data_.size())) {
                SetDataTsFromFpaGpsTime(data_ts, llh.gps_time);
            }
        } else if ((msg.name_ == FpaTfPayload::MSG_NAME)) {
            FpaTfPayload tf;
            if (tf.SetFromMsg(msg.data_.data(), msg.data_.size())) {
                SetDataTsFromFpaGpsTime(data_ts, tf.gps_time);
            }
        }
    }
    // UBX
    else if (msg.proto_ == Protocol::UBX) {
        const uint8_t cls_id = UbxClsId(msg.Data());
        const uint8_t msg_id = UbxMsgId(msg.Data());
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
        }
        if (itow != no_itow) {
            const auto wnotow = recv_ts.GetWnoTow(WnoTow::Sys::GPS);
            const double tow = (double)itow * 1e-3;
            // @todo will this work correctly at tow rollover?
            data_ts.SetWnoTow({ wnotow.tow_ < tow ? wnotow.wno_ - 1 : wnotow.wno_, tow, WnoTow::Sys::GPS });
        }
    }
    // RTCM3
    else if (msg.proto_ == Protocol::RTCM3) {
        Rtcm3MsmHeader msm;
        if (Rtcm3GetMsmHeader(msg.data_.data(), msm)) {
            // @todo This doesn't work correctly at week (or for GLO, day) rollover
            switch (msm.gnss_) {
                case Rtcm3MsmGnss::SBAS:
                case Rtcm3MsmGnss::QZSS:
                case Rtcm3MsmGnss::GPS: {
                    const auto wnotow = recv_ts.GetWnoTow(WnoTow::Sys::GPS);
                    data_ts.SetWnoTow({ wnotow.tow_ < msm.gps_tow_ ? wnotow.wno_ - 1 : wnotow.wno_, msm.gps_tow_, WnoTow::Sys::GPS });
                    break;
                }
                case Rtcm3MsmGnss::GAL: {
                    const auto wnotow = recv_ts.GetWnoTow(WnoTow::Sys::GAL);
                    data_ts.SetWnoTow({ wnotow.tow_ < msm.glo_tow_ ? wnotow.wno_ - 1 : wnotow.wno_, msm.glo_tow_, WnoTow::Sys::GAL });
                    break;
                }
                case Rtcm3MsmGnss::BDS: {
                    const auto wnotow = recv_ts.GetWnoTow(WnoTow::Sys::BDS);
                    data_ts.SetWnoTow({ wnotow.tow_ < msm.bds_tow_ ? wnotow.wno_ - 1 : wnotow.wno_, msm.bds_tow_, WnoTow::Sys::BDS });
                    break;
                }
                case Rtcm3MsmGnss::GLO: {
                    auto gt = recv_ts.GetGloTime();
                    gt.TOD_ = std::fmod(msm.glo_tow_, SEC_IN_DAY_D);
                    data_ts.SetGloTime(gt);
                    break;
                }
                case Rtcm3MsmGnss::NAVIC:
                    break;
            }
        }
    }

    return data_ts;
}

// ---------------------------------------------------------------------------------------------------------------------

bool SystemdNotify(const char* args) {
    DEBUG("SystemdNotify %s", args);
    if (!args || (args[0] == '\0')) {
        return false;
    }
    const char* notify_socket = std::getenv("NOTIFY_SOCKET");
    if ((notify_socket == nullptr) || (notify_socket[0] == '\0')) {
        WARNING("SystemdNotify no socket");
        return true;
    }

    char cmd[1024];
    if (std::snprintf(cmd, sizeof(cmd), "exec systemd-notify %s", args) > (int)sizeof(cmd)) {
        WARNING("SystemdNotify args too long");
        return false;
    }
    TRACE("SystemdNotify %s", cmd);
    const int res = std::system(cmd);
    if (res != 0) {
        WARNING("SystemdNotify %s fail (%d)", args, res);
        return false;
    } else {
        return true;
    }

    return true;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
