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
#include <fpsdk_common/parser/sbf.hpp>
#include <fpsdk_common/parser/ubx.hpp>
#include <fpsdk_common/parser/rtcm3.hpp>
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
using namespace fpsdk::common::parser::sbf;
using namespace fpsdk::common::parser::rtcm3;
using namespace fpsdk::common::string;
using namespace fpsdk::common::trafo;
using namespace fpsdk::common::types;

#if 0
#  define EPOCH_TRACE(fmt, args...) TRACE("epoch: " fmt, ##args)
#else
#  define EPOCH_TRACE(...) /* nothing */
#endif

// ---------------------------------------------------------------------------------------------------------------------

// Persistent data
struct CollPersist  // clang-format off
{
    std::size_t  seq           = 0;

    // UBX, SBF
    uint32_t     towMs         = 0;
    bool         haveTowMs     = false;

    // NMEA
    int          secMs         = -1;
    bool         haveSecMs     = false;
};  // clang-format on

// "Quality" (precision) of collected information
enum class CollQual : int
{
    NONE = 0,
    LOW,   // e.g. NMEA-Gx-GGA
    MED,   // e.g. UBX-NAV-POSECEF
    HIGH,  // e.g. UBX-NAV-HPPOSECEF
};

#ifndef NDEBUG
static const char* CollQualStr(const CollQual qual)
{
    switch (qual) {  // clang-format off
        case CollQual::NONE: return "NONE";
        case CollQual::LOW:  return "LOW";
        case CollQual::MED:  return "MED";
        case CollQual::HIGH: return "HIGH";
    }  // clang-format on
    return "?";
}
#endif

// Per-epoch collection state
struct CollState  // clang-format off
{
    CollQual haveFixType     = CollQual::NONE;

    CollQual havePosLlh      = CollQual::NONE;
    CollQual havePosLl       = CollQual::NONE;
    CollQual havePosXyz      = CollQual::NONE;
    CollQual haveRelPos      = CollQual::NONE;

    CollQual havePosAcc      = CollQual::NONE;
    CollQual havePosAccHoriz = CollQual::NONE;
    CollQual havePosAccVert  = CollQual::NONE;

    CollQual haveVelNed      = CollQual::NONE;
    CollQual haveVelAcc      = CollQual::NONE;

    CollQual haveSigs        = CollQual::NONE;
    CollQual haveSats        = CollQual::NONE;
    CollQual haveGpsTow      = CollQual::NONE;
    CollQual haveGpsWno      = CollQual::NONE;
    CollQual haveDiffAge     = CollQual::NONE;

    CollQual haveNumSatUsed  = CollQual::NONE;

    CollQual haveClock       = CollQual::NONE;
    CollQual haveUptime      = CollQual::NONE;
    CollQual haveTimeAcc     = CollQual::NONE;

    CollQual haveHms         = CollQual::NONE;
    bool     confHms         = false; // hour, minute, second are confirmed valid
    bool     leapsKnown      = false;
    int      hour            = 0;
    int      minute          = 0;
    double   second          = 0.0;

    CollQual haveYmd         = CollQual::NONE;
    bool     confYmd         = false; // day, month, year are confirmed valid
    int      day             = 0;
    int      month           = 0;
    int      year            = 0;

    std::vector<NmeaGsvPayload> gsvMsgs;
    std::vector<NmeaGsaPayload> gsaMsgs;

    bool     relPosValid     = false;

    std::vector<uint8_t> sbfChannelStatus;
    std::vector<uint8_t> sbfMeasEpoch;

    Time startTime;

    CollState()
    {
        startTime = Time::FromClockRealtime();
    }

};  // clang-format on

// ---------------------------------------------------------------------------------------------------------------------

EpochCollector::EpochCollector(const Mode mode) /* clang-format off */ :
    mode_   { mode }  // clang-format on
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
    persist_ = std::make_unique<CollPersist>();
    state_ = std::make_unique<CollState>();
}

// ---------------------------------------------------------------------------------------------------------------------

enum class Detect {
    NONE,   // Not an epoch marker message
    END,    // Message marks end of epoch, is included in currently collected epoch
    BEGIN,  // Message marks start of epoch, is not included in currently collecte epoch
};

static Detect DetectNmea(CollPersist& persist, const ParserMsg& msg, const NmeaPayloadPtr& nmea);
static Detect DetectUbx(CollPersist& persist, const ParserMsg& msg);
static Detect DetectSbf(CollPersist& persist, const ParserMsg& msg);
static Detect DetectRtcm3(CollPersist& persist, const ParserMsg& msg);

static void CollectNmea(Epoch& coll, CollState& state, const ParserMsg& msg, const NmeaPayloadPtr& nmea);
static void CollectUbx(Epoch& coll, CollState& state, const ParserMsg& msg);
static void CollectSbf(Epoch& coll, CollState& state, const ParserMsg& msg);
static void CollectRtcm3(Epoch& coll, CollState& state, const ParserMsg& msg);

static void Complete(CollPersist& persist, Epoch& coll, CollState& state);

EpochPtr EpochCollector::Collect(const fpsdk::common::parser::ParserMsg& msg, bool* msgPartOfEpoch)
{
    EPOCH_TRACE("collect %s", msg.name_.c_str());

    coll_->msgsTotal++;
    coll_->bytesTotal += msg.Size();

    // Decode NMEA only once, as it is quite expensive
    NmeaPayloadPtr nmea;

    // Detect end of epoch / start of next epoch
    Detect detect = Detect::NONE;
    switch (mode_) {
        case Mode::RECEIVER:
            switch (msg.proto_) {
                case Protocol::UBX:
                    detect = DetectUbx(*persist_, msg);
                    if (detect != Detect::NONE) {
                        persist_->haveSecMs = false;
                    }
                    break;
                case Protocol::SBF:
                    detect = DetectSbf(*persist_, msg);
                    if (detect != Detect::NONE) {
                        persist_->haveSecMs = false;
                    }
                    break;
                case Protocol::NMEA:
                    nmea = NmeaDecodeMessage(msg.Data(), msg.Size());
                    detect = DetectNmea(*persist_, msg, nmea);
                    if (detect != Detect::NONE) {
                        persist_->haveTowMs = false;
                    }
                    break;
                default:
                break;
            }
            break;
        case Mode::CORRECTIONS:
            switch (msg.proto_) {
                case Protocol::RTCM3:
                    detect = DetectRtcm3(*persist_, msg);
                    break;
                default:
                    break;
            }
            break;
    }

    // Output epoch
    EpochPtr epoch;

    // Message marks beginning of epoch -> output epoch first, then collect
    if (detect == Detect::BEGIN) {
        EPOCH_TRACE("epoch complete (BEGIN)");
        Complete(*persist_, *coll_, *state_);
        std::swap(epoch, coll_);
        coll_ = std::make_unique<Epoch>();
        state_ = std::make_unique<CollState>();
    }

    // Collect data
    switch (mode_) {
        case Mode::RECEIVER:
            switch (msg.proto_) {
                case Protocol::UBX:
                    CollectUbx(*coll_, *state_, msg);
                    break;
                case Protocol::SBF:
                    CollectSbf(*coll_, *state_, msg);
                    break;
                case Protocol::NMEA:
                    if (nmea) {
                        CollectNmea(*coll_, *state_, msg, nmea);
                    }
                    break;
                default:
                    break;
            }
            break;
        case Mode::CORRECTIONS:
            switch (msg.proto_) {
                case Protocol::RTCM3:
                    CollectRtcm3(*coll_, *state_, msg);
                    break;
                default:
                    break;
            }
            break;
    }

    // Message marks end of epoch -> collect first, then output
    if (detect == Detect::END) {
        EPOCH_TRACE("epoch complete (END)");
        Complete(*persist_, *coll_, *state_);
        std::swap(epoch, coll_);
        coll_ = std::make_unique<Epoch>();
        state_ = std::make_unique<CollState>();
    }

    if (msgPartOfEpoch) {
        *msgPartOfEpoch = (detect == Detect::END);
    }

    return epoch;
}


// ---------------------------------------------------------------------------------------------------------------------

static void sbfCompleteSatSig(Epoch& coll, CollState& state);

static void totalPerGnssCnt(PerGnssCnt& cnt)
{
    cnt.numTotal = cnt.numGps + cnt.numGlo + cnt.numGal + cnt.numBds + cnt.numSbas + cnt.numQzss + cnt.numNavic;
}

static void Complete(CollPersist& persist, Epoch& coll, CollState& state)
{
#ifndef NDEBUG
    (void)CollQualStr;
#endif

    persist.seq++;
    coll.seq = persist.seq;

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
        coll.havePos = (state.havePosLlh > CollQual::NONE);
    }
    if (coll.havePos) {
        coll.posDmsLat = { RadToDeg(coll.posLlh[0]) };
        coll.posDmsLon = { RadToDeg(coll.posLlh[1]) };
    }
    // TODO: how to handle only havePosLl (no height)?

    if ((state.havePosAccHoriz > CollQual::NONE) && (state.havePosAccVert > CollQual::NONE) &&
        (state.havePosAcc < CollQual::MED)) {
        coll.posAcc = std::sqrt((coll.posAccHoriz * coll.posAccHoriz) + (coll.posAccVert * coll.posAccVert));
        state.havePosAcc = std::min(state.havePosAccHoriz, state.havePosAccVert);
    }

    coll.havePosAcc = (state.havePosAcc > CollQual::NONE);
    coll.havePosAccHoriz = (state.havePosAccHoriz > CollQual::NONE);
    coll.havePosAccVert = (state.havePosAccVert > CollQual::NONE);

    coll.haveFixType = (state.haveFixType > CollQual::NONE);
    coll.fixTypeStr = FixTypeStr(coll.fixType);

    // Sometimes fix is still RTK_FIXED/FLOAT but UBX-NAV-RELPOSNED.relPosValid indicates that the relative (RTK)
    // position is no longer available. Don't trust that fix.
    if ((state.haveRelPos > CollQual::NONE) && (coll.fixType >= FixType::RTK_FLOAT) && !state.relPosValid) {
        coll.fixOk = false;
    }

    if (state.haveVelNed > CollQual::NONE) {
        coll.haveVel = (coll.fixType > FixType::NOFIX);
        const double velNEsq = (coll.velNed[0] * coll.velNed[0]) + (coll.velNed[1] * coll.velNed[1]);
        coll.vel2d = std::sqrt(velNEsq);
        coll.vel3d = std::sqrt(velNEsq + (coll.velNed[2] * coll.velNed[2]));
    }

    // Prefer wno/tow over date/time
    if ((state.haveGpsWno > state.haveYmd) && (state.haveGpsTow > state.haveHms)) {
        coll.time.SetWnoTow({ coll.gpsWno, coll.gpsTow, WnoTow::Sys::GPS });
    } else if ((state.haveHms > CollQual::NONE) && (state.haveYmd > CollQual::NONE)) {
        coll.time.SetUtcTime({ state.year, state.month, state.day, state.hour, state.minute, state.second });
    }
    if (!coll.time.IsZero()) {
        coll.timeGpsWnoTow = coll.time.GetWnoTow();
        coll.timeUtc = coll.time.GetUtcTime(3);
        coll.haveTime = true;
    }
    coll.confTime = (state.confHms && state.confYmd);
    coll.leapsKnown = state.leapsKnown;
    coll.haveGpsWno = (state.haveGpsWno > CollQual::NONE);
    coll.haveGpsTow = (state.haveGpsTow > CollQual::NONE);

    coll.haveTimeAcc = (state.haveTimeAcc > CollQual::NONE);

    coll.haveClock = (state.haveClock > CollQual::NONE);

    coll.haveRelPos = (state.haveRelPos > CollQual::NONE);

    // Maybe we have SBF-MEASEPOCH and/or SBF-CHANNELSTATUS
    if ((state.haveSats < CollQual::MED) && (state.haveSigs < CollQual::MED)) {
        sbfCompleteSatSig(coll, state);
    }

    // Use NMEA GSA/GSV if we don't have anything better
    if ((state.haveSats == CollQual::NONE) || (state.haveSigs == CollQual::NONE)) {
        EPOCH_TRACE("sat/sig from nmea gsa=%" PRIuMAX " gsv=%" PRIuMAX, state.gsaMsgs.size(), state.gsvMsgs.size());
        NmeaCollectGsaGsv gsaGsvColl;
        if (gsaGsvColl.AddGsaAndGsv(state.gsaMsgs, state.gsvMsgs)) {
            if (state.haveSats == CollQual::NONE) {
                for (const auto& sat : gsaGsvColl.sats_) {
                    SatInfo satInfo;
                    satInfo.sat = NmeaSystemIdSvIdToSat(sat.system_, sat.svid_);
                    satInfo.azim = sat.az_;
                    satInfo.elev = sat.el_;
                    satInfo.orbUsed = SatOrb::EPH;    // presumably..
                    satInfo.orbAvail |= SatOrb::EPH;  // presumably..
                    coll.sats.push_back(satInfo);
                }
                state.haveSats = CollQual::LOW;
            }
            if (state.haveSigs == CollQual::NONE) {
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
                state.haveSigs = CollQual::LOW;
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
                    case Gnss::GPS:   coll.numSigUsed.numGps++;   switch (sigInfo.band) { case Band::UNKNOWN: break;
                                                                                          case Band::L1: coll.numSigUsedL1.numGps++;   break;
                                                                                          case Band::E6: coll.numSigUsedE6.numGps++;   break;
                                                                                          case Band::L2: coll.numSigUsedL2.numGps++;   break;
                                                                                          case Band::L5: coll.numSigUsedL5.numGps++;   break; } break;
                    case Gnss::SBAS:  coll.numSigUsed.numSbas++;  switch (sigInfo.band) { case Band::UNKNOWN: break;
                                                                                          case Band::L1: coll.numSigUsedL1.numSbas++;  break;
                                                                                          case Band::E6: coll.numSigUsedE6.numSbas++;  break;
                                                                                          case Band::L2: coll.numSigUsedL2.numSbas++;  break;
                                                                                          case Band::L5: coll.numSigUsedL5.numSbas++;  break; } break;
                    case Gnss::GAL:   coll.numSigUsed.numGal++;   switch (sigInfo.band) { case Band::UNKNOWN: break;
                                                                                          case Band::L1: coll.numSigUsedL1.numGal++;   break;
                                                                                          case Band::E6: coll.numSigUsedE6.numGal++;   break;
                                                                                          case Band::L2: coll.numSigUsedL2.numGal++;   break;
                                                                                          case Band::L5: coll.numSigUsedL5.numGal++;   break; } break;
                    case Gnss::BDS:   coll.numSigUsed.numBds++;   switch (sigInfo.band) { case Band::UNKNOWN: break;
                                                                                          case Band::L1: coll.numSigUsedL1.numBds++;   break;
                                                                                          case Band::E6: coll.numSigUsedE6.numBds++;   break;
                                                                                          case Band::L2: coll.numSigUsedL2.numBds++;   break;
                                                                                          case Band::L5: coll.numSigUsedL5.numBds++;   break; } break;
                    case Gnss::QZSS:  coll.numSigUsed.numQzss++;  switch (sigInfo.band) { case Band::UNKNOWN: break;
                                                                                          case Band::L1: coll.numSigUsedL1.numQzss++;  break;
                                                                                          case Band::E6: coll.numSigUsedE6.numQzss++;  break;
                                                                                          case Band::L2: coll.numSigUsedL2.numQzss++;  break;
                                                                                          case Band::L5: coll.numSigUsedL5.numQzss++;  break; } break;
                    case Gnss::GLO:   coll.numSigUsed.numGlo++;   switch (sigInfo.band) { case Band::UNKNOWN: break;
                                                                                          case Band::L1: coll.numSigUsedL1.numGlo++;   break;
                                                                                          case Band::E6: coll.numSigUsedE6.numGlo++;   break;
                                                                                          case Band::L2: coll.numSigUsedL2.numGlo++;   break;
                                                                                          case Band::L5: coll.numSigUsedL5.numGlo++;   break; } break;
                    case Gnss::NAVIC: coll.numSigUsed.numNavic++; switch (sigInfo.band) { case Band::UNKNOWN: break;
                                                                                          case Band::L1: coll.numSigUsedL1.numNavic++; break;
                                                                                          case Band::E6: coll.numSigUsedE6.numNavic++; break;
                                                                                          case Band::L2: coll.numSigUsedL2.numNavic++; break;
                                                                                          case Band::L5: coll.numSigUsedL5.numNavic++; break; } break;
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


    totalPerGnssCnt(coll.numSigUsed);
    totalPerGnssCnt(coll.numSigUsedL1);
    totalPerGnssCnt(coll.numSigUsedE6);
    totalPerGnssCnt(coll.numSigUsedL2);
    totalPerGnssCnt(coll.numSigUsedL5);
    coll.haveNumSigUsed = (coll.numSigUsed.numTotal > 0);

    totalPerGnssCnt(coll.numSatUsed);
    coll.haveNumSatUsed = (coll.numSatUsed.numTotal > 0);

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

    const auto dt = (Time::FromClockRealtime() - state.startTime).GetSec();
    if (dt > 1e-3) {
        coll.bytesRate = (double)coll.bytesTotal / dt;
        coll.msgsRate = (double)coll.msgsTotal / dt;
    }

    std::snprintf(coll.str, sizeof(coll.str), "%-12s %s (%c) %+11.7f %+12.7f (%5.1f) %+5.0f (%5.1f) %4.1f",
        coll.fixTypeStr, coll.time.StrUtcTime().c_str(), coll.haveTime ? (coll.confTime ? 'Y' : 'y') : 'n',
        RadToDeg(coll.posLlh[0]), RadToDeg(coll.posLlh[1]), coll.posAccHoriz, coll.posLlh[2], coll.posAccVert,
        coll.pDOP);
}

// ---------------------------------------------------------------------------------------------------------------------

static Detect DetectNmea(CollPersist& persist, const ParserMsg& msg, const NmeaPayloadPtr& nmea)
{
    if (!nmea) {
        return Detect::NONE;
    }

    Detect detect = Detect::NONE;
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
        if (persist.haveSecMs && (millis != persist.secMs)) {
            EPOCH_TRACE("persist %s %d != %d", msg.name_.c_str(), millis, persist.secMs);
            (void)msg;
            detect = Detect::BEGIN;
        }
        persist.secMs = millis;
        persist.haveSecMs = true;
    }

    return detect;
}

// ---------------------------------------------------------------------------------------------------------------------

static Detect DetectUbx(CollPersist& persist, const ParserMsg& msg)
{
    const uint8_t* ubx = msg.Data();
    const std::size_t size = msg.Size();

    const uint8_t clsId = UbxClsId(ubx);
    if (clsId != UBX_NAV_CLSID) {
        return Detect::NONE;
    }
    const uint8_t msgId = UbxMsgId(ubx);
    Detect detect = Detect::NONE;
    uint32_t iTow = std::numeric_limits<uint32_t>::max();
    switch (msgId) {
        case UBX_NAV_EOE_MSGID:
            EPOCH_TRACE("detect %s", msg.name_.c_str());
            persist.haveTowMs = false;  // or we'll detect the epoch again on next message with tow
            detect = Detect::END;
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
            if (size > (UBX_FRAME_SIZE + sizeof(uint32_t))) {
                std::memcpy(&iTow, &ubx[UBX_HEAD_SIZE], sizeof(iTow));
            }
            break;
        case UBX_NAV_SVIN_MSGID:
        case UBX_NAV_ODO_MSGID:
        case UBX_NAV_HPPOSLLH_MSGID:
        case UBX_NAV_HPPOSECEF_MSGID:
        case UBX_NAV_RELPOSNED_MSGID:
            if (size > (UBX_FRAME_SIZE + 4 + +sizeof(uint32_t))) {
                std::memcpy(&iTow, &ubx[UBX_HEAD_SIZE + 4], sizeof(iTow));
            }
            break;
    }

    if (iTow != std::numeric_limits<uint32_t>::max()) {
        if (persist.haveTowMs && (persist.towMs != iTow)) {
            EPOCH_TRACE("detect %s %" PRIu32 " != %" PRIu32, msg.name_.c_str(), persist.towMs, iTow);
            detect = Detect::BEGIN;
        }
        persist.towMs = iTow;
        persist.haveTowMs = true;
    }

    return detect;
}

// ---------------------------------------------------------------------------------------------------------------------

static Detect DetectSbf(CollPersist& persist, const ParserMsg& msg)
{
    const uint8_t* sbf = msg.Data();
    const std::size_t size = msg.Size();

    const uint16_t msgId = SbfBlockType(sbf);

    Detect detect = Detect::NONE;
    uint32_t iTow = std::numeric_limits<uint32_t>::max();
    switch (msgId) {
        // We cannot really detect based on this. There's also a ENDOFATT message and we cannot know which of these are
        // enabled or not. :-/ case SBF_ENDOFPVT_MSGID:
        //     EPOCH_TRACE("detect %s", msg.name_.c_str());
        //     detect.haveTowMs = false; // or we'll detect the epoch again on next message with tow
        //     complete = true;
        //     break;

        // So we can only really detect based on time (and live with the latency)
        case SBF_ENDOFPVT_MSGID:
        case SBF_ENDOFATT_MSGID:

        case SBF_MEASEPOCH_MSGID:
        case SBF_MEASEXTRA_MSGID:
        case SBF_CHANNELSTATUS_MSGID:
        case SBF_PVTCARTESIAN_MSGID:
        case SBF_PVTGEODETIC_MSGID:
        case SBF_POSCOVCARTESIAN_MSGID:
        case SBF_POSCOVGEODETIC_MSGID:
        case SBF_VELCOVCARTESIAN_MSGID:
        case SBF_VELCOVGEODETIC_MSGID:
        case SBF_DOP_MSGID:
        case SBF_BASEVECTORCART_MSGID:
        case SBF_BASEVECTORGEOD_MSGID:
        case SBF_PVTSUPPORT_MSGID:
        case SBF_PVTSUPPORTA_MSGID:
        case SBF_NAVCART_MSGID:
        case SBF_ATTEULER_MSGID:
        case SBF_ATTCOVEULER_MSGID:
        case SBF_AUXANTPOSITIONS_MSGID:
        case SBF_RECEIVERTIME_MSGID:
            if (size > (SBF_HEAD_SIZE + sizeof(uint32_t))) {
                std::memcpy(&iTow, &sbf[SBF_HEAD_SIZE], sizeof(iTow));
            }
            break;
    }

    if (iTow != std::numeric_limits<uint32_t>::max()) {
        if (persist.haveTowMs && (persist.towMs != iTow)) {
            EPOCH_TRACE("detect %s %" PRIu32 " != %" PRIu32, msg.name_.c_str(), persist.towMs, iTow);
            detect = Detect::BEGIN;
        }
        persist.towMs = iTow;
        persist.haveTowMs = true;
    }

    return detect;
}

// ---------------------------------------------------------------------------------------------------------------------

static Detect DetectRtcm3(CollPersist& /*persist*/, const ParserMsg& msg)
{
    Rtcm3MsmHeader msm;
    if (Rtcm3GetMsmHeader(msg.Data(), msm) && !msm.multi_msg_bit_) {
        return Detect::END;
    }

    return Detect::NONE;
}

// ---------------------------------------------------------------------------------------------------------------------

static void CollectNmea(Epoch& coll, CollState& state, const ParserMsg& msg, const NmeaPayloadPtr& nmea)
{
    if (!nmea) {
        return;
    }
    UNUSED(msg);  // debug disabled or release build
    switch (nmea->formatter_) {
        case NmeaFormatter::GGA: {
            const auto& gga = dynamic_cast<const NmeaGgaPayload&>(*nmea);
            EPOCH_TRACE("collect %s", msg.name_.c_str());
            if (gga.time.valid && (state.haveHms < CollQual::LOW)) {
                state.haveHms = CollQual::LOW;
                state.hour = gga.time.hours;
                state.minute = gga.time.mins;
                state.second = gga.time.secs;
                state.leapsKnown = true;  // we can only hope...
            }
            if ((gga.quality != NmeaQualityGga::UNSPECIFIED) && (state.haveFixType < CollQual::LOW)) {
                state.haveFixType = CollQual::LOW;
                coll.fixType = NmeaQualityGgaToFixType(gga.quality);
                coll.fixOk = true;
            }
            if ((gga.quality > NmeaQualityGga::NOFIX) && gga.llh.latlon_valid && gga.llh.height_valid &&
                (state.havePosLlh < CollQual::LOW)) {
                state.havePosLlh = CollQual::LOW;
                coll.posLlh[0] = DegToRad(gga.llh.lat);
                coll.posLlh[1] = DegToRad(gga.llh.lon);
                coll.posLlh[2] = gga.llh.height;
                coll.heightMsl = gga.height_msl.value;
                coll.haveHeightMsl = gga.height_msl.valid;
            }
            if (gga.diff_age.valid && (state.haveDiffAge < CollQual::LOW)) {
                state.haveDiffAge = CollQual::LOW;
                coll.diffAge = gga.diff_age.value;
                coll.haveDiffAge = true;
            }
            if (!gga.num_sv.valid && (state.haveNumSatUsed < CollQual::LOW)) {
                state.haveNumSatUsed = CollQual::LOW;
                coll.numSatUsed.numTotal = gga.num_sv.value;
            }
            break;
        }
        case NmeaFormatter::RMC: {
            EPOCH_TRACE("collect %s", msg.name_.c_str());
            const auto& rmc = dynamic_cast<const NmeaRmcPayload&>(*nmea);
            if (rmc.time.valid && (state.haveHms < CollQual::LOW)) {
                state.haveHms = CollQual::LOW;
                state.hour = rmc.time.hours;
                state.minute = rmc.time.mins;
                state.second = rmc.time.secs;
                state.leapsKnown = true;  // we can only hope...
            }
            if (rmc.date.valid && (state.haveYmd < CollQual::LOW)) {
                state.haveYmd = CollQual::LOW;
                state.day = rmc.date.days;
                state.month = rmc.date.months;
                state.year = rmc.date.years;
            }
            FixType fixType = FixType::UNKNOWN;
            if ((rmc.mode != NmeaModeRmcGns::UNSPECIFIED) && (state.haveFixType < CollQual::LOW)) {
                state.haveFixType = CollQual::LOW;
                fixType = NmeaModeRmcGnsToFixType(rmc.mode);
                coll.fixType = fixType;
                coll.fixOk = (rmc.status == NmeaStatusGllRmc::VALID);
            }
            if ((fixType > FixType::NOFIX) && rmc.ll.latlon_valid && rmc.ll.height_valid &&
                (state.havePosLl < CollQual::LOW)) {
                state.havePosLl = CollQual::LOW;
                coll.posLlh[0] = DegToRad(rmc.ll.lat);
                coll.posLlh[1] = DegToRad(rmc.ll.lon);
            }
            break;
        }
        case NmeaFormatter::GLL: {
            EPOCH_TRACE("collect %s", msg.name_.c_str());
            const auto& gll = dynamic_cast<const NmeaGllPayload&>(*nmea);
            if (gll.time.valid && (state.haveHms < CollQual::LOW)) {
                state.haveHms = CollQual::LOW;
                state.hour = gll.time.hours;
                state.minute = gll.time.mins;
                state.second = gll.time.secs;
                state.leapsKnown = true;  // we can only hope...
            }
            FixType fixType = FixType::UNKNOWN;
            if ((gll.mode != NmeaModeGllVtg::UNSPECIFIED) && (state.haveFixType < CollQual::LOW)) {
                state.haveFixType = CollQual::LOW;
                fixType = NmeaModeGllVtgToFixType(gll.mode);
                coll.fixType = fixType;
                coll.fixOk = (gll.status == NmeaStatusGllRmc::VALID);
            }
            if ((fixType > FixType::NOFIX) && gll.ll.latlon_valid && gll.ll.height_valid &&
                (state.havePosLl < CollQual::LOW)) {
                state.havePosLl = CollQual::LOW;
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
                if (state.haveFixType < CollQual::MED) {
                    state.haveFixType = CollQual::MED;
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
                if (UBX_NAV_PVT_V1_VALID_VALIDTIME(pvt.valid) && (state.haveHms < CollQual::MED)) {
                    state.haveHms = CollQual::MED;
                    state.haveTimeAcc = CollQual::MED;
                    state.hour = pvt.hour;
                    state.minute = pvt.min;
                    state.second = (double)pvt.sec + ((double)pvt.nano * 1e-9);
                    state.confHms =
                        (UBX_NAV_PVT_V1_FLAGS2_CONFAVAIL(pvt.flags2) && UBX_NAV_PVT_V1_FLAGS2_CONFTIME(pvt.flags2));
                    coll.timeAcc = (double)pvt.tAcc * UBX_NAV_PVT_V1_TACC_SCALE;
                    state.leapsKnown = UBX_NAV_PVT_V1_VALID_FULLYRESOLVED(pvt.valid);
                }

                // Date
                if (UBX_NAV_PVT_V1_VALID_VALIDDATE(pvt.valid) && (state.haveYmd < CollQual::MED)) {
                    state.haveYmd = CollQual::MED;
                    state.year = pvt.year;
                    state.month = pvt.month;
                    state.day = pvt.day;
                    state.confYmd =
                        (UBX_NAV_PVT_V1_FLAGS2_CONFAVAIL(pvt.flags2) && UBX_NAV_PVT_V1_FLAGS2_CONFDATE(pvt.flags2));
                }

                // Geodetic coordinates
                if (!UBX_NAV_PVT_V1_FLAGS3_INVALIDLLH(pvt.flags3) && (state.havePosLlh < CollQual::MED)) {
                    state.havePosLlh = CollQual::MED;
                    coll.posLlh[0] = DegToRad((double)pvt.lat * UBX_NAV_PVT_V1_LAT_SCALE);
                    coll.posLlh[1] = DegToRad((double)pvt.lon * UBX_NAV_PVT_V1_LON_SCALE);
                    coll.posLlh[2] = (double)pvt.height * UBX_NAV_PVT_V1_HEIGHT_SCALE;
                    coll.heightMsl = (double)pvt.hMSL * UBX_NAV_PVT_V1_HEIGHT_SCALE;
                    coll.haveHeightMsl = true;
                }

                // Position accuracy estimate
                if ((fixType > FixType::NOFIX) && (state.havePosAccHoriz < CollQual::MED)) {
                    state.havePosAccHoriz = CollQual::MED;
                    coll.posAccHoriz = (double)pvt.hAcc * UBX_NAV_PVT_V1_HACC_SCALE;
                }
                if ((fixType > FixType::NOFIX) && (state.havePosAccVert < CollQual::MED)) {
                    state.havePosAccVert = CollQual::MED;
                    coll.posAccVert = (double)pvt.vAcc * UBX_NAV_PVT_V1_VACC_SCALE;
                }

                // Velocity
                if ((fixType > FixType::NOFIX) && (state.haveVelNed < CollQual::MED)) {
                    state.haveVelNed = CollQual::MED;
                    coll.velNed[0] = pvt.velN * UBX_NAV_PVT_V1_VELNED_SCALE;
                    coll.velNed[1] = pvt.velE * UBX_NAV_PVT_V1_VELNED_SCALE;
                    coll.velNed[2] = pvt.velD * UBX_NAV_PVT_V1_VELNED_SCALE;
                }
                if ((fixType > FixType::NOFIX) && (state.haveVelAcc < CollQual::MED)) {
                    state.haveVelAcc = CollQual::MED;
                    coll.velAcc = pvt.sAcc * UBX_NAV_PVT_V1_SACC_SCALE;
                }

                if (pvt.pDOP < 9999) {
                    coll.pDOP = (float)pvt.pDOP * UBX_NAV_PVT_V1_PDOP_SCALE;
                    coll.havePdop = true;
                }

                if (state.haveNumSatUsed < CollQual::MED) {
                    state.haveNumSatUsed = CollQual::MED;
                    coll.numSatUsed.numTotal = pvt.numSV;
                }

                if (state.haveGpsTow < CollQual::MED) {
                    state.haveGpsTow = CollQual::MED;
                    coll.gpsTow = pvt.iTOW * UBX_NAV_PVT_V1_ITOW_SCALE;
                }
            }
            break;

        case UBX_NAV_POSECEF_MSGID:
            if (size == UBX_NAV_POSECEF_V0_SIZE) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());
                UBX_NAV_POSECEF_V0_GROUP0 pos;
                std::memcpy(&pos, &data[UBX_HEAD_SIZE], sizeof(pos));
                if (state.havePosXyz < CollQual::MED) {
                    state.havePosXyz = CollQual::MED;
                    coll.posXyz[0] = (double)pos.ecefX * UBX_NAV_POSECEF_V0_ECEF_XYZ_SCALE;
                    coll.posXyz[1] = (double)pos.ecefY * UBX_NAV_POSECEF_V0_ECEF_XYZ_SCALE;
                    coll.posXyz[2] = (double)pos.ecefZ * UBX_NAV_POSECEF_V0_ECEF_XYZ_SCALE;
                }
                if (state.havePosAcc < CollQual::MED) {
                    state.havePosAcc = CollQual::MED;
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
                    if (state.havePosXyz < CollQual::HIGH) {
                        state.havePosXyz = CollQual::HIGH;
                        coll.posXyz[0] = ((double)pos.ecefX * UBX_NAV_HPPOSECEF_V0_ECEF_XYZ_SCALE) +
                                         ((double)pos.ecefXHp * UBX_NAV_HPPOSECEF_V0_ECEF_XYZ_HP_SCALE);
                        coll.posXyz[1] = ((double)pos.ecefY * UBX_NAV_HPPOSECEF_V0_ECEF_XYZ_SCALE) +
                                         ((double)pos.ecefYHp * UBX_NAV_HPPOSECEF_V0_ECEF_XYZ_HP_SCALE);
                        coll.posXyz[2] = ((double)pos.ecefZ * UBX_NAV_HPPOSECEF_V0_ECEF_XYZ_SCALE) +
                                         ((double)pos.ecefZHp * UBX_NAV_HPPOSECEF_V0_ECEF_XYZ_HP_SCALE);
                    }
                    if (state.havePosAcc < CollQual::HIGH) {
                        state.havePosAcc = CollQual::HIGH;
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
                if (UBX_NAV_TIMEGPS_V0_VALID_WEEKVALID(time.valid) && (state.haveGpsWno < CollQual::MED)) {
                    state.haveGpsWno = CollQual::MED;
                }
                if (UBX_NAV_TIMEGPS_V0_VALID_TOWVALID(time.valid)) {
                    if (state.haveGpsTow < CollQual::HIGH) {
                        state.haveGpsTow = CollQual::HIGH;
                    }
                    if (state.haveTimeAcc < CollQual::HIGH) {
                        state.haveTimeAcc = CollQual::HIGH;
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
                if (state.haveUptime < CollQual::MED) {
                    state.haveUptime = CollQual::MED;
                    coll.uptime.SetNSec((uint64_t)status.msss * (uint64_t)1000000);
                }

                // Fix info
                FixType fixType = FixType::UNKNOWN;
                if (state.haveFixType < CollQual::MED) {
                    state.haveFixType = CollQual::MED;
                    switch (status.gpsFix) {  // clang-format off
                        case UBX_NAV_STATUS_V0_GPSFIX_NOFIX:  fixType = FixType::NOFIX;     break;
                        case UBX_NAV_STATUS_V0_GPSFIX_DRONLY: fixType = FixType::DRONLY;    break;
                        case UBX_NAV_STATUS_V0_GPSFIX_2D:     fixType = FixType::SPP_2D;    break;
                        case UBX_NAV_STATUS_V0_GPSFIX_3D:     fixType = FixType::SPP_3D;    break;
                        case UBX_NAV_STATUS_V0_GPSFIX_3D_DR:  fixType = FixType::SPP_3D_DR; break;
                        case UBX_NAV_STATUS_V0_GPSFIX_TIME:   fixType = FixType::TIME;      break;
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
                if (state.haveClock < CollQual::MED) {
                    state.haveClock = CollQual::MED;
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
                if (UBX_NAV_RELPOSNED_V1_FLAGS_RELPOSVALID(rel.flags) && (state.haveRelPos < CollQual::HIGH)) {
                    state.haveRelPos = CollQual::HIGH;
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
                if (state.haveSigs < CollQual::MED) {
                    state.haveSigs = CollQual::MED;
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
                if (state.haveSats < CollQual::MED) {
                    state.haveSats = CollQual::MED;
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
                            satInfo.orbAvail |= SatOrb::EPH;
                        }
                        if (UBX_NAV_SAT_V1_FLAGS_ALMAVAIL(sat.flags)) {
                            satInfo.orbAvail |= SatOrb::ALM;
                        }
                        if (UBX_NAV_SAT_V1_FLAGS_ANOAVAIL(sat.flags) || UBX_NAV_SAT_V1_FLAGS_AOPAVAIL(sat.flags)) {
                            satInfo.orbAvail |= SatOrb::PRED;
                        }
                        satInfo.azim = sat.azim;
                        satInfo.elev = sat.elev;

                        coll.sats.push_back(satInfo);
                    }
                }
            }
            break;

        case UBX_NAV_DOP_MSGID:
            if (size == UBX_NAV_DOP_V0_SIZE) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());
                UBX_NAV_DOP_V0_GROUP0 dop;
                std::memcpy(&dop, &data[UBX_HEAD_SIZE], sizeof(dop));
                if (dop.pDOP < 9999) {
                    coll.pDOP = (float)dop.pDOP * UBX_NAV_DOP_V0_XDOP_SCALE;
                    coll.havePdop = true;
                }
                if (dop.gDOP < 9999) {
                    coll.gDOP = (float)dop.gDOP * UBX_NAV_DOP_V0_XDOP_SCALE;
                    coll.haveGdop = true;
                }
                if (dop.tDOP < 9999) {
                    coll.tDOP = (float)dop.tDOP * UBX_NAV_DOP_V0_XDOP_SCALE;
                    coll.haveTdop = true;
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

static FixType sbfModePvtToFixType(const uint8_t mode);

static void CollectSbf(Epoch& coll, CollState& state, const ParserMsg& msg)
{
    const uint8_t* data = msg.Data();
    const std::size_t size = msg.Size();

    switch (SbfBlockType(data)) {
        case SBF_NAVCART_MSGID:
            if (size >= SBF_NAVCART_REV0_SIZE) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());
                SbfNavCartRev0 nav;
                std::memcpy(&nav, &data[SBF_HEAD_SIZE], sizeof(nav));

                if (state.haveFixType < CollQual::LOW) {
                    state.haveFixType = CollQual::LOW;
                    coll.fixType = sbfModePvtToFixType(nav.Mode);
                    coll.fixOk = true;
                }

                if (!SbfDoNotUse(nav.WNc) && (state.haveGpsWno < CollQual::LOW)) {
                    state.haveGpsWno = CollQual::LOW;
                    coll.gpsWno = nav.WNc;
                }

                if (!SbfDoNotUse(nav.TOW) && (state.haveGpsTow < CollQual::LOW)) {
                    state.haveGpsTow = CollQual::LOW;
                    coll.gpsTow = (double)nav.TOW * 1e-3;
                }

                if (!SbfDoNotUse(nav.X) && !SbfDoNotUse(nav.Y) && !SbfDoNotUse(nav.Z) &&
                    (state.havePosXyz < CollQual::HIGH)) {
                    state.havePosXyz = CollQual::HIGH;
                    coll.posXyz[0] = nav.X;
                    coll.posXyz[1] = nav.Y;
                    coll.posXyz[2] = nav.Z;
                }

                if (!SbfDoNotUse(nav.PosHAcc) && !SbfDoNotUse(nav.PosVAcc) && (state.havePosAcc < CollQual::HIGH)) {
                    state.havePosAcc = CollQual::HIGH;
                    coll.posAccHoriz = (double)nav.PosHAcc * 1e-3;
                    coll.posAccVert = (double)nav.PosVAcc * 1e-3;
                }

                if (!SbfDoNotUse(nav.RxClkBias) && !SbfDoNotUse(nav.RxClkDrift) && (state.haveClock < CollQual::HIGH)) {
                    state.haveClock = CollQual::HIGH;
                    coll.clockBias = nav.RxClkBias;
                    coll.clockDrift = (double)nav.RxClkDrift * 1e-6;  // ?
                }

                if (!SbfDoNotUse(nav.UTCHour) && !SbfDoNotUse(nav.UTCMin) && !SbfDoNotUse(nav.UTCmsec) &&
                    (state.haveHms < CollQual::MED)) {
                    state.haveHms = CollQual::MED;
                    state.hour = nav.UTCHour;
                    state.minute = nav.UTCMin;
                    state.second = (double)nav.UTCmsec * 1e-3;
                }
                if (!SbfDoNotUse(nav.UTCYear) && !SbfDoNotUse(nav.UTCMonth) && !SbfDoNotUse(nav.UTCDay) &&
                    (state.haveYmd < CollQual::MED)) {
                    state.haveYmd = CollQual::MED;
                    state.year = nav.UTCYear + 2000;
                    state.month = nav.UTCMonth;
                    state.day = nav.UTCDay;
                }
            }
            break;

        case SBF_MEASEPOCH_MSGID:
            if ((state.haveSigs < CollQual::MED) && state.sbfMeasEpoch.empty() && (SbfBlockRev(data) == 1) &&
                (size >= SBF_MEASEPOCH_REV1_MIN_SIZE)) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());

                // Check message sanity and store it for later processing together with ChannelStatus data
                const std::size_t SB1Length = data[SBF_HEAD_SIZE + 7];
                const std::size_t SB2Length = data[SBF_HEAD_SIZE + 8];
                if ((SB1Length >= sizeof(MeasEpochChannelType1Rev0)) &&
                    (SB2Length >= sizeof(MeasEpochChannelType2Rev0))) {
                    const std::size_t N1 = data[SBF_HEAD_SIZE + 6];
                    std::size_t offs = SBF_HEAD_SIZE + sizeof(MeasEpochRev1);
                    for (std::size_t ix1 = 0; (ix1 < N1) && (offs < size); ix1++) {
                        const std::size_t N2 = data[offs + 19];
                        offs += SB1Length;
                        offs += (N2 * SB2Length);
                    }
                    if (offs <= size) {
                        state.sbfMeasEpoch = msg.data_;
                    }
                }
            }
            break;
        case SBF_CHANNELSTATUS_MSGID:
            if ((state.haveSats < CollQual::MED) && state.sbfChannelStatus.empty() && (SbfBlockRev(data) == 0) &&
                (size >= SBF_CHANNELSTATUS_REV0_MIN_SIZE)) {
                EPOCH_TRACE("collect %s", msg.name_.c_str());

                // Check message sanity and store it for later processing together with MeasEpoch data
                const std::size_t SB1Length = data[SBF_HEAD_SIZE + 7];
                const std::size_t SB2Length = data[SBF_HEAD_SIZE + 8];
                if ((SB1Length >= sizeof(ChannelSatInfoRev0)) && (SB2Length >= sizeof(ChannelStateInfoRev0))) {
                    const std::size_t N1 = data[SBF_HEAD_SIZE + 6];
                    std::size_t offs = SBF_HEAD_SIZE + sizeof(ChannelStatusRev0);
                    for (std::size_t ix1 = 0; (ix1 < N1) && (offs < size); ix1++) {
                        const std::size_t N2 = data[offs + 9];
                        offs += SB1Length;
                        offs += (N2 * SB2Length);
                    }
                    if (offs <= size) {
                        state.sbfChannelStatus = msg.data_;
                    }
                }
            }
    }
}

static FixType sbfModePvtToFixType(const uint8_t mode)
{
    FixType fix = FixType::UNKNOWN;
    switch (SBF_MODEPVT_SOL(mode)) {  // clang-format off
        case SBF_MODEPVT_SOL_NO_GNSS_PVT:              fix = FixType::NOFIX;      break;
        case SBF_MODEPVT_SOL_STANDALONE_PVT:           /* FALLTHROUGH */
        case SBF_MODEPVT_SOL_SBAS_AIDED_PVT:           /* FALLTHROUGH */
        case SBF_MODEPVT_SOL_DIFFERENTIAL_PVT:         fix = SBF_MODEPVT_IS2D(mode) ? FixType::SPP_2D : FixType::SPP_3D;  break;
        case SBF_MODEPVT_SOL_FIXED_LOCATION:           fix = FixType::TIME;       break;
        case SBF_MODEPVT_SOL_MOVINGBASE_RTK_FIXED:     /* FALLTHROUGH */
        case SBF_MODEPVT_SOL_RTK_FIXED:                fix = FixType::RTK_FIXED;  break;
        case SBF_MODEPVT_SOL_PPP:                      /* FALLTHROUGH */
        case SBF_MODEPVT_SOL_MOVINGBASE_RTK_FLOAT:     /* FALLTHROUGH */
        case SBF_MODEPVT_SOL_RTK_FLOAT:                fix = FixType::RTK_FLOAT;  break;
    }
    return fix;
}

static Signal sbfSigIdxToSignal(const uint8_t lo, const uint8_t hi)
{
    const int id = (lo == 31 ? hi + 31 : lo);
    switch (id) { // clang-format off
        case  0: return Signal::GPS_L1CA;
        case  3: return Signal::GPS_L2C;
        case  4: return Signal::GPS_L5;
        case 24: return Signal::SBAS_L1CA;
        case 17: return Signal::GAL_E1;
        case 19: return Signal::GAL_E6;
        case 21: return Signal::GAL_E5B;
        case 20: return Signal::GAL_E5A;
        case 13: return Signal::BDS_B1C;
        case 28: return Signal::BDS_B1I;
        case 30: return Signal::BDS_B3I;
        case 29: return Signal::BDS_B2I;
        case 34: return Signal::BDS_B2B;
        case 14: return Signal::BDS_B2A;
        case  6: return Signal::QZSS_L1CA;
        case 33: return Signal::QZSS_L1S;
        case  7: return Signal::QZSS_L2C;
        case 26: return Signal::QZSS_L5;
        case  8: return Signal::GLO_L1OF;
        case 11: return Signal::GLO_L2OF;
        case 15: return Signal::NAVIC_L5A;
    }  // clang-format on
    return Signal::UNKNOWN;
}

static SigInfo sbfMakeSigInfo(const uint16_t SVID, const uint8_t Type, const uint8_t ObsInfo, const uint8_t CN0)
{
    const Sat sat(SbfSvidToSat(SVID));
    const uint8_t sigIdxLo = SBF_MEASEPOCH_CHANNEL_TYPE_SIGIDXLO(Type);
    const uint8_t sigIdxHi = SBF_MEASEPOCH_CHANNEL_OBSINFO_SIGIDXHI(ObsInfo);

    // DEBUG("SVID %d (%s) sigIdx %d %d (%s) CN0 %d", (int)SVID, SbfSvidToSat(SVID).GetStr(), (int)sigIdxLo,
    // (int)sigIdxHi,
    //     SignalStr(sbfSigIdxToSignal(sigIdxLo, sigIdxHi)), (int)CN0);

    SigInfo sigInfo;
    sigInfo.satSig = { SbfSvidToSat(SVID), sbfSigIdxToSignal(sigIdxLo, sigIdxHi) };
    if ((sigIdxLo >= 8) && (sigIdxLo <= 11)) {
        sigInfo.gloFcn = (int)sigIdxHi - 8;
    }
    if (!SbfDoNotUse(CN0)) {
        sigInfo.use = SigUse::ACQUIRED;
        sigInfo.cno = (float)CN0 * 0.25f;
    }

    return sigInfo;
}

static void sbfUpdateSigInfo(SigInfoList& sigs, const SatSig satSig, const uint16_t HealthStatus,
    const uint16_t TrackingStatus, const uint16_t PVTStatus)
{
    int sigIx = -1;
    switch (satSig.GetSignal()) {  // clang-format off
        case Signal::GPS_L1CA:
        case Signal::GLO_L1OF:
        case Signal::SBAS_L1CA:
        case Signal::BDS_B1I:
        case Signal::QZSS_L1CA:
        case Signal::NAVIC_L5A:  sigIx = 0; break;
        case Signal::GAL_E1:
        case Signal::BDS_B2I:
        case Signal::QZSS_L2C:   sigIx = 1; break;
        case Signal::BDS_B3I:
        case Signal::QZSS_L5:    sigIx = 2; break;
        case Signal::GPS_L2C:
        case Signal::GLO_L2OF:
        case Signal::GAL_E6:
        case Signal::BDS_B1C:    sigIx = 3; break;
        case Signal::GPS_L5:
        case Signal::GAL_E5A:
        case Signal::BDS_B2A:    sigIx = 4; break;
        case Signal::GAL_E5B:
        case Signal::BDS_B2B:
        case Signal::QZSS_L1S:   sigIx = 5; break;
        default: break;
    }  // clang-format on

    if (sigIx < 0) {
        return;
    }

    auto entry = std::find_if(sigs.begin(), sigs.end(), [&satSig](const auto& cand) { return cand.satSig == satSig; });
    if (entry == sigs.end()) {
        SigInfo sigInfo;
        sigInfo.satSig = satSig;
        entry = sigs.insert(sigs.end(), sigInfo);
    }

    // clang-format off
    switch (SBF_CHANNELSTATUS_SI_HEALTH_SIG(HealthStatus, sigIx)) {
        case SBF_CHANNELSTATUS_SI_HEALTH_SIG_HEALTHY:   entry->health = SigHealth::HEALTHY;   break;
        case SBF_CHANNELSTATUS_SI_HEALTH_SIG_UNHEALTHY: entry->health = SigHealth::UNHEALTHY; break;
    }
    switch (SBF_CHANNELSTATE_TRKSTA_SIG(PVTStatus, sigIx)) {
        case SBF_CHANNELSTATE_PVTSTA_SIG_USED:          entry->prUsed = true; break;
        case SBF_CHANNELSTATE_PVTSTA_SIG_UNUSED:
        case SBF_CHANNELSTATE_PVTSTA_SIG_NOEPH:
        case SBF_CHANNELSTATE_PVTSTA_SIG_REJECTED:      break;
    }
    switch (SBF_CHANNELSTATE_PVTSTA_SIG(TrackingStatus, sigIx)) {
        case SBF_CHANNELSTATE_TRKSTA_SIG_IDLE:          entry->use = SigUse::NONE;     break;
        case SBF_CHANNELSTATE_TRKSTA_SIG_SEARCH:        entry->use = SigUse::SEARCH;   break;
        case SBF_CHANNELSTATE_TRKSTA_SIG_SYNC:          entry->use = SigUse::ACQUIRED; break;
        case SBF_CHANNELSTATE_TRKSTA_SIG_TRACKING:      entry->use = SigUse::CODELOCK; break;
    }
    // clang-format on
}

static void sbfCompleteSatSig(Epoch& coll, CollState& state)
{
    // This gives us some signal info (SV, signal and CNO)
    if (!state.sbfMeasEpoch.empty()) {
        state.haveSigs = CollQual::MED;
        const uint8_t* data = state.sbfMeasEpoch.data();
        const std::size_t size = state.sbfMeasEpoch.size();

        MeasEpochRev1 head;
        std::size_t offs = SBF_HEAD_SIZE;
        std::memcpy(&head, &data[offs], sizeof(head));
        offs += sizeof(head);

        for (std::size_t ix1 = 0; (ix1 < head.N1) && (offs < size); ix1++) {
            MeasEpochChannelType1Rev0 chn1;
            std::memcpy(&chn1, &data[offs], sizeof(chn1));
            offs += head.SB1Length;

            if (SBF_MEASEPOCH_CHANNEL_TYPE_ANTID(chn1.Type) == SBF_MEASEPOCH_CHANNEL_TYPE_ANTID_MAIN) {
                coll.sigs.push_back(sbfMakeSigInfo(chn1.SVID, chn1.Type, chn1.ObsInfo, chn1.CN0));
            }

            for (std::size_t ix2 = 0; (ix2 < chn1.N2) && (offs < size); ix2++) {
                MeasEpochChannelType2Rev0 chn2;
                std::memcpy(&chn2, &data[offs], sizeof(chn2));
                offs += head.SB2Length;

                if (SBF_MEASEPOCH_CHANNEL_TYPE_ANTID(chn2.Type) == SBF_MEASEPOCH_CHANNEL_TYPE_ANTID_MAIN) {
                    coll.sigs.push_back(sbfMakeSigInfo(chn1.SVID, chn2.Type, chn2.ObsInfo, chn2.CN0));
                }
            }
        }
    }

    // This gives us SV azim/elev, but also some more signal info (used, tracked, etc.)
    if (!state.sbfChannelStatus.empty()) {
        ChannelStatusRev0 head;
        const uint8_t* data = state.sbfChannelStatus.data();
        const std::size_t size = state.sbfChannelStatus.size();

        std::size_t offs = SBF_HEAD_SIZE;
        std::memcpy(&head, &data[offs], sizeof(head));
        offs += sizeof(head);

        for (std::size_t ix1 = 0; (ix1 < head.N) && (offs < size); ix1++) {
            ChannelSatInfoRev0 info1;
            std::memcpy(&info1, &data[offs], sizeof(info1));
            offs += head.SB1Length;

            const uint16_t SVID = (info1.SVID == 0 ? info1.SVIDFull : info1.SVID);
            const uint16_t azim = SBF_CHANNELSTATUS_SI_AZRS_AZIMUTH(info1.AzimuthRiseSet);  // do not use = 511
            const int8_t elev = info1.Elevation;

            bool anySigUsed = false;
            const Sat sat = SbfSvidToSat(SVID);

            for (std::size_t ix2 = 0; (ix2 < info1.N2) && (offs < size); ix2++) {
                ChannelStateInfoRev0 info2;
                std::memcpy(&info2, &data[offs], sizeof(info2));
                offs += head.SB2Length;

                if (info2.Antenna == 0) {
                    switch (sat.GetGnss()) {  // clang-format off
                        case Gnss::GPS:     sbfUpdateSigInfo(coll.sigs, {sat, Signal::GPS_L1CA},  info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::GPS_L2C},   info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::GPS_L5},    info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            break;
                        case Gnss::GLO:     sbfUpdateSigInfo(coll.sigs, {sat, Signal::GLO_L1OF},  info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::GLO_L2OF},  info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            break;
                        case Gnss::GAL:     sbfUpdateSigInfo(coll.sigs, {sat, Signal::GAL_E1},    info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::GAL_E6},    info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::GAL_E5A},   info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::GAL_E5B},   info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            break;
                        case Gnss::SBAS:    sbfUpdateSigInfo(coll.sigs, {sat, Signal::SBAS_L1CA}, info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            break;
                        case Gnss::BDS:     sbfUpdateSigInfo(coll.sigs, {sat, Signal::BDS_B1I},   info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::BDS_B2I},   info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::BDS_B3I},   info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::BDS_B1C},   info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::BDS_B2A},   info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::BDS_B2B},   info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            break;
                        case Gnss::QZSS:    sbfUpdateSigInfo(coll.sigs, {sat, Signal::QZSS_L1CA}, info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::QZSS_L2C},  info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::QZSS_L5},   info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            sbfUpdateSigInfo(coll.sigs, {sat, Signal::QZSS_L1S},  info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            break;
                        case Gnss::NAVIC:   sbfUpdateSigInfo(coll.sigs, {sat, Signal::NAVIC_L5A}, info1.HealthStatus, info2.TrackingStatus, info2.PVTStatus);
                                            break;
                        case Gnss::UNKNOWN: break;
                    }  // clang-format on
                }
            }

            SatInfo satInfo;
            satInfo.sat = sat;
            if ((azim != 511) && !SbfDoNotUse(elev)) {
                satInfo.azim = azim;
                satInfo.elev = elev;
                satInfo.orbUsed = (anySigUsed ? SatOrb::EPH : SatOrb::ALM);  // presumably...
            }
            coll.sats.push_back(satInfo);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

static void rtcm3MsmToSigInfo(SigInfoList& sigs, const Rtcm3MsmHeader& msm, const ParserMsg& msg);
static Sat rtcm3MsmGnssSatIdtoSat(const Rtcm3MsmGnss gnss, const int satId);
static Signal rtcm3MsmGnssSigIdtoSignal(const Rtcm3MsmGnss gnss, const int sigId);

static void CollectRtcm3(Epoch& coll, CollState& state, const ParserMsg& msg)
{
    const bool haveFixType = (state.haveFixType >= CollQual::LOW);
    const bool havePosXyz = (state.havePosXyz >= CollQual::MED);

    Rtcm3Arp arp;
    if ((!havePosXyz || !haveFixType) && Rtcm3GetArp(msg.Data(), arp)) {
        if (!havePosXyz) {
            state.havePosXyz = CollQual::MED;
            coll.posXyz[0] = arp.ecef_x_;
            coll.posXyz[1] = arp.ecef_y_;
            coll.posXyz[2] = arp.ecef_z_;
        }
        if (!haveFixType) {
            state.haveFixType = CollQual::MED;
            coll.fixType = FixType::SPP_3D;
            coll.fixOk = true;
        }
    }

    Rtcm3MsmHeader msmHeader;
    bool isMsm = Rtcm3GetMsmHeader(msg.Data(), msmHeader);
    if (isMsm && (state.haveGpsTow < CollQual::MED) && (msmHeader.gnss_ == Rtcm3MsmGnss::GPS)) {
        state.haveGpsTow = CollQual::MED;
        coll.gpsTow = msmHeader.gps_tow_;
    }

    // Only MSM4-7 have useful signal infos
    if (isMsm && (state.haveSigs < CollQual::MED)) {
        rtcm3MsmToSigInfo(coll.sigs, msmHeader, msg);
        // DEBUG("MSM sigs %" PRIuMAX ", %s", coll.sigs.size(), msmHeader.multi_msg_bit_ ? "more" : "last");
        // Last MSM, assuming they all come in a row
        if (!msmHeader.multi_msg_bit_) {
            state.haveSigs = CollQual::MED;
        }
    }
}

static void rtcm3MsmToSigInfo(SigInfoList& sigs, const Rtcm3MsmHeader& msm, const ParserMsg& msg)
{
    // DEBUG("%s %s %s %s sat_mask=0x%016" PRIx64 " (%d) sig_mask=0x08%" PRIx32 " (%d) cell_mask=0x%016" PRIu64 " (%d)",
    //     msg.name_.c_str(), Rtcm3MsmGnssStr(msm.gnss_), Rtcm3MsmTypeStr(msm.msm_), msm.multi_msg_bit_ ? "more" : "last",
    //     msm.sat_mask_, msm.num_sat_, msm.sig_mask_, msm.num_sig_, msm.cell_mask_, msm.num_cell_);

    if ((msm.num_sat_ <= 0) || (msm.num_sig_ <= 0) || (msm.num_cell_ <= 0) || ((msm.num_sat_ * msm.num_sig_) > 64)) {
        return;
    }

    const std::size_t hdrOffs = RTCM3_HEAD_SIZE * 8;
    const std::size_t hdrSize = 12 + 12 + 30 + +1 + 3 + 7 + 2 + 2 + 1 + 3 + 64 + 32 /* = 169 */ + (msm.num_sat_ * msm.num_sig_);
    const std::size_t satOffs = hdrOffs + hdrSize;
    std::size_t satSize = 0;
    switch (msm.msm_) { // clang-format off                DF397 sp DF398 DF399
        case Rtcm3MsmType::MSM1: satSize = msm.num_sat_ * (           10       ); break; // = 10
        case Rtcm3MsmType::MSM2: satSize = msm.num_sat_ * (           10       ); break; // = 10
        case Rtcm3MsmType::MSM3: satSize = msm.num_sat_ * (           10       ); break; // = 10
        case Rtcm3MsmType::MSM4: satSize = msm.num_sat_ * (  8      + 10       ); break; // = 18
        case Rtcm3MsmType::MSM5: satSize = msm.num_sat_ * (  8 + 4  + 10 +  14 ); break; // = 36
        case Rtcm3MsmType::MSM6: satSize = msm.num_sat_ * (  8      + 10       ); break; // = 18
        case Rtcm3MsmType::MSM7: satSize = msm.num_sat_ * (  8 + 4  + 10 +  14 ); break; // = 36
    } // clang-format off
    const std::size_t sigOffs = satOffs + satSize;

    std::size_t sigSize = 0;
    switch (msm.msm_) { // clang-format off
        //                                                  DF400 DF401 DF402 DF405 DF406 DF407 DF420 DF403 DF408 DF404
        //                                                    C     L     L     C     L     L     L     S     S     D
        case Rtcm3MsmType::MSM1: sigSize = msm.num_cell_ * (  15                                                        ); break; // = 15   C
        case Rtcm3MsmType::MSM2: sigSize = msm.num_cell_ * (        22  + 4                     + 1                     ); break; // = 27   L
        case Rtcm3MsmType::MSM3: sigSize = msm.num_cell_ * (  15  + 22  + 4                     + 1                     ); break; // = 42   C, L
        case Rtcm3MsmType::MSM4: sigSize = msm.num_cell_ * (  15  + 22  + 4                     + 1   + 6               ); break; // = 48   full C, full L, S
        case Rtcm3MsmType::MSM5: sigSize = msm.num_cell_ * (  15  + 22  + 4                     + 1   + 6         + 15  ); break; // = 63   full C, full L, S, D
        case Rtcm3MsmType::MSM6: sigSize = msm.num_cell_ * (                    20  + 24  + 10  + 1         + 10        ); break; // = 65   ext full C, ext full L, S
        case Rtcm3MsmType::MSM7: sigSize = msm.num_cell_ * (                    20  + 24  + 10  + 1         + 10  + 15  ); break; // = 80   ext full C, ext full L, S, D
    } // clang-format on
    const std::size_t totSize = sigOffs + sigSize;

    // DEBUG("hdr %" PRIuMAX " + %" PRIuMAX " = sat %" PRIuMAX " + %" PRIuMAX " = sig %" PRIuMAX " + %" PRIuMAX " = tot %" PRIuMAX " (msg %" PRIuMAX ")",
    //     hdrOffs, hdrSize, satOffs, satSize, sigOffs, sigSize, totSize, (msg.Size() - 3) * 8);
    UNUSED(totSize);

    int cellBit = 0;
    std::size_t sigIx = 0;
    for (int satBit = 63, satId = 1; satBit >= 0; satBit--, satId++) {
        if (!CheckBitsAll(msm.sat_mask_, Bit<uint64_t>(satBit))) {
            continue;
        }
        for (int sigBit = 31, sigId = 1; sigBit >= 0; sigBit--, sigId++) {
            if (!CheckBitsAll(msm.sig_mask_, Bit<uint32_t>(sigBit))) {
                continue;
            }

            // Signal data available
            if (CheckBitsAll(msm.cell_mask_, Bit<uint64_t>(cellBit))) {
                SigInfo sig;
                sig.satSig = { rtcm3MsmGnssSatIdtoSat(msm.gnss_, satId), rtcm3MsmGnssSigIdtoSignal(msm.gnss_, sigId) };
                switch (msm.msm_) {  // clang-format off
                    case Rtcm3MsmType::MSM1:
                        sig.prUsed =     Rtcm3GetUnsigned(msg.Data(), sigOffs                        + (sigIx * 15), 15) != 0x4000;   // DF400
                        break;
                    case Rtcm3MsmType::MSM2:
                        sig.prUsed =     Rtcm3GetUnsigned(msg.Data(), sigOffs                        + (sigIx * 15), 15) != 0x4000;   // DF400
                        break;
                    case Rtcm3MsmType::MSM3:
                        sig.prUsed =     Rtcm3GetUnsigned(msg.Data(), sigOffs                        + (sigIx * 15), 15) != 0x4000;   // DF400
                        sig.crUsed =     Rtcm3GetUnsigned(msg.Data(), sigOffs + (msm.num_cell_ * 15) + (sigIx * 22), 22) != 0x200000; // DF401
                        sig.cno = (float)Rtcm3GetUnsigned(msg.Data(), sigOffs + (msm.num_cell_ * 42) + (sigIx *  6),  6);             // DF403
                        break;
                    case Rtcm3MsmType::MSM4:
                        break;
                    case Rtcm3MsmType::MSM5:
                        sig.prUsed =     Rtcm3GetUnsigned(msg.Data(), sigOffs                        + (sigIx * 15), 15) != 0x4000;   // DF400
                        sig.crUsed =     Rtcm3GetUnsigned(msg.Data(), sigOffs + (msm.num_cell_ * 15) + (sigIx * 22), 22) != 0x200000; // DF401
                        sig.cno = (float)Rtcm3GetUnsigned(msg.Data(), sigOffs + (msm.num_cell_ * 42) + (sigIx *  6),  6);             // DF403
                        sig.doUsed =     Rtcm3GetUnsigned(msg.Data(), sigOffs + (msm.num_cell_ * 48) + (sigIx * 15), 15) != 0x4000;   // DF404
                        break;
                    case Rtcm3MsmType::MSM6:
                        sig.prUsed =     Rtcm3GetUnsigned(msg.Data(), sigOffs                        + (sigIx * 20), 20) != 0x80000;  // DF405
                        sig.crUsed =     Rtcm3GetUnsigned(msg.Data(), sigOffs + (msm.num_cell_ * 20) + (sigIx * 24), 24) != 0x800000; // DF406
                        sig.cno = (float)Rtcm3GetUnsigned(msg.Data(), sigOffs + (msm.num_cell_ * 55) + (sigIx * 10), 10) * 0.0625f;   // DF408
                        break;
                    case Rtcm3MsmType::MSM7:
                        sig.prUsed =     Rtcm3GetUnsigned(msg.Data(), sigOffs                        + (sigIx * 20), 20) != 0x80000;  // DF405
                        sig.crUsed =     Rtcm3GetUnsigned(msg.Data(), sigOffs + (msm.num_cell_ * 20) + (sigIx * 24), 24) != 0x800000; // DF406
                        sig.cno = (float)Rtcm3GetUnsigned(msg.Data(), sigOffs + (msm.num_cell_ * 55) + (sigIx * 10), 10) * 0.0625f;   // DF408
                        sig.doUsed =     Rtcm3GetUnsigned(msg.Data(), sigOffs + (msm.num_cell_ * 65) + (sigIx * 15), 15) != 0x4000;   // DF404
                        break;
                }  // clang-format on
                sig.use = (sig.crUsed ? SigUse::CARRLOCK : sig.prUsed ? SigUse::CODELOCK : SigUse::ACQUIRED);
                sigs.push_back(sig);
                // DEBUG("satBit=%2d satId=%2d sigBit=%2d sigId=%2d -> %s %s C %s L %s D %s S %.1f", satBit, satId, sigBit,
                //     sigId, sig.satSig.GetSat().GetStr(), SignalStr(sig.satSig.GetSignal()), ToStr(sig.prUsed),
                //     ToStr(sig.crUsed), ToStr(sig.doUsed), sig.cno);
                sigIx++;
            }
            cellBit++;
        }
    }
}

static Sat rtcm3MsmGnssSatIdtoSat(const Rtcm3MsmGnss gnss, const int satId)
{
    switch (gnss) {  // clang-format off
        case Rtcm3MsmGnss::GPS:    if ((satId > 0) && (satId <= NUM_GPS))   { return Sat(Gnss::GPS,   satId + (FIRST_GPS   - 1)); }; break;
        case Rtcm3MsmGnss::GLO:    if ((satId > 0) && (satId <= NUM_GLO))   { return Sat(Gnss::GLO,   satId + (FIRST_GLO   - 1)); }; break;
        case Rtcm3MsmGnss::GAL:    if ((satId > 0) && (satId <= NUM_GAL))   { return Sat(Gnss::GAL,   satId + (FIRST_GAL   - 1)); }; break;
        case Rtcm3MsmGnss::BDS:    if ((satId > 0) && (satId <= NUM_BDS))   { return Sat(Gnss::BDS,   satId + (FIRST_BDS   - 1)); }; break;
        case Rtcm3MsmGnss::SBAS:   if ((satId > 0) && (satId <= NUM_SBAS))  { return Sat(Gnss::SBAS,  satId + (FIRST_SBAS  - 1)); }; break;
        case Rtcm3MsmGnss::QZSS:   if ((satId > 0) && (satId <= NUM_QZSS))  { return Sat(Gnss::QZSS,  satId + (FIRST_QZSS  - 1)); }; break;
        case Rtcm3MsmGnss::NAVIC:  if ((satId > 0) && (satId <= NUM_NAVIC)) { return Sat(Gnss::NAVIC, satId + (FIRST_NAVIC - 1)); }; break;
    }  // clang-format on
    return INVALID_SAT;
}

static Signal rtcm3MsmGnssSigIdtoSignal(const Rtcm3MsmGnss gnss, const int sigId)
{
    switch (gnss) {  // clang-format off
        case Rtcm3MsmGnss::GPS:   switch (sigId) { case  2: return Signal::GPS_L1CA;
                                                   case 15: // M
                                                   case 16: // L
                                                   case 17: return Signal::GPS_L2C; // M+L
                                                   case 22: // I
                                                   case 23: // Q
                                                   case 24: return Signal::GPS_L5;    } break; // I+Q
        case Rtcm3MsmGnss::GLO:   switch (sigId) { case  2: return Signal::GLO_L1OF;
                                                   case  8: return Signal::GLO_L2OF;  } break;
        case Rtcm3MsmGnss::GAL:   switch (sigId) { case  2: // C
                                                   case  3: // A
                                                   case  4: // B
                                                   case  5: // B+C
                                                   case  6: return Signal::GAL_E1; // A+B+C
                                                   case  8: // C
                                                   case  9: // A
                                                   case 10: // B
                                                   case 11: // B+C
                                                   case 12: return Signal::GAL_E6; // A+B+C
                                                   case 14: // I
                                                   case 15: // Q
                                                   case 16: return Signal::GAL_E5B; // I+Q
                                                   case 22: // I
                                                   case 23: // Q
                                                   case 24: return Signal::GAL_E5A;   } break; // I+Q
        case Rtcm3MsmGnss::BDS:   switch (sigId) { case 30:
                                                   case 31:
                                                   case 32: return Signal::BDS_B1C; // ??
                                                   case  2: return Signal::BDS_B1I;
                                                   case  8: return Signal::BDS_B3I;
                                                   case 14: return Signal::BDS_B2I;
                                                   case 25:
                                                   case 26:
                                                   case 27: return Signal::BDS_B2B; // ??
                                                   case 22:
                                                   case 23:
                                                   case 24: return Signal::BDS_B2A; // ??
                                                   } break;
        case Rtcm3MsmGnss::SBAS:  switch (sigId) { case  2: return Signal::SBAS_L1CA; } break;
        case Rtcm3MsmGnss::QZSS:  switch (sigId) { case  2: return Signal::QZSS_L1CA;
                                                   case  9: return Signal::QZSS_L1S;
                                                   case 15: // M
                                                   case 16: // L
                                                   case 17: return Signal::QZSS_L2C; // L+M
                                                   case 22: // I
                                                   case 23: // Q
                                                   case 24: return Signal::QZSS_L5;   } break; // I+Q
        case Rtcm3MsmGnss::NAVIC: switch (sigId) { case  0: return Signal::NAVIC_L5A; } break;
    }  // clang-format on
    return Signal::UNKNOWN;
}

// ---------------------------------------------------------------------------------------------------------------------

bool IsNmeaTos(const ParserMsg& msg)
{
    //           1
    // 01234567890123456789
    //    |          |
    // $GNRMC,165441.60,...
    // $GNGST,165441.60,...
    // $GPZDA,165441.60,...
    // $GNGGA,165441.60,...
    // Maybe... but time is at a variable offset
    // $GNGLL,...,165441.6004,...

    if (msg.proto_ != Protocol::NMEA) {
        return false;
    }

    const uint8_t* data = msg.Data();
    if (((data[3] == 'R') && (data[4] == 'M') && (data[5] == 'C') && (data[7] != ',')) ||  // NMEA-Gx-RMC
        ((data[3] == 'G') && (data[4] == 'S') && (data[5] == 'T') && (data[7] != ',')) ||  // NMEA-Gx-GST
        ((data[3] == 'Z') && (data[4] == 'D') && (data[5] == 'A') && (data[7] != ',')) ||  // NMEA-Gx-ZDA
        ((data[3] == 'G') && (data[4] == 'G') && (data[5] == 'A') && (data[7] != ','))) {  // NMEA-Gx-GGA
        const int t = ((int)(data[14] - '0') * 100) + ((int)(data[15] - '0') * 10); // time [ms]
        if ((t < 50) || (t > 950)) { // less than 50ms from top-of-second
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------------------------------------------------

bool IsUbxTos(const ParserMsg& msg)
{
    if ((msg.proto_ != Protocol::UBX) || (UbxClsId(msg.Data()) != UBX_NAV_CLSID)) {
        return false;
    }
    uint32_t iTow = std::numeric_limits<uint32_t>::max();
    switch (UbxMsgId(msg.Data())) {
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
            if (msg.Size() > (UBX_FRAME_SIZE + sizeof(uint32_t))) {
                std::memcpy(&iTow, msg.Data() + UBX_HEAD_SIZE, sizeof(iTow));
            }
            break;
        case UBX_NAV_SVIN_MSGID:
        case UBX_NAV_ODO_MSGID:
        case UBX_NAV_HPPOSLLH_MSGID:
        case UBX_NAV_HPPOSECEF_MSGID:
        case UBX_NAV_RELPOSNED_MSGID:
            if (msg.Size() > (UBX_FRAME_SIZE + 4 + sizeof(uint32_t))) {
                std::memcpy(&iTow, msg.Data() + UBX_HEAD_SIZE + 4, sizeof(iTow));
            }
            break;
    }

    if (iTow != std::numeric_limits<uint32_t>::max()) {
        const uint32_t t = iTow % 1000;
        if ((t < 50) || (t > 950)) { // less than 50ms from top-of-second
            return true;
        }
    }

    return false;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
