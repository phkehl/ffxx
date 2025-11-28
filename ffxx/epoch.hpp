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
 *
 * @page FFXX_EPOCH Navigation epoch
 *
 * @section FFXX_EPOCH_CONCEPT Concept
 *
 * - Collect individual messages from GNSS receivers in different formats into a "navigation epoch" structure
 * - This (mostly) receiver-independent structure is a normalisation of the navigation solution, which is output in
 *   different ways for different receivers (or receiver configurations)
 * - Different data can be calculated or derived from other data. For example, position in ECEF can be calculated from
 *   position in lat/lon/height and vice versa.
 * - All data is normalised to SI units, receiver-specific identifiers are mapped to common enums, etc.
 * - Data from multiple messages are collected into a single structure
 * - Only navigation solution data is part of the epoch. Any auxillary data, such as raw measurements or system status
 *   messages, are not part of the navigation solution and therefore not part of the epoch. If such correlation is
 *   needed, it must be done on application level (i.e. not in the epoch code here).
 * - The epoch collection works for data received in real-time from a receiver as well as when reading it from a
 *   logfile.
 * - The epoch collection works for multiple, possibly overlapping or redundant, input formats. Currently it supports
 *   UBX and NMEA input, which can also be mixed.
 * - ...
 *
 * @section FFXX_EPOCH_EXAMPLE Example
 *
 * @code{cpp}
 * // TODO
 * @endcode
 *
 * @section FFXX_EPOCH_NOTES Notes
 *
 * - The epochCollect() function returns true once it has been fed with enough messages to have complete epoch
 * - The completeness can vary, depending on what messages are provided. For example, if no UBX-NAV-SAT is fed into
 *   the collector, no satellite information will be available in the resulting epoch.
 * - A epoch is considered complete by observing the supplied messages (called "epoch detection" in the code). It works
 *   in one of these two ways:
 *   - The receiver ends a sequence of navigation data messages with a specific "end of epoch" message. For example,
 *     u-blox receivers have UBX-NAV-EOE for this.
 *   - A new sequence of navigation data messages is detected by a changed time (iTOW field in UBX-NAV messages, or
 *     the time field in NMEA messages)
 * - The consequence of the two detection methods are:
 *   - If a end-of-epoch marker is available, epochDetect() returns true and provides the epoch data right away as
 *     soon as this marker message is received. There is no delay besides the time it takes the receiver to
 *     calculate and output the navigation solution.
 *   - If no end-of-epoch marker is available, the detection relies on observing changes in the time fields. As the
 *     change can only be observed in a subsequent navigation solution output, epochDetect() returns true only once the
 *     receiver starts to output a next navigation solution. That is, if the navigation output rate is 1Hz, the
 *     epochDetect() repots the epoch with 1s delay (or 0.5s at 2Hz, etc.)
 *
 */
#ifndef __FFXX_EPOCH_HPP__
#define __FFXX_EPOCH_HPP__

/* LIBC/STL */
#include <array>
#include <vector>
#include <cstdint>
#include <memory>

/* EXTERNAL */
#include <fpsdk_common/gnss.hpp>
#include <fpsdk_common/math.hpp>
#include <fpsdk_common/time.hpp>
#include <fpsdk_common/parser/types.hpp>

/* PACKAGE */

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::gnss;
using namespace fpsdk::common::math;
using namespace fpsdk::common::time;

/**
 * @brief Epoch satellite info
 */
struct SatInfo  // clang-format off
{
    Sat         sat             = INVALID_SAT;

    Gnss        gnss            = Gnss::UNKNOWN;
    SvNr        svNr            = INAVLID_SVNR;

    SatOrb      orbUsed         = SatOrb::UNKNOWN;
    int         orbAvail        = 0x00;                // Bits of SatOrb

    int         elev            = 0;                   // Elevation [deg] (-90..+90), only valid if orbUsed > SatOrb::NONE
    int         azim            = 0;                   // Azimuth [deg] (0..359), only valid if orbUsed > SatOrb::NONE

    const char* gnssStr         = nullptr;
    const char* satStr          = nullptr;
    const char* orbUsedStr      = nullptr;

    int ix = -1;
    std::array<int, 4> sigIxs = {{ -1, -1, -1, -1 }};

};  // clang-format on

// clang-format off
inline bool operator<(const SatInfo& lhs, const SatInfo& rhs) { return lhs.sat < rhs.sat; }
inline bool operator==(const SatInfo& lhs, const SatInfo& rhs) { return (lhs.sat == rhs.sat); }
struct SatInfoHasher { std::size_t operator()(const SatInfo& k) const { return k.sat.id_; } };
// clang-format on

/**
 * @brief Epoch signal info
 */
struct SigInfo  // clang-format off
{
    SatSig      satSig          = INVALID_SATSIG;

    Sat         sat             = INVALID_SAT;
    Gnss        gnss            = Gnss::UNKNOWN;
    SvNr        svNr            = INAVLID_SVNR;
    Signal      signal          = Signal::UNKNOWN;
    Band        band            = Band::UNKNOWN;

    int8_t      gloFcn          = 0;
    float       prRes           = 0.0f;
    float       cno             = 0.0f;

    SigUse      use             = SigUse::UNKNOWN;
    SigCorr     corr            = SigCorr::UNKNOWN;
    SigIono     iono            = SigIono::UNKNOWN;
    SigHealth   health          = SigHealth::UNKNOWN;

    bool        prUsed          = false;
    bool        crUsed          = false;
    bool        doUsed          = false;
    bool        anyUsed         = false;

    bool        prCorrUsed      = false;
    bool        crCorrUsed      = false;
    bool        doCorrUsed      = false;


    const char* gnssStr         = nullptr;
    const char* satStr          = nullptr;
    const char* signalStr       = nullptr;
    const char* signalStrShort  = nullptr;
    const char* bandStr         = nullptr;
    const char* useStr          = nullptr;
    const char* corrStr         = nullptr;
    const char* ionoStr         = nullptr;
    const char* healthStr       = nullptr;

    int ix = -1;
    int satIx = -1;
};  // clang-format on

// clang-format off
inline bool operator<(const SigInfo& lhs, const SigInfo& rhs) { return lhs.satSig < rhs.satSig; }
inline bool operator==(const SigInfo& lhs, const SigInfo& rhs) { return (lhs.satSig == rhs.satSig); }
struct SigInfoHasher { std::size_t operator()(const SigInfo& k) const { return k.satSig.id_; } };
// clang-format on

// CNo histogram
// clang-format off
// ix    0    1     2      3      4      5      6      7      8      9     10      11
// cno  0-4  5-9  10-14  15-19  20-24  25-29  30-34  35-39  40-44  45-49  50-44  55-...
using SigCnoHist = std::array<float, 12>;
static constexpr std::size_t CnoToIx(const float cno) { return ((cno) > 55.0f ? (12 - 1) : (cno > 0.0f ? (std::size_t)((cno) / 5.0) : 0)); }
static constexpr float IxToCnoLo(const std::size_t ix) { return (float)ix * 5.0; }
static constexpr float IxToCnoHi(const std::size_t ix) { return IxToCnoLo(ix + 1) - 1.0; };
// clang-format on

using EpochVec3 = std::array<double, 3>;
using SigInfoList = std::vector<SigInfo>;
using SatInfoList = std::vector<SatInfo>;

struct PerGnssCnt
{
    int numTotal = 0;
    int numGps = 0;
    int numSbas = 0;
    int numGal = 0;
    int numBds = 0;
    int numQzss = 0;
    int numGlo = 0;
    int numNavic = 0;
};

/**
 * @brief Navigation epoch data
 */
struct Epoch  // clang-format off
{
    bool        valid           = false;  // Epoch valid, collection completed

    std::size_t seq             = 0;
    char        str[256]        = { 0 };

    // Fix information
    bool        haveFixType     = false;
    FixType     fixType         = FixType::UNKNOWN;
    bool        fixOk           = false;
    const char* fixTypeStr      = nullptr;

    // Position solution and accuracy estimates
    bool        havePos         = false;
    EpochVec3   posLlh          = {{ 0.0, 0.0, 0.0 }}; // [rad, rad, m]
    DegMinSec   posDmsLat;
    DegMinSec   posDmsLon;
    EpochVec3   posXyz          = {{ 0.0, 0.0, 0.0 }}; // [m, m, m]
    bool        havePosAcc      = false;
    double      posAcc          = 0.0;
    bool        havePosAccHoriz = false;
    double      posAccHoriz     = 0.0;
    bool        havePosAccVert  = false;
    double      posAccVert      = 0.0;

    bool        haveHeightMsl   = false;
    double      heightMsl       = 0.0;
    bool        havePdop        = false;
    float       pDOP            = 0.0f;

    // Velocity solution and accuracy estimates
    bool        haveVel         = false;
    EpochVec3   velNed          = {{ 0.0, 0.0, 0.0 }};
    double      vel2d           = 0.0;
    double      vel3d           = 0.0;
    bool        haveVelAcc      = false;
    double      velAcc          = 0.0;

    // Time solution
    bool        haveTime        = false;  // = !time.IsZero()
    bool        confTime        = false;
    bool        leapsKnown      = false;
    Time        time;
    WnoTow      timeGpsWnoTow;            // [ms] precision
    UtcTime     timeUtc;                  // [ms] precision

    bool        haveTimeAcc     = false;
    double      timeAcc         = 0.0;

    bool        haveGpsWno      = false;
    int         gpsWno          = 0;
    bool        haveGpsTow      = false;
    double      gpsTow          = 0.0f;

    // Receiver clock
    bool        haveClock       = false;
    double      clockBias       = 0.0;
    double      clockDrift      = 0.0;

    // RTK relative position
    bool        haveRelPos      = false;
    double      relPosLen       = 0.0;
    EpochVec3   relPosNed       = {{ 0.0, 0.0, 0.0 }};
    EpochVec3   relPosAcc       = {{ 0.0, 0.0, 0.0 }};

    // Satellite and signal information
    SigInfoList sigs;
    SatInfoList sats;

    // Number of satellites, signals used in navigation solution
    bool        haveNumSigUsed  = false;
    PerGnssCnt  numSigUsed;
    bool        haveNumSatUsed  = false;
    PerGnssCnt  numSatUsed;

    // Histogram of signal levels
    bool        haveSigCnoHist  = false;
    SigCnoHist  sigCnoHistTrk;
    SigCnoHist  sigCnoHistNav;

    // Age of corrections
    bool        haveDiffAge     = false;
    float       diffAge         = 0.0f;

    // Receiver uptime
    Duration    uptime;
    char        uptimeStr[100]  = { 0 };
};  // clang-format on


// ---------------------------------------------------------------------------------------------------------------------

struct CollDetect;
struct CollState;

using EpochPtr = std::unique_ptr<Epoch>;

class EpochCollector
{
   public:
    EpochCollector();
    ~EpochCollector();

    EpochPtr Collect(const fpsdk::common::parser::ParserMsg& msg);

    void Reset();

    private:
    std::unique_ptr<Epoch> coll_;
    std::unique_ptr<CollDetect> detect_;
    std::unique_ptr<CollState> state_;
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_EPOCH_HPP__
