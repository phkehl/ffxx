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
 * @brief Navigation epoch
 */

/* LIBC/STL */
#include <cstring>

/* EXTERNAL */
#include <fpsdk_common/ext/eigen_core.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/math.hpp>
#include <fpsdk_common/parser/nmea.hpp>
#include <fpsdk_common/parser/ubx.hpp>
#include <fpsdk_common/string.hpp>
#include <fpsdk_common/trafo.hpp>
#include <fpsdk_common/types.hpp>

/* PACKAGE */
#include "epoch.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::math;
using namespace fpsdk::common::parser;
using namespace fpsdk::common::parser::nmea;
using namespace fpsdk::common::parser::ubx;
using namespace fpsdk::common::string;
using namespace fpsdk::common::trafo;
using namespace fpsdk::common::types;

#if 0
#  define EPOCH_TRACE(fmt, args...) TRACE("epoch: " fmt, ##args)
#else
#  define EPOCH_TRACE(...) /* nothing */
#endif

// ---------------------------------------------------------------------------------------------------------------------

struct CollDetect  // clang-format off
{
    std::size_t  seq = 0;
    uint32_t     ubxItow = 0;
    bool         haveUbxItow = false;
    int          nmeaMillis = -1;
    bool         haveNmeaMillis = false;
};  // clang-format on

// "Quality" (precision) if information: e.g. UBX better than NMEA, UBX high-precision messages better than normal UBX
enum class CollQual : int
{
    NOTHING = 0,
    NMEA,
    UBX,
    UBX_HP
};

static const char* CollQualStr(const CollQual qual)
{
    switch (qual) {  // clang-format off
        case CollQual::NOTHING: return "NOTHING";
        case CollQual::NMEA:    return "NMEA";
        case CollQual::UBX:     return "UBX";
        case CollQual::UBX_HP:  return "UBX_HP";
    }  // clang-format on
    return "?";
}

struct CollState  // clang-format off
{
    CollQual haveFixType = CollQual::NOTHING;

    CollQual havePosLlh  = CollQual::NOTHING;
    CollQual havePosLl   = CollQual::NOTHING;
    CollQual havePosXyz  = CollQual::NOTHING;
    CollQual haveRelPos  = CollQual::NOTHING;

    CollQual havePosAcc      = CollQual::NOTHING;
    CollQual havePosAccHoriz = CollQual::NOTHING;
    CollQual havePosAccVert  = CollQual::NOTHING;

    CollQual haveVelNed  = CollQual::NOTHING;
    CollQual haveVelAcc  = CollQual::NOTHING;

    CollQual haveSigs    = CollQual::NOTHING;
    CollQual haveSats    = CollQual::NOTHING;
    CollQual haveGpsTow  = CollQual::NOTHING;
    CollQual haveGpsWno  = CollQual::NOTHING;
    CollQual haveDiffAge = CollQual::NOTHING;

    CollQual haveNumSatUsed = CollQual::NOTHING;

    CollQual haveClock   = CollQual::NOTHING;
    CollQual haveUptime  = CollQual::NOTHING;
    CollQual haveTimeAcc = CollQual::NOTHING;

    CollQual haveHms    = CollQual::NOTHING;
    bool     confHms    = false; // hour, minute, second are confirmed valid
    bool     leapsKnown  = false;
    int      hour        = 0;
    int      minute      = 0;
    double   second      = 0.0;

    CollQual haveYmd    = CollQual::NOTHING;
    bool     confYmd    = false; // day, month, year are confirmed valid
    int      day         = 0;
    int      month       = 0;
    int      year        = 0;

    std::vector<NmeaGsvPayload> gsvMsgs;
    std::vector<NmeaGsaPayload> gsaMsgs;

    bool     relPosValid = false;
};  // clang-format on

// ---------------------------------------------------------------------------------------------------------------------

EpochCollector::EpochCollector()
{
    Reset();
}

EpochCollector::~EpochCollector()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void EpochCollector::Reset()
{
    coll_ = std::make_unique<Epoch>();

    const std::size_t seq = (detect_ ? detect_->seq : 0);
    detect_ = std::make_unique<CollDetect>();
    detect_->seq = seq;

    state_ = std::make_unique<CollState>();
}

// ---------------------------------------------------------------------------------------------------------------------

static bool DetectNmea(CollDetect& detect, const ParserMsg& msg, const NmeaPayloadPtr& nmea);
static bool DetectUbx(CollDetect& detect, const ParserMsg& msg);
static void CollectUbx(Epoch& coll, CollState& state, const ParserMsg& msg);
static void CollectNmea(Epoch& coll, CollState& state, const ParserMsg& msg, const NmeaPayloadPtr& nmea);
static void Complete(Epoch& coll, CollState& state);

EpochPtr EpochCollector::Collect(const fpsdk::common::parser::ParserMsg& msg)
{
    EPOCH_TRACE("collect %s", msg.name_.c_str());

    // Decode NMEA here, as this is quite expensive
    NmeaPayloadPtr nmea;
    if (msg.proto_ == Protocol::NMEA) {
        nmea = NmeaDecodeMessage(msg.Data(), msg.Size());
    }

    // Detect end of epoch / start of next epoch
    bool complete = false;
    switch (msg.proto_) {
        case Protocol::UBX:
            if (DetectUbx(*detect_, msg)) {
                complete = true;
                detect_->haveNmeaMillis = false;
            }
            break;
        case Protocol::NMEA:
            if (nmea && DetectNmea(*detect_, msg, nmea)) {
                complete = true;
                detect_->haveUbxItow = false;
            }
            break;
        default:
            break;
    }

    // TODO: in case of UBX-NAV-EOE, output that first and complete epoch on next call (if it can be done nicely..)

    // Output epoch
    EpochPtr epoch;
    if (complete) {
        // Complete data
        detect_->seq++;
        coll_->seq = detect_->seq;
        Complete(*coll_, *state_);

        // Collected data becomes the epoch we'll return
        std::swap(epoch, coll_);
        // epoch = std::make_unique<Epoch>(coll_.release());

        // Prepare for next epoch
        Reset();
    }

    // Collect data
    switch (msg.proto_) {
        case Protocol::UBX:
            CollectUbx(*coll_, *state_, msg);
            break;
        case Protocol::NMEA:
            if (nmea) {
                CollectNmea(*coll_, *state_, msg, nmea);
            }
            break;
        default:
            break;
    }

    return epoch;
}

// ---------------------------------------------------------------------------------------------------------------------

static bool DetectNmea(CollDetect& detect, const ParserMsg& msg, const NmeaPayloadPtr& nmea)
{
    bool complete = false;
    int millis = -1;
    switch (nmea->formatter_) {
        case NmeaFormatter::GGA: {
            const auto& gga = dynamic_cast<const NmeaGgaPayload&>(*nmea);
            if (gga.time.valid) {
                millis = std::floor(gga.time.secs * 1e3);
            }
            break;
        }
        case NmeaFormatter::RMC: {
            const auto& rmc = dynamic_cast<const NmeaRmcPayload&>(*nmea);
            if (rmc.time.valid) {
                millis = std::floor(rmc.time.secs * 1e3);
            }
            break;
        }
        case NmeaFormatter::GLL: {
            const auto& gll = dynamic_cast<const NmeaGllPayload&>(*nmea);
            if (gll.time.valid) {
                millis = std::floor(gll.time.secs * 1e3);
            }
            break;
        }
        default:
            break;
    }

    if (millis >= 0) {
        if (detect.haveNmeaMillis && (millis != detect.nmeaMillis)) {
            EPOCH_TRACE("detect %s %d != %d", msg.name_.c_str(), millis, detect.nmeaMillis);
            (void)msg;
            complete = true;
        }
        detect.nmeaMillis = millis;
        detect.haveNmeaMillis = true;
    }

    return complete;
}

// ---------------------------------------------------------------------------------------------------------------------

static bool DetectUbx(CollDetect& detect, const ParserMsg& msg)
{
    const uint8_t* ubx = msg.Data();
    const std::size_t size = msg.Size();

    const uint8_t clsId = UbxClsId(ubx);
    if (clsId != UBX_NAV_CLSID) {
        return false;
    }
    const uint8_t msgId = UbxMsgId(ubx);
    bool complete = false;
    uint32_t iTow = std::numeric_limits<uint32_t>::max();
    switch (msgId) {
        case UBX_NAV_EOE_MSGID:
            EPOCH_TRACE("detect %s", msg.name_.c_str());
            detect.haveUbxItow = false;
            complete = true;
            break;
        case UBX_NAV_PVT_MSGID:
        case UBX_NAV_SAT_MSGID:
        case UBX_NAV_ORB_MSGID:
        case UBX_NAV_STATUS_MSGID:
        case UBX_NAV_SIG_MSGID:
        case UBX_NAV_CLOCK_MSGID:
        case UBX_NAV_DOP_MSGID:
        case UBX_NAV_POSECEF_MSGID:
        case UBX_NAV_POSLLH_MSGID:
        case UBX_NAV_VELECEF_MSGID:
        case UBX_NAV_VELNED_MSGID:
        case UBX_NAV_GEOFENCE_MSGID:
        case UBX_NAV_TIMEUTC_MSGID:
        case UBX_NAV_TIMELS_MSGID:
        case UBX_NAV_TIMEGPS_MSGID:
        case UBX_NAV_TIMEGLO_MSGID:
        case UBX_NAV_TIMEBDS_MSGID:
        case UBX_NAV_TIMEGAL_MSGID:
            if (size > (UBX_FRAME_SIZE + 4)) {
                std::memcpy(&iTow, &ubx[UBX_HEAD_SIZE], sizeof(iTow));
            }
            break;
        case UBX_NAV_SVIN_MSGID:
        case UBX_NAV_ODO_MSGID:
        case UBX_NAV_HPPOSLLH_MSGID:
        case UBX_NAV_HPPOSECEF_MSGID:
        case UBX_NAV_RELPOSNED_MSGID:
            if (size > (UBX_FRAME_SIZE + 4 + 4)) {
                std::memcpy(&iTow, &ubx[UBX_HEAD_SIZE + 4], sizeof(iTow));
            }
            break;
    }

    if (iTow != std::numeric_limits<uint32_t>::max()) {
        if (detect.haveUbxItow && (detect.ubxItow != iTow)) {
            EPOCH_TRACE("detect %s %" PRIu32 " != %" PRIu32, msg.name_.c_str(), detect.ubxItow, iTow);
            complete = true;
        }
        detect.ubxItow = iTow;
        detect.haveUbxItow = true;
    }

    return complete;
}

// ---------------------------------------------------------------------------------------------------------------------

static SigUse ubxSigUse(const uint8_t qualityInd);
static SigCorr ubxSigCorrSource(const uint8_t corrSource);
static SigIono ubxIonoModel(const uint8_t ionoModel);
static SigHealth ubxSigHealth(const uint8_t health);
static SatOrb ubxSatOrb(const uint8_t orbSrc);

static void CollectUbx(Epoch& coll, CollState& state, const ParserMsg& msg)
{
    const uint8_t* data = msg.Data();
    const std::size_t size = msg.Size();
    const uint8_t clsId = UbxClsId(data);
    if (clsId != UBX_NAV_CLSID) {
        return;
    }

    const uint8_t msgId = UbxMsgId(data);

    switch (msgId) {
        case UBX_NAV_PVT_MSGID:
            if (size == UBX_NAV_PVT_V1_SIZE) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());
                UBX_NAV_PVT_V1_GROUP0 pvt;
                std::memcpy(&pvt, &data[UBX_HEAD_SIZE], sizeof(pvt));

                // Fix info
                FixType fixType = FixType::UNKNOWN;
                if (state.haveFixType < CollQual::UBX) {
                    state.haveFixType = CollQual::UBX;
                    switch (pvt.fixType) {  // clang-format off
                        case UBX_NAV_PVT_V1_FIXTYPE_NOFIX:  fixType = FixType::NOFIX;     break;
                        case UBX_NAV_PVT_V1_FIXTYPE_DRONLY: fixType = FixType::DRONLY;    break;
                        case UBX_NAV_PVT_V1_FIXTYPE_2D:     fixType = FixType::SPP_2D;    break;
                        case UBX_NAV_PVT_V1_FIXTYPE_3D:     fixType = FixType::SPP_3D;    break;
                        case UBX_NAV_PVT_V1_FIXTYPE_3D_DR:  fixType = FixType::SPP_3D_DR; break;
                        case UBX_NAV_PVT_V1_FIXTYPE_TIME:   fixType = FixType::TIME;      break;
                    }  // clang-format on
                    if (fixType > FixType::NOFIX) {
                        coll.fixOk = UBX_NAV_PVT_V1_FLAGS_GNSSFIXOK(pvt.flags);
                    }
                    const uint8_t carrSoln = UBX_NAV_PVT_V1_FLAGS_CARRSOLN(pvt.flags);
                    switch (carrSoln) {
                        case UBX_NAV_PVT_V1_FLAGS_CARRSOLN_FLOAT:
                            fixType = (fixType == FixType::SPP_3D_DR ? FixType::RTK_FLOAT_DR : FixType::RTK_FLOAT);
                            break;
                        case UBX_NAV_PVT_V1_FLAGS_CARRSOLN_FIXED:
                            fixType = (fixType == FixType::SPP_3D_DR ? FixType::RTK_FIXED_DR : FixType::RTK_FIXED);
                            break;
                    }
                    coll.fixType = fixType;
                }

                // Time
                if (UBX_NAV_PVT_V1_VALID_VALIDTIME(pvt.valid) && (state.haveHms < CollQual::UBX)) {
                    state.haveHms = CollQual::UBX;
                    state.haveTimeAcc = CollQual::UBX;
                    state.hour = pvt.hour;
                    state.minute = pvt.min;
                    state.second = (double)pvt.sec + ((double)pvt.nano * 1e-9);
                    state.confHms =
                        (UBX_NAV_PVT_V1_FLAGS2_CONFAVAIL(pvt.flags2) && UBX_NAV_PVT_V1_FLAGS2_CONFTIME(pvt.flags2));
                    coll.timeAcc = (double)pvt.tAcc * UBX_NAV_PVT_V1_TACC_SCALE;
                    state.leapsKnown = UBX_NAV_PVT_V1_VALID_FULLYRESOLVED(pvt.valid);
                }

                // Date
                if (UBX_NAV_PVT_V1_VALID_VALIDDATE(pvt.valid) && (state.haveYmd < CollQual::UBX)) {
                    state.haveYmd = CollQual::UBX;
                    state.year = pvt.year;
                    state.month = pvt.month;
                    state.day = pvt.day;
                    state.confYmd =
                        (UBX_NAV_PVT_V1_FLAGS2_CONFAVAIL(pvt.flags2) && UBX_NAV_PVT_V1_FLAGS2_CONFDATE(pvt.flags2));
                }

                // Geodetic coordinates
                if (!UBX_NAV_PVT_V1_FLAGS3_INVALIDLLH(pvt.flags3) && (state.havePosLlh < CollQual::UBX)) {
                    state.havePosLlh = CollQual::UBX;
                    coll.posLlh[0] = DegToRad((double)pvt.lat * UBX_NAV_PVT_V1_LAT_SCALE);
                    coll.posLlh[1] = DegToRad((double)pvt.lon * UBX_NAV_PVT_V1_LON_SCALE);
                    coll.posLlh[2] = (double)pvt.height * UBX_NAV_PVT_V1_HEIGHT_SCALE;
                    coll.heightMsl = (double)pvt.hMSL * UBX_NAV_PVT_V1_HEIGHT_SCALE;
                    coll.haveHeightMsl = true;
                }

                // Position accuracy estimate
                if ((fixType > FixType::NOFIX) && (state.havePosAccHoriz < CollQual::UBX)) {
                    state.havePosAccHoriz = CollQual::UBX;
                    coll.posAccHoriz = (double)pvt.hAcc * UBX_NAV_PVT_V1_HACC_SCALE;
                }
                if ((fixType > FixType::NOFIX) && (state.havePosAccVert < CollQual::UBX)) {
                    state.havePosAccVert = CollQual::UBX;
                    coll.posAccVert = (double)pvt.vAcc * UBX_NAV_PVT_V1_VACC_SCALE;
                }

                // Velocity
                if ((fixType > FixType::NOFIX) && (state.haveVelNed < CollQual::UBX)) {
                    state.haveVelNed = CollQual::UBX;
                    coll.velNed[0] = pvt.velN * UBX_NAV_PVT_V1_VELNED_SCALE;
                    coll.velNed[1] = pvt.velE * UBX_NAV_PVT_V1_VELNED_SCALE;
                    coll.velNed[2] = pvt.velD * UBX_NAV_PVT_V1_VELNED_SCALE;
                }
                if ((fixType > FixType::NOFIX) && (state.haveVelAcc < CollQual::UBX)) {
                    state.haveVelAcc = CollQual::UBX;
                    coll.velAcc = pvt.sAcc * UBX_NAV_PVT_V1_SACC_SCALE;
                }

                coll.pDOP = (float)pvt.pDOP * UBX_NAV_PVT_V1_PDOP_SCALE;
                coll.havePdop = true;

                if (state.haveNumSatUsed < CollQual::UBX) {
                    state.haveNumSatUsed = CollQual::UBX;
                    coll.numSatUsed.numTotal = pvt.numSV;
                }

                if (state.haveGpsTow < CollQual::UBX) {
                    state.haveGpsTow = CollQual::UBX;
                    coll.gpsTow = pvt.iTOW * UBX_NAV_PVT_V1_ITOW_SCALE;
                }
            }
            break;

        case UBX_NAV_POSECEF_MSGID:
            if (size == UBX_NAV_POSECEF_V0_SIZE) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());
                UBX_NAV_POSECEF_V0_GROUP0 pos;
                std::memcpy(&pos, &data[UBX_HEAD_SIZE], sizeof(pos));
                if (state.havePosXyz < CollQual::UBX) {
                    state.havePosXyz = CollQual::UBX;
                    coll.posXyz[0] = (double)pos.ecefX * UBX_NAV_POSECEF_V0_ECEF_XYZ_SCALE;
                    coll.posXyz[1] = (double)pos.ecefY * UBX_NAV_POSECEF_V0_ECEF_XYZ_SCALE;
                    coll.posXyz[2] = (double)pos.ecefZ * UBX_NAV_POSECEF_V0_ECEF_XYZ_SCALE;
                }
                if (state.havePosAcc < CollQual::UBX) {
                    state.havePosAcc = CollQual::UBX;
                    coll.posAcc = (double)pos.pAcc * UBX_NAV_POSECEF_V0_PACC_SCALE;
                }
            }
            break;

        case UBX_NAV_HPPOSECEF_MSGID:
            if ((size == UBX_NAV_HPPOSECEF_V0_SIZE) &&
                (UBX_NAV_HPPOSECEF_VERSION(data) == UBX_NAV_HPPOSECEF_V0_VERSION)) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());
                UBX_NAV_HPPOSECEF_V0_GROUP0 pos;
                std::memcpy(&pos, &data[UBX_HEAD_SIZE], sizeof(pos));
                if (!UBX_NAV_HPPOSECEF_V0_FLAGS_INVALIDECEF(pos.flags)) {
                    if (state.havePosXyz < CollQual::UBX_HP) {
                        state.havePosXyz = CollQual::UBX_HP;
                        coll.posXyz[0] = ((double)pos.ecefX * UBX_NAV_HPPOSECEF_V0_ECEF_XYZ_SCALE) +
                                         ((double)pos.ecefXHp * UBX_NAV_HPPOSECEF_V0_ECEF_XYZ_HP_SCALE);
                        coll.posXyz[1] = ((double)pos.ecefY * UBX_NAV_HPPOSECEF_V0_ECEF_XYZ_SCALE) +
                                         ((double)pos.ecefYHp * UBX_NAV_HPPOSECEF_V0_ECEF_XYZ_HP_SCALE);
                        coll.posXyz[2] = ((double)pos.ecefZ * UBX_NAV_HPPOSECEF_V0_ECEF_XYZ_SCALE) +
                                         ((double)pos.ecefZHp * UBX_NAV_HPPOSECEF_V0_ECEF_XYZ_HP_SCALE);
                    }
                    if (state.havePosAcc < CollQual::UBX_HP) {
                        state.havePosAcc = CollQual::UBX_HP;
                        coll.posAcc = (double)pos.pAcc * UBX_NAV_HPPOSECEF_V0_PACC_SCALE;
                    }
                }
            }
            break;

        case UBX_NAV_TIMEGPS_MSGID:
            if (size == UBX_NAV_TIMEGPS_V0_SIZE) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());
                UBX_NAV_TIMEGPS_V0_GROUP0 time;
                std::memcpy(&time, &data[UBX_HEAD_SIZE], sizeof(time));

                // Store anyway, receiver counts up tow even when not known yet
                coll.gpsWno = time.week;
                coll.gpsTow = (time.iTow * UBX_NAV_TIMEGPS_V0_ITOW_SCALE) + (time.fTOW * UBX_NAV_TIMEGPS_V0_FTOW_SCALE);
                if (UBX_NAV_TIMEGPS_V0_VALID_WEEKVALID(time.valid) && (state.haveGpsWno < CollQual::UBX)) {
                    state.haveGpsWno = CollQual::UBX;
                }
                if (UBX_NAV_TIMEGPS_V0_VALID_TOWVALID(time.valid)) {
                    if (state.haveGpsTow < CollQual::UBX_HP) {
                        state.haveGpsTow = CollQual::UBX_HP;
                    }
                    if (state.haveTimeAcc < CollQual::UBX_HP) {
                        state.haveTimeAcc = CollQual::UBX_HP;
                        coll.timeAcc = time.tAcc * UBX_NAV_TIMEGPS_V0_TACC_SCALE;
                    }
                }
            }
            break;

        case UBX_NAV_STATUS_MSGID:
            if (size == UBX_NAV_STATUS_V0_SIZE) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());
                UBX_NAV_STATUS_V0_GROUP0 status;
                std::memcpy(&status, &data[UBX_HEAD_SIZE], sizeof(status));
                if (state.haveUptime < CollQual::UBX) {
                    state.haveUptime = CollQual::UBX;
                    coll.uptime.SetNSec((uint64_t)status.msss * (uint64_t)1000000);
                }

                // Fix info
                FixType fixType = FixType::UNKNOWN;
                if (state.haveFixType < CollQual::UBX) {
                    state.haveFixType = CollQual::UBX;
                    switch (status.gpsFix) {  // clang-format off
                        case UBX_NAV_STATUS_V0_FIXTYPE_NOFIX:  fixType = FixType::NOFIX;     break;
                        case UBX_NAV_STATUS_V0_FIXTYPE_DRONLY: fixType = FixType::DRONLY;    break;
                        case UBX_NAV_STATUS_V0_FIXTYPE_2D:     fixType = FixType::SPP_2D;    break;
                        case UBX_NAV_STATUS_V0_FIXTYPE_3D:     fixType = FixType::SPP_3D;    break;
                        case UBX_NAV_STATUS_V0_FIXTYPE_3D_DR:  fixType = FixType::SPP_3D_DR; break;
                        case UBX_NAV_STATUS_V0_FIXTYPE_TIME:   fixType = FixType::TIME;      break;
                    }  // clang-format on
                    if (fixType > FixType::NOFIX) {
                        coll.fixOk = UBX_NAV_STATUS_V0_FLAGS_GPSFIXOK(status.flags);
                    }

                    if (UBX_NAV_STATUS_V0_FIXSTAT_CARRSOLNVALID(status.flags)) {
                        const uint8_t carrSoln = UBX_NAV_STATUS_V0_FLAGS2_CARRSOLN(status.flags2);
                        switch (carrSoln) {
                            case UBX_NAV_STATUS_V0_FLAGS2_CARRSOLN_FLOAT:
                                fixType = (fixType == FixType::SPP_3D_DR ? FixType::RTK_FLOAT_DR : FixType::RTK_FLOAT);
                                break;
                            case UBX_NAV_STATUS_V0_FLAGS2_CARRSOLN_FIXED:
                                fixType = (fixType == FixType::SPP_3D_DR ? FixType::RTK_FIXED_DR : FixType::RTK_FIXED);
                                break;
                        }
                    }
                    coll.fixType = fixType;
                }
            }
            break;

        case UBX_NAV_CLOCK_MSGID:
            if (size == UBX_NAV_CLOCK_V0_SIZE) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());
                UBX_NAV_CLOCK_V0_GROUP0 clock;
                std::memcpy(&clock, &data[UBX_HEAD_SIZE], sizeof(clock));
                if (state.haveClock < CollQual::UBX) {
                    state.haveClock = CollQual::UBX;
                    coll.clockBias = (double)clock.clkB * UBX_NAV_CLOCK_V0_CLKB_SCALE;
                    coll.clockDrift = (double)clock.clkD * UBX_NAV_CLOCK_V0_CLKD_SCALE;
                }
            }
            break;

        case UBX_NAV_RELPOSNED_MSGID:
            if ((size == UBX_NAV_RELPOSNED_V1_SIZE) &&
                (UBX_NAV_RELPOSNED_VERSION(data) == UBX_NAV_RELPOSNED_V1_VERSION)) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());
                UBX_NAV_RELPOSNED_V1_GROUP0 rel;
                std::memcpy(&rel, &data[UBX_HEAD_SIZE], sizeof(rel));
                if (UBX_NAV_RELPOSNED_V1_FLAGS_RELPOSVALID(rel.flags) && (state.haveRelPos < CollQual::UBX_HP)) {
                    state.haveRelPos = CollQual::UBX_HP;
                    coll.relPosNed[0] = (rel.relPosN * UBX_NAV_RELPOSNED_V1_RELPOSN_E_D_SCALE) +
                                        (rel.relPosHPN * UBX_NAV_RELPOSNED_V1_RELPOSHPN_E_D_SCALE);
                    coll.relPosNed[1] = (rel.relPosE * UBX_NAV_RELPOSNED_V1_RELPOSN_E_D_SCALE) +
                                        (rel.relPosHPE * UBX_NAV_RELPOSNED_V1_RELPOSHPN_E_D_SCALE);
                    coll.relPosNed[2] = (rel.relPosD * UBX_NAV_RELPOSNED_V1_RELPOSN_E_D_SCALE) +
                                        (rel.relPosHPD * UBX_NAV_RELPOSNED_V1_RELPOSHPN_E_D_SCALE);
                    coll.relPosAcc[0] = rel.accN * UBX_NAV_RELPOSNED_V1_ACCN_E_D_SCALE;
                    coll.relPosAcc[1] = rel.accE * UBX_NAV_RELPOSNED_V1_ACCN_E_D_SCALE;
                    coll.relPosAcc[2] = rel.accD * UBX_NAV_RELPOSNED_V1_ACCN_E_D_SCALE;
                    coll.relPosLen = (rel.relPosLength * UBX_NAV_RELPOSNED_V1_RELPOSLENGTH_SCALE) +
                                     (rel.relPosHPLength * UBX_NAV_RELPOSNED_V1_RELPOSHPLENGTH_SCALE);
                }
                state.relPosValid = UBX_NAV_RELPOSNED_V1_FLAGS_RELPOSVALID(rel.flags);
            }
            break;

        case UBX_NAV_SIG_MSGID:
            if ((size >= UBX_NAV_SIG_V0_MIN_SIZE) && (UBX_NAV_SIG_VERSION(data) == UBX_NAV_SIG_V0_VERSION)) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());
                if (state.haveSigs < CollQual::UBX) {
                    state.haveSigs = CollQual::UBX;
                    UBX_NAV_SIG_V0_GROUP0 head;
                    std::memcpy(&head, &data[UBX_HEAD_SIZE], sizeof(head));
                    for (int ix = 0; ix < (int)head.numSigs; ix++) {
                        UBX_NAV_SIG_V0_GROUP1 sig;
                        std::memcpy(&sig, &data[UBX_HEAD_SIZE + sizeof(UBX_NAV_SIG_V0_GROUP0) + (ix * sizeof(sig))],
                            sizeof(sig));

                        SigInfo sigInfo;

                        sigInfo.satSig = { UbxGnssIdSvIdToSat(sig.gnssId, sig.svId),
                            UbxGnssIdSigIdToSignal(sig.gnssId, sig.sigId) };

                        sigInfo.gloFcn = (int)sig.freqId - 7;
                        sigInfo.prRes = (float)sig.prRes * (float)UBX_NAV_SIG_V0_PRRES_SCALE;
                        sigInfo.cno = sig.cno;

                        sigInfo.use = ubxSigUse(sig.qualityInd);
                        sigInfo.corr = ubxSigCorrSource(sig.corrSource);
                        sigInfo.iono = ubxIonoModel(sig.ionoModel);
                        sigInfo.health = ubxSigHealth(UBX_NAV_SIG_V0_SIGFLAGS_HEALTH(sig.sigFlags));

                        sigInfo.prUsed = UBX_NAV_SIG_V0_SIGFLAGS_PR_USED(sig.sigFlags);
                        sigInfo.crUsed = UBX_NAV_SIG_V0_SIGFLAGS_CR_USED(sig.sigFlags);
                        sigInfo.doUsed = UBX_NAV_SIG_V0_SIGFLAGS_DO_USED(sig.sigFlags);
                        sigInfo.prCorrUsed = UBX_NAV_SIG_V0_SIGFLAGS_PR_CORR_USED(sig.sigFlags);
                        sigInfo.crCorrUsed = UBX_NAV_SIG_V0_SIGFLAGS_CR_CORR_USED(sig.sigFlags);
                        sigInfo.doCorrUsed = UBX_NAV_SIG_V0_SIGFLAGS_DO_CORR_USED(sig.sigFlags);

                        coll.sigs.push_back(sigInfo);
                    }
                }
            }
            break;

        case UBX_NAV_SAT_MSGID:
            if ((size >= UBX_NAV_SAT_V1_MIN_SIZE) && (UBX_NAV_SAT_VERSION(data) == UBX_NAV_SAT_V1_VERSION)) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());
                if (state.haveSats < CollQual::UBX) {
                    state.haveSats = CollQual::UBX;
                    UBX_NAV_SAT_V1_GROUP0 head;
                    std::memcpy(&head, &data[UBX_HEAD_SIZE], sizeof(head));

                    for (int ix = 0; ix < (int)head.numSvs; ix++) {
                        UBX_NAV_SAT_V1_GROUP1 sat;
                        std::memcpy(&sat, &data[UBX_HEAD_SIZE + sizeof(UBX_NAV_SAT_V1_GROUP0) + (ix * sizeof(sat))],
                            sizeof(sat));
                        SatInfo satInfo;

                        satInfo.sat = UbxGnssIdSvIdToSat(sat.gnssId, sat.svId);

                        const int orbSrc = UBX_NAV_SAT_V1_FLAGS_ORBITSOURCE(sat.flags);
                        satInfo.orbUsed = ubxSatOrb(orbSrc);
                        if (UBX_NAV_SAT_V1_FLAGS_EPHAVAIL(sat.flags)) {
                            SetBits(satInfo.orbAvail, Bit<int>(EnumToVal(SatOrb::EPH)));
                        }
                        if (UBX_NAV_SAT_V1_FLAGS_ALMAVAIL(sat.flags)) {
                            SetBits(satInfo.orbAvail, Bit<int>(EnumToVal(SatOrb::ALM)));
                        }
                        if (UBX_NAV_SAT_V1_FLAGS_ANOAVAIL(sat.flags) || UBX_NAV_SAT_V1_FLAGS_AOPAVAIL(sat.flags)) {
                            SetBits(satInfo.orbAvail, Bit<int>(EnumToVal(SatOrb::PRED)));
                        }
                        satInfo.azim = sat.azim;
                        satInfo.elev = sat.elev;

                        coll.sats.push_back(satInfo);
                    }
                }
            }
            break;
    }
}

static SigUse ubxSigUse(const uint8_t qualityInd)
{
    switch (qualityInd) {  // clang-format off
        case UBX_NAV_SIG_V0_QUALITYIND_SEARCH:    return SigUse::SEARCH;
        case UBX_NAV_SIG_V0_QUALITYIND_ACQUIRED:  return SigUse::ACQUIRED;
        case UBX_NAV_SIG_V0_QUALITYIND_UNUSED:    return SigUse::UNUSABLE;
        case UBX_NAV_SIG_V0_QUALITYIND_CODELOCK:  return SigUse::CODELOCK;
        case UBX_NAV_SIG_V0_QUALITYIND_CARRLOCK1:
        case UBX_NAV_SIG_V0_QUALITYIND_CARRLOCK2:
        case UBX_NAV_SIG_V0_QUALITYIND_CARRLOCK3: return SigUse::CARRLOCK;
        case UBX_NAV_SIG_V0_QUALITYIND_NOSIG:     return SigUse::NONE;
    }  // clang-format on
    return SigUse::UNKNOWN;
}

static SigCorr ubxSigCorrSource(const uint8_t corrSource)
{
    switch (corrSource) {  // clang-format off
        case UBX_NAV_SIG_V0_CORRSOURCE_NONE:      return SigCorr::NONE;
        case UBX_NAV_SIG_V0_CORRSOURCE_SBAS:      return SigCorr::SBAS;
        case UBX_NAV_SIG_V0_CORRSOURCE_BDS:       return SigCorr::BDS;
        case UBX_NAV_SIG_V0_CORRSOURCE_RTCM2:     return SigCorr::RTCM2;
        case UBX_NAV_SIG_V0_CORRSOURCE_RTCM3_OSR: return SigCorr::RTCM3_OSR;
        case UBX_NAV_SIG_V0_CORRSOURCE_RTCM3_SSR: return SigCorr::RTCM3_SSR;
        case UBX_NAV_SIG_V0_CORRSOURCE_QZSS_SLAS: return SigCorr::QZSS_SLAS;
        case UBX_NAV_SIG_V0_CORRSOURCE_SPARTN:    return SigCorr::SPARTN;
    }  // clang-format on
    return SigCorr::UNKNOWN;
}

static SigIono ubxIonoModel(const uint8_t ionoModel)
{
    switch (ionoModel) {  // clang-format off
        case UBX_NAV_SIG_V0_IONOMODEL_NONE:     return SigIono::NONE;
        case UBX_NAV_SIG_V0_IONOMODEL_KLOB_GPS: return SigIono::KLOB_GPS;
        case UBX_NAV_SIG_V0_IONOMODEL_KLOB_BDS: return SigIono::KLOB_BDS;
        case UBX_NAV_SIG_V0_IONOMODEL_SBAS:     return SigIono::SBAS;
        case UBX_NAV_SIG_V0_IONOMODEL_DUALFREQ: return SigIono::DUAL_FREQ;
    }  // clang-format on
    return SigIono::UNKNOWN;
}

static SigHealth ubxSigHealth(const uint8_t health)
{
    switch (health) {  // clang-format off
        case UBX_NAV_SIG_V0_SIGFLAGS_HEALTH_HEALTHY:   return SigHealth::HEALTHY;
        case UBX_NAV_SIG_V0_SIGFLAGS_HEALTH_UNHEALTHY: return SigHealth::UNHEALTHY;
        case UBX_NAV_SIG_V0_SIGFLAGS_HEALTH_UNKNO:     break;
    }  // clang-format on
    return SigHealth::UNKNOWN;
}

static SatOrb ubxSatOrb(const uint8_t orbSrc)
{
    switch (orbSrc) {  // clang-format off
        case UBX_NAV_SAT_V1_FLAGS_ORBITSOURCE_NONE:   return SatOrb::NONE;
        case UBX_NAV_SAT_V1_FLAGS_ORBITSOURCE_EPH:    return SatOrb::EPH;
        case UBX_NAV_SAT_V1_FLAGS_ORBITSOURCE_ALM:    return SatOrb::ALM;
        case UBX_NAV_SAT_V1_FLAGS_ORBITSOURCE_ANO:    /* FALLTHROUGH */
        case UBX_NAV_SAT_V1_FLAGS_ORBITSOURCE_ANA:    return SatOrb::PRED;
        case UBX_NAV_SAT_V1_FLAGS_ORBITSOURCE_OTHER1: /* FALLTHROUGH */
        case UBX_NAV_SAT_V1_FLAGS_ORBITSOURCE_OTHER2: /* FALLTHROUGH */
        case UBX_NAV_SAT_V1_FLAGS_ORBITSOURCE_OTHER3: return SatOrb::OTHER;
    }  // clang-format on
    return SatOrb::UNKNOWN;
}

// ---------------------------------------------------------------------------------------------------------------------

static void CollectNmea(Epoch& coll, CollState& state, const ParserMsg& msg, const NmeaPayloadPtr& nmea)
{
    UNUSED(msg); // debug disabled or release build
    switch (nmea->formatter_) {
        case NmeaFormatter::GGA: {
            const auto& gga = dynamic_cast<const NmeaGgaPayload&>(*nmea);
            EPOCH_TRACE("collect %s", msg.name_.c_str());
            if (gga.time.valid && (state.haveHms < CollQual::NMEA)) {
                state.haveHms = CollQual::NMEA;
                state.hour = gga.time.hours;
                state.minute = gga.time.mins;
                state.second = gga.time.secs;
                state.leapsKnown = true;  // we can only hope...
            }
            if ((gga.quality != NmeaQualityGga::UNSPECIFIED) && (state.haveFixType < CollQual::NMEA)) {
                state.haveFixType = CollQual::NMEA;
                coll.fixType = NmeaQualityGgaToFixType(gga.quality);
                coll.fixOk = true;
            }
            if ((gga.quality > NmeaQualityGga::NOFIX) && gga.llh.latlon_valid && gga.llh.height_valid &&
                (state.havePosLlh < CollQual::NMEA)) {
                state.havePosLlh = CollQual::NMEA;
                coll.posLlh[0] = DegToRad(gga.llh.lat);
                coll.posLlh[1] = DegToRad(gga.llh.lon);
                coll.posLlh[2] = gga.llh.height;
                coll.heightMsl = gga.height_msl.value;
                coll.haveHeightMsl = gga.height_msl.valid;
            }
            if (gga.diff_age.valid && (state.haveDiffAge < CollQual::NMEA)) {
                state.haveDiffAge = CollQual::NMEA;
                coll.diffAge = gga.diff_age.value;
                coll.haveDiffAge = true;
            }
            if (!gga.num_sv.valid && (state.haveNumSatUsed < CollQual::NMEA)) {
                state.haveNumSatUsed = CollQual::NMEA;
                coll.numSatUsed.numTotal = gga.num_sv.value;
            }
            break;
        }
        case NmeaFormatter::RMC: {
            EPOCH_TRACE("collect %s", msg.name_.c_str());
            const auto& rmc = dynamic_cast<const NmeaRmcPayload&>(*nmea);
            if (rmc.time.valid && (state.haveHms < CollQual::NMEA)) {
                state.haveHms = CollQual::NMEA;
                state.hour = rmc.time.hours;
                state.minute = rmc.time.mins;
                state.second = rmc.time.secs;
                state.leapsKnown = true;  // we can only hope...
            }
            if (rmc.date.valid && (state.haveYmd < CollQual::NMEA)) {
                state.haveYmd = CollQual::NMEA;
                state.day = rmc.date.days;
                state.month = rmc.date.months;
                state.year = rmc.date.years;
            }
            FixType fixType = FixType::UNKNOWN;
            if ((rmc.mode != NmeaModeRmcGns::UNSPECIFIED) && (state.haveFixType < CollQual::NMEA)) {
                state.haveFixType = CollQual::NMEA;
                fixType = NmeaModeRmcGnsToFixType(rmc.mode);
                coll.fixType = fixType;
                coll.fixOk = (rmc.status == NmeaStatusGllRmc::VALID);
            }
            if ((fixType > FixType::NOFIX) && rmc.ll.latlon_valid && rmc.ll.height_valid &&
                (state.havePosLl < CollQual::NMEA)) {
                state.havePosLl = CollQual::NMEA;
                coll.posLlh[0] = DegToRad(rmc.ll.lat);
                coll.posLlh[1] = DegToRad(rmc.ll.lon);
            }
            break;
        }
        case NmeaFormatter::GLL: {
            EPOCH_TRACE("collect %s", msg.name_.c_str());
            const auto& gll = dynamic_cast<const NmeaGllPayload&>(*nmea);
            if (gll.time.valid && (state.haveHms < CollQual::NMEA)) {
                state.haveHms = CollQual::NMEA;
                state.hour = gll.time.hours;
                state.minute = gll.time.mins;
                state.second = gll.time.secs;
                state.leapsKnown = true;  // we can only hope...
            }
            FixType fixType = FixType::UNKNOWN;
            if ((gll.mode != NmeaModeGllVtg::UNSPECIFIED) && (state.haveFixType < CollQual::NMEA)) {
                state.haveFixType = CollQual::NMEA;
                fixType = NmeaModeGllVtgToFixType(gll.mode);
                coll.fixType = fixType;
                coll.fixOk = (gll.status == NmeaStatusGllRmc::VALID);
            }
            if ((fixType > FixType::NOFIX) && gll.ll.latlon_valid && gll.ll.height_valid &&
                (state.havePosLl < CollQual::NMEA)) {
                state.havePosLl = CollQual::NMEA;
                coll.posLlh[0] = DegToRad(gll.ll.lat);
                coll.posLlh[1] = DegToRad(gll.ll.lon);
            }
            break;
        }
        case NmeaFormatter::GSV: {
            EPOCH_TRACE("collect %s", msg.name_.c_str());
            const auto& gsv = dynamic_cast<const NmeaGsvPayload&>(*nmea);
            state.gsvMsgs.push_back(gsv);
            break;
        }
        case NmeaFormatter::GSA: {
            EPOCH_TRACE("collect %s", msg.name_.c_str());
            const auto& gsa = dynamic_cast<const NmeaGsaPayload&>(*nmea);
            state.gsaMsgs.push_back(gsa);
            break;
        }
        case NmeaFormatter::VTG:
        case NmeaFormatter::GST:
        case NmeaFormatter::HDT:
        case NmeaFormatter::ZDA:
        case NmeaFormatter::UNSPECIFIED:
            break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

static void Complete(Epoch& coll, CollState& state)
{
    UNUSED(CollQualStr); // debug disabled or release build

    coll.valid = true;

    // Convert stuff, prefer better quality, FIXME: this assumes WGS84...
    // llh2xyz
    if (state.havePosLlh > state.havePosXyz) {
        EPOCH_TRACE("complete: llh (%s) -> xyz (%s)", CollQualStr(state.havePosLlh), CollQualStr(state.havePosXyz));
        Eigen::Map<Eigen::Vector3d> pos(coll.posXyz.data());
        pos = TfEcefWgs84Llh(Eigen::Map<Eigen::Vector3d>(coll.posLlh.data()));
        coll.havePos = (coll.fixType > FixType::NOFIX);
    }
    // xyz2llh
    else if (state.havePosXyz > state.havePosLlh) {
        EPOCH_TRACE("complete: xyz (%s) -> llh (%s)", CollQualStr(state.havePosXyz), CollQualStr(state.havePosLlh));
        Eigen::Map<Eigen::Vector3d> pos(coll.posLlh.data());
        pos = TfWgs84LlhEcef(Eigen::Map<Eigen::Vector3d>(coll.posXyz.data()));
        coll.havePos = (coll.fixType > FixType::NOFIX);
    }
    // both (un)available equally
    else {
        EPOCH_TRACE("complete: llh (%s) = xyz (%s)", CollQualStr(state.havePosLlh), CollQualStr(state.havePosXyz));
        coll.havePos = (state.havePosLlh > CollQual::NOTHING);
    }
    if (coll.havePos) {
        coll.posDmsLat = { RadToDeg(coll.posLlh[0]) };
        coll.posDmsLon = { RadToDeg(coll.posLlh[1]) };
    }
    // TODO: how to handle only havePosLl (no height)?

    if ((state.havePosAccHoriz > CollQual::NOTHING) && (state.havePosAccVert > CollQual::NOTHING) &&
        (state.havePosAcc < CollQual::UBX)) {
        coll.posAcc = std::sqrt((coll.posAccHoriz * coll.posAccHoriz) + (coll.posAccVert * coll.posAccVert));
        state.havePosAcc = std::min(state.havePosAccHoriz, state.havePosAccVert);
    }

    coll.havePosAcc = (state.havePosAcc > CollQual::NOTHING);
    coll.havePosAccHoriz = (state.havePosAccHoriz > CollQual::NOTHING);
    coll.havePosAccVert = (state.havePosAccVert > CollQual::NOTHING);

    coll.haveFixType = (state.haveFixType > CollQual::NOTHING);
    coll.fixTypeStr = FixTypeStr(coll.fixType);

    // Sometimes fix is still RTK_FIXED/FLOAT but UBX-NAV-RELPOSNED.relPosValid indicates that the relative (RTK)
    // position is no longer available. Don't trust that fix.
    if ((state.haveRelPos > CollQual::NOTHING) && (coll.fixType >= FixType::RTK_FLOAT) && !state.relPosValid) {
        coll.fixOk = false;
    }

    if (state.haveVelNed > CollQual::NOTHING) {
        coll.haveVel = (coll.fixType > FixType::NOFIX);
        const double velNEsq = (coll.velNed[0] * coll.velNed[0]) + (coll.velNed[1] * coll.velNed[1]);
        coll.vel2d = std::sqrt(velNEsq);
        coll.vel3d = std::sqrt(velNEsq + (coll.velNed[2] * coll.velNed[2]));
    }

    // Prefer wno/tow over date/time
    if ((state.haveGpsWno > state.haveYmd) && (state.haveGpsTow > state.haveHms)) {
        coll.time.SetWnoTow({ coll.gpsWno, coll.gpsTow, WnoTow::Sys::GPS });
    } else if ((state.haveHms > CollQual::NOTHING) && (state.haveYmd > CollQual::NOTHING)) {
        coll.time.SetUtcTime({ state.year, state.month, state.day, state.hour, state.minute, state.second });
    }
    if (!coll.time.IsZero()) {
        coll.timeGpsWnoTow = coll.time.GetWnoTow();
        coll.timeUtc = coll.time.GetUtcTime(3);
        coll.haveTime = true;
    }
    coll.confTime = (state.confHms && state.confYmd);
    coll.leapsKnown = state.leapsKnown;
    coll.haveGpsWno = (state.haveGpsWno > CollQual::NOTHING);
    coll.haveGpsTow = (state.haveGpsTow > CollQual::NOTHING);

    coll.haveTimeAcc = (state.haveTimeAcc > CollQual::NOTHING);

    coll.haveClock = (state.haveClock > CollQual::NOTHING);

    coll.haveRelPos = (state.haveRelPos > CollQual::NOTHING);

    // Fall back to NMEA GSA/GSV
    if ((state.haveSats == CollQual::NOTHING) || (state.haveSigs == CollQual::NOTHING)) {
        EPOCH_TRACE("sat/sig from nmea gsa=%" PRIuMAX " gsv=%" PRIuMAX, state.gsaMsgs.size(), state.gsvMsgs.size());
        NmeaCollectGsaGsv gsaGsvColl;
        if (gsaGsvColl.AddGsaAndGsv(state.gsaMsgs, state.gsvMsgs)) {
            if (state.haveSats == CollQual::NOTHING) {
                for (const auto& sat : gsaGsvColl.sats_) {
                    SatInfo satInfo;
                    satInfo.sat = NmeaSystemIdSvIdToSat(sat.system_, sat.svid_);
                    satInfo.azim = sat.az_;
                    satInfo.elev = sat.el_;
                    satInfo.orbUsed = SatOrb::EPH;                                    // presumably..
                    SetBits(satInfo.orbAvail, Bit<int>(EnumToVal(SatOrb::EPH)));  // presumably..
                    coll.sats.push_back(satInfo);
                }
                state.haveSats = CollQual::NMEA;
            }
            if (state.haveSigs == CollQual::NOTHING) {
                for (const auto& sig : gsaGsvColl.sigs_) {
                    SigInfo sigInfo;
                    sigInfo.satSig = { NmeaSystemIdSvIdToSat(sig.system_, sig.svid_),
                        NmeaSignalIdToSignal(sig.signal_) };
                    sigInfo.cno = sig.cno_;
                    sigInfo.use = (sig.used_ ? SigUse::CODELOCK : (sig.cno_ > 0.0 ? SigUse::ACQUIRED : SigUse::SEARCH));
                    sigInfo.health = SigHealth::HEALTHY;  // presumably...
                    sigInfo.prUsed = true;                // presumably...
                    coll.sigs.push_back(sigInfo);
                }
                state.haveSigs = CollQual::NMEA;
            }
        }
    }

    std::sort(coll.sats.begin(), coll.sats.end());
    int ix = 0;
    for (auto& satInfo : coll.sats) {
        satInfo.ix = ix++;

        satInfo.gnss = satInfo.sat.GetGnss();
        satInfo.svNr = satInfo.sat.GetSvNr();

        satInfo.gnssStr = GnssStr(satInfo.gnss);
        satInfo.satStr = satInfo.sat.GetStr();
        satInfo.orbUsedStr = SatOrbStr(satInfo.orbUsed);
    }

    std::sort(coll.sigs.begin(), coll.sigs.end());
    ix = 0;
    const char* prevSatStr = nullptr;
    for (auto& sigInfo : coll.sigs) {
        sigInfo.ix = ix++;

        sigInfo.sat = sigInfo.satSig.GetSat();
        sigInfo.gnss = sigInfo.sat.GetGnss();
        sigInfo.svNr = sigInfo.sat.GetSvNr();
        sigInfo.signal = sigInfo.satSig.GetSignal();
        sigInfo.band = sigInfo.satSig.GetBand();

        sigInfo.gnssStr = GnssStr(sigInfo.gnss);
        sigInfo.satStr = sigInfo.satSig.GetSat().GetStr();
        sigInfo.signalStr = SignalStr(sigInfo.signal);
        sigInfo.signalStrShort = SignalStr(sigInfo.signal, true);
        sigInfo.bandStr = BandStr(sigInfo.band);
        sigInfo.useStr = SigUseStr(sigInfo.use);
        sigInfo.corrStr = SigCorrStr(sigInfo.corr);
        sigInfo.ionoStr = SigIonoStr(sigInfo.iono);
        sigInfo.healthStr = SigHealthStr(sigInfo.health);

        sigInfo.anyUsed = (sigInfo.prUsed || sigInfo.crUsed || sigInfo.doUsed);

        if (sigInfo.use >= SigUse::ACQUIRED) {
            const int histIx = CnoToIx(sigInfo.cno);
            coll.sigCnoHistTrk[histIx]++;
            coll.haveSigCnoHist = true;

            if (sigInfo.anyUsed) {
                coll.sigCnoHistNav[histIx]++;
                switch (sigInfo.gnss) {  // clang-format off
                    case Gnss::GPS:   coll.numSigUsed.numGps++;   break;
                    case Gnss::SBAS:  coll.numSigUsed.numSbas++;  break;
                    case Gnss::GAL:   coll.numSigUsed.numGal++;   break;
                    case Gnss::BDS:   coll.numSigUsed.numBds++;   break;
                    case Gnss::QZSS:  coll.numSigUsed.numQzss++;  break;
                    case Gnss::GLO:   coll.numSigUsed.numGlo++;   break;
                    case Gnss::NAVIC: coll.numSigUsed.numNavic++; break;
                    case Gnss::UNKNOWN: break;
                }  // clang-format on

                if (/*std::strcmp(prevSatStr, sigInfo.satStr) != 0*/ prevSatStr != sigInfo.satStr) {
                    switch (sigInfo.gnss) {  // clang-format off
                        case Gnss::GPS:   coll.numSatUsed.numGps++;   break;
                        case Gnss::SBAS:  coll.numSatUsed.numSbas++;  break;
                        case Gnss::GAL:   coll.numSatUsed.numGal++;   break;
                        case Gnss::BDS:   coll.numSatUsed.numBds++;   break;
                        case Gnss::QZSS:  coll.numSatUsed.numQzss++;  break;
                        case Gnss::GLO:   coll.numSatUsed.numGlo++;   break;
                        case Gnss::NAVIC: coll.numSatUsed.numNavic++; break;
                        case Gnss::UNKNOWN: break;
                    }  // clang-format on
                }
                prevSatStr = sigInfo.satStr;
            }
        }
    }
    const int numSigUsedTotal = coll.numSigUsed.numGps + coll.numSigUsed.numGlo + coll.numSigUsed.numGal +
                                coll.numSigUsed.numBds + coll.numSigUsed.numSbas + coll.numSigUsed.numQzss +
                                coll.numSigUsed.numNavic;
    if (numSigUsedTotal > 0) {
        coll.numSigUsed.numTotal = numSigUsedTotal;
        coll.haveNumSigUsed = true;
    }
    const int numSatUsedTotal = coll.numSatUsed.numGps + coll.numSatUsed.numGlo + coll.numSatUsed.numGal +
                                coll.numSatUsed.numBds + coll.numSatUsed.numSbas + coll.numSatUsed.numQzss +
                                coll.numSatUsed.numNavic;
    if (numSatUsedTotal > 0) {
        coll.numSatUsed.numTotal = numSatUsedTotal;
        coll.haveNumSatUsed = true;
    }

    // Populate lookup table for signal <--> satellite
    auto satsBegin = coll.sats.begin();
    for (auto& sigInfo : coll.sigs) {
        satsBegin =
            std::find_if(satsBegin, coll.sats.end(), [&sigInfo](const auto& cand) { return cand.sat == sigInfo.sat; });
        // TODO: handle INVALID_SAT / INVALID_SIG
        if (satsBegin != coll.sats.end()) {
            sigInfo.satIx = satsBegin->ix;  // clang-format off
            if      (satsBegin->sigIxs[0] < 0) { satsBegin->sigIxs[0] = sigInfo.ix; }
            else if (satsBegin->sigIxs[1] < 0) { satsBegin->sigIxs[1] = sigInfo.ix; }
            else if (satsBegin->sigIxs[2] < 0) { satsBegin->sigIxs[2] = sigInfo.ix; }
            else if (satsBegin->sigIxs[3] < 0) { satsBegin->sigIxs[3] = sigInfo.ix; }  // clang-format on
        }
    }

    // for (const auto& satInfo : coll.sats) {
    //     EPOCH_TRACE("sat[%2d] %s -> sig[%d, %d, %d, %d]", satInfo.ix, satInfo.satStr, satInfo.sigIxs[0],
    //         satInfo.sigIxs[1], satInfo.sigIxs[2], satInfo.sigIxs[3]);
    // }
    // for (const auto& sigInfo : coll.sigs) {
    //     EPOCH_TRACE("sig[%2d] %s %s -> sat[%d]", sigInfo.ix, sigInfo.satStr, sigInfo.signalStr, sigInfo.satIx);
    // }

    if (!coll.uptime.IsZero()) {
        std::snprintf(coll.uptimeStr, sizeof(coll.uptimeStr), "%s", coll.uptime.Stringify(0).c_str());
    }

    std::snprintf(coll.str, sizeof(coll.str), "%-12s %s (%c) %+11.7f %+12.7f (%5.1f) %+5.0f (%5.1f) %4.1f",
        coll.fixTypeStr, coll.time.StrUtcTime().c_str(), coll.haveTime ? (coll.confTime ? 'Y' : 'y') : 'n',
        RadToDeg(coll.posLlh[0]), RadToDeg(coll.posLlh[1]), coll.posAccHoriz, coll.posLlh[2], coll.posAccVert,
        coll.pDOP);
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
