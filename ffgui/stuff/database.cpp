/* ****************************************************************************************************************** */
// flipflip's gui (ffgui)
//
// Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
// https://oinkzwurgl.org/projaeggd/ffxx/
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>.

//
#include "ffgui_inc.hpp"
//
#include <cfloat>
#include <cstring>
#include <limits>
#include <numeric>
//
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/extended_p_square.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
//
#include <fpsdk_common/trafo.hpp>
//
#include "database.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

// using namespace ffxx;
using namespace fpsdk::common::trafo;

Database::Database(const std::string& name) /* clang-format off */ :
    name_   { name }  // clang-format on
{
    DEBUG("Database(%s)", name_.c_str());
    GuiGlobal::LoadObj(name_ + ".Database", cfg_);
    Clear();
}

Database::~Database()
{
    DEBUG("~Database(%s)", name_.c_str());
    GuiGlobal::SaveObj(name_ + ".Database", cfg_);
}

#define _DB_SKIP(_field_, _type_, _init_, _fmt_, _label_) /* nothing */

// ---------------------------------------------------------------------------------------------------------------------

const std::string& Database::GetName() const
{
    return name_;
}

// ---------------------------------------------------------------------------------------------------------------------

void Database::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    rows_.clear();
    rowsT0_ = {};
    info_ = Info();
    statsAcc_ = std::make_unique<StatsAcc>();
    serial_++;
    CollectData(std::make_shared<DataEvent>(this, DataEvent::DBUPDATE));
}

// ---------------------------------------------------------------------------------------------------------------------

void Database::SetSize(const int size)
{
    std::lock_guard<std::mutex> lock(mutex_);
    cfg_.dbSize = std::clamp(size, DB_SIZE_MIN, DB_SIZE_MAX);
    _Sync();
}

// ---------------------------------------------------------------------------------------------------------------------

bool Database::Changed(const void* uid)
{
    bool res = false;
    auto entry = changed_.find(uid);
    if (entry != changed_.end()) {
        if (entry->second != serial_) {
            res = true;
            entry->second = serial_;
        }
    } else {
        changed_.emplace(uid, serial_);
        res = true;
    }
    return res;
}

// ---------------------------------------------------------------------------------------------------------------------

Database::Info Database::GetInfo()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return info_;
}

// ---------------------------------------------------------------------------------------------------------------------

const Database::Row& Database::LatestRow()
{
    static Row empty;
    return rows_.empty() ? empty : rows_.back();
}

// ---------------------------------------------------------------------------------------------------------------------

enum Database::RefPos Database::GetRefPos() const
{
    //std::lock_guard<std::mutex> lock(mutex_);
    return cfg_.refPos;
}

enum Database::RefPos Database::GetRefPos(Eigen::Vector3d& llh, Eigen::Vector3d& xyz) const
{
    //std::lock_guard<std::mutex> lock(mutex_);
    llh = cfg_.refPosLlh;
    xyz = cfg_.refPosXyz;
    return cfg_.refPos;
}

void Database::SetRefPosMean()
{
    std::lock_guard<std::mutex> lock(mutex_);
    cfg_.refPos = RefPos::MEAN;
    _Sync();
}

void Database::SetRefPosLast()
{
    std::lock_guard<std::mutex> lock(mutex_);
    cfg_.refPos = RefPos::LAST;
    _Sync();
}

void Database::SetRefPosLlh(const Eigen::Vector3d& llh)
{
    std::lock_guard<std::mutex> lock(mutex_);
    cfg_.refPos = RefPos::USER;
    cfg_.refPosLlh = llh;
    cfg_.refPosXyz = TfEcefWgs84Llh(cfg_.refPosLlh);
    _Sync();
}

void Database::SetRefPosXyz(const Eigen::Vector3d& xyz)
{
    std::lock_guard<std::mutex> lock(mutex_);
    cfg_.refPos = RefPos::USER;
    cfg_.refPosXyz = xyz;
    cfg_.refPosLlh = TfWgs84LlhEcef(cfg_.refPosXyz);
    _Sync();
}

// ---------------------------------------------------------------------------------------------------------------------

void Database::ProcRows(std::function<bool(const Row&)> cb, const bool backwards)
{
    std::lock_guard<std::mutex> lock(mutex_);
    _ProcRows(cb, backwards);
}

void Database::_ProcRows(std::function<bool(const Row&)> cb, const bool backwards)
{
    bool abort = false;
    if (!backwards) {
        for (auto entry = rows_.cbegin(); !abort && (entry != rows_.cend()); entry++) {
            if (!cb(*entry)) {
                abort = true;
            }
        }
    } else {
        for (auto entry = rows_.crbegin(); !abort && (entry != rows_.crend()); entry++) {
            if (!cb(*entry)) {
                abort = true;
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void Database::BeginGetRows()
{
    mutex_.lock();
}

const Database::Row& Database::GetRow(const int ix) const
{
    return rows_[ix];
}

const Database::Row& Database::operator[](const int ix) const
{
    return rows_[ix];
}

void Database::EndGetRows()
{
    mutex_.unlock();
}

// ---------------------------------------------------------------------------------------------------------------------

#define _DB_SWITCH_CASE( _field_, _type_, _init_, _fmt_, _label_) case CONCAT(ix_, _field_): return (double)_field_;

double Database::Row::operator[](const FieldIx field) const
{
    switch (field)
    {
        DATABASE_COLUMNS(_DB_SKIP, _DB_SWITCH_CASE, _DB_SWITCH_CASE, _DB_SWITCH_CASE, _DB_SWITCH_CASE)
        default: return NAN;
    }
}

#undef _DB_SWITCH_CASE

// ---------------------------------------------------------------------------------------------------------------------

void Database::AddEpoch(const DataEpoch& inEpoch, const bool isRealTime)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Row row;
    row.epoch_ = inEpoch.epoch_;
    const Epoch& epoch = row.epoch_;

    row.time_in = inEpoch.time_;
    row.seq_ = ++seq_;

    // Monotonic time only for real-time input
    if (isRealTime) {
        if (rowsT0_.IsZero()) {
            rowsT0_ = inEpoch.time_;
        }
        row.time_monotonic = (inEpoch.time_ - rowsT0_).GetSec();
    }
    // But we can fake it for logfiles
    else if (epoch.haveTime) {
        if (rowsT0_.IsZero()) {
            rowsT0_ = epoch.time;
        }
        row.time_monotonic = (epoch.time - rowsT0_).GetSec();
    }

    if (epoch.haveTime) {
        row.time_fix = epoch.time;
        row.time_posix = epoch.time.GetPosixSec();
        row.time_tai = epoch.time.GetTaiSec();
        row.time_str = epoch.time.StrUtcTime(3);
    }

    if (epoch.haveGpsTow) {
        row.time_gps_tow = epoch.gpsTow;
    } else if (epoch.haveTime) {
        row.time_gps_tow = epoch.time.GetWnoTow(WnoTow::Sys::GPS).tow_;
    }

    row.time_acc = epoch.timeAcc;

    row.fix_type = epoch.fixType;
    row.fix_ok = epoch.fixOk;
    row.fix_colour = FIX_COLOUR(epoch.fixType, epoch.fixOk);
    row.fix_type_val = EnumToVal(epoch.fixType);
    row.fix_ok_val = (epoch.fixOk ? 1.0 : 0.0);
    row.fix_str = epoch.fixTypeStr;

    if (epoch.haveTime) {
        // Find last row with fix, calculate mean interval and rate
        // FIXME: is this all correct?!
        auto lastFixRow1 = rows_.rend();
        auto lastFixRow10 = rows_.rend();
        int numFixes = 0;
        float latencySum = 0.0f;
        int latencyNum = 0;
        for (auto cand = rows_.rbegin(); (cand != rows_.rend()) && (numFixes <= 9); cand++) {
            if (cand->fix_type > FixType::NOFIX) {
                numFixes++;
                switch (numFixes) { // clang-format off
                    case  1: lastFixRow1  = cand; break;
                    case 10: lastFixRow10 = cand; break;
                } // clang-format on
                if (!std::isnan(cand->fix_latency)) {
                    latencySum += cand->fix_latency;
                    latencyNum++;
                }
            }
        }
        if (lastFixRow1 != rows_.rend()) {
            row.fix_interval = (row.time_posix - lastFixRow1->time_posix);
            if (row.fix_interval > 0.0f) {
                row.fix_rate = 1.0f / row.fix_interval;
            }
            if (lastFixRow10 != rows_.rend()) {
                row.fix_mean_interval = (row.time_posix - lastFixRow10->time_posix) / 10.0f;
            }
            if (row.fix_interval > 0.0f) {
                row.fix_mean_rate = 1.0f / row.fix_mean_interval;
            }
        }
        if (isRealTime) {
            row.fix_latency = (inEpoch.time_ - epoch.time).GetSec();
            if (latencyNum > 5) {
                row.fix_mean_latency = (latencySum + row.fix_latency) / (float)(latencyNum + 1);
            }
        }
    }

    if (epoch.havePos) {
        row.pos_avail = true;
        row.pos_ecef_x = epoch.posXyz[0];
        row.pos_ecef_y = epoch.posXyz[1];
        row.pos_ecef_z = epoch.posXyz[2];
        row.pos_llh_lat_deg = RadToDeg(epoch.posLlh[0]);
        row.pos_llh_lon_deg = RadToDeg(epoch.posLlh[1]);
        row.pos_llh_lat = epoch.posLlh[0];
        row.pos_llh_lon = epoch.posLlh[1];
        row.pos_llh_height = epoch.posLlh[2];
    }
    if (epoch.havePosAcc) {
        row.pos_acc_3d = epoch.posAcc;
    }
    if (epoch.havePosAccHoriz) {
        row.pos_acc_horiz = epoch.posAccHoriz;
    }
    if (epoch.havePosAccVert) {
        row.pos_acc_vert = epoch.posAccVert;
    }

    if (epoch.haveVel) {
        row.vel_avail = true;
        row.vel_enu_east = epoch.velNed[1];
        row.vel_enu_north = epoch.velNed[0];
        row.vel_enu_down = -epoch.velNed[2];
        row.vel_3d = epoch.vel3d;
        row.vel_horiz = epoch.vel2d;
    }

    if (epoch.haveClock) {
        row.clock_bias = epoch.clockBias;
        row.clock_drift = epoch.clockDrift;
    }

    if (epoch.haveRelPos) {
        row.relpos_avail = true;
        row.relpos_dist = epoch.relPosLen;
        row.relpos_ned_north = epoch.relPosNed[0];
        row.relpos_ned_east = epoch.relPosNed[1];
        row.relpos_ned_down = epoch.relPosNed[2];
    }

    if (epoch.havePdop) {
        row.dop_pdop = epoch.pDOP;
    }

    if (epoch.haveNumSigUsed) {
        row.sol_numsig_avail = true;
        row.sol_numsig_tot = epoch.numSigUsed.numTotal;
        row.sol_numsig_gps = epoch.numSigUsed.numGps;
        row.sol_numsig_sbas = epoch.numSigUsed.numSbas;
        row.sol_numsig_gal = epoch.numSigUsed.numGal;
        row.sol_numsig_bds = epoch.numSigUsed.numBds;
        row.sol_numsig_qzss = epoch.numSigUsed.numQzss;
        row.sol_numsig_glo = epoch.numSigUsed.numGlo;
        row.sol_numsig_navic = epoch.numSigUsed.numNavic;
    }
    if (epoch.haveNumSatUsed) {
        row.sol_numsat_avail = true;
        row.sol_numsat_tot = epoch.numSatUsed.numTotal;
        row.sol_numsat_gps = epoch.numSatUsed.numGps;
        row.sol_numsat_sbas = epoch.numSatUsed.numSbas;
        row.sol_numsat_gal = epoch.numSatUsed.numGal;
        row.sol_numsat_bds = epoch.numSatUsed.numBds;
        row.sol_numsat_qzss = epoch.numSatUsed.numQzss;
        row.sol_numsat_glo = epoch.numSatUsed.numGlo;
        row.sol_numsat_navic = epoch.numSatUsed.numNavic;
    }

    if (epoch.haveSigCnoHist) {
        static_assert(NumOf<SigCnoHist>() == 12); // we'll have to update our code if Epoch.h changes...
        row.cno_nav_00 = epoch.sigCnoHistNav[0];
        row.cno_nav_05 = epoch.sigCnoHistNav[1];
        row.cno_nav_10 = epoch.sigCnoHistNav[2];
        row.cno_nav_15 = epoch.sigCnoHistNav[3];
        row.cno_nav_20 = epoch.sigCnoHistNav[4];
        row.cno_nav_25 = epoch.sigCnoHistNav[5];
        row.cno_nav_30 = epoch.sigCnoHistNav[6];
        row.cno_nav_35 = epoch.sigCnoHistNav[7];
        row.cno_nav_40 = epoch.sigCnoHistNav[8];
        row.cno_nav_45 = epoch.sigCnoHistNav[9];
        row.cno_nav_50 = epoch.sigCnoHistNav[10];
        row.cno_nav_55 = epoch.sigCnoHistNav[11];
        row.cno_trk_00 = epoch.sigCnoHistTrk[0];
        row.cno_trk_05 = epoch.sigCnoHistTrk[1];
        row.cno_trk_10 = epoch.sigCnoHistTrk[2];
        row.cno_trk_15 = epoch.sigCnoHistTrk[3];
        row.cno_trk_20 = epoch.sigCnoHistTrk[4];
        row.cno_trk_25 = epoch.sigCnoHistTrk[5];
        row.cno_trk_30 = epoch.sigCnoHistTrk[6];
        row.cno_trk_35 = epoch.sigCnoHistTrk[7];
        row.cno_trk_40 = epoch.sigCnoHistTrk[8];
        row.cno_trk_45 = epoch.sigCnoHistTrk[9];
        row.cno_trk_50 = epoch.sigCnoHistTrk[10];
        row.cno_trk_55 = epoch.sigCnoHistTrk[11];
    }

    if (!epoch.sigs.empty()) { // clang-format off
        std::vector<float> cnoL1;
        std::vector<float> cnoE6;
        std::vector<float> cnoL2;
        std::vector<float> cnoL5;
        row.cno_nav_l1 = 0;
        row.cno_nav_e6 = 0;
        row.cno_nav_l2 = 0;
        row.cno_nav_l5 = 0;
        row.cno_trk_l1 = 0;
        row.cno_trk_e6 = 0;
        row.cno_trk_l2 = 0;
        row.cno_trk_l5 = 0;
        for (const auto &sig: epoch.sigs) {
            if (sig.cno > 0.0f) {
                switch (sig.band) {
                    case Band::L1: cnoL1.push_back(sig.cno); if (sig.use >= SigUse::CODELOCK) { row.cno_nav_l1++; } if (sig.use >= SigUse::ACQUIRED) { row.cno_trk_l1++; } break;
                    case Band::E6: cnoE6.push_back(sig.cno); if (sig.use >= SigUse::CODELOCK) { row.cno_nav_e6++; } if (sig.use >= SigUse::ACQUIRED) { row.cno_trk_e6++; } break;
                    case Band::L2: cnoL2.push_back(sig.cno); if (sig.use >= SigUse::CODELOCK) { row.cno_nav_l2++; } if (sig.use >= SigUse::ACQUIRED) { row.cno_trk_l2++; } break;
                    case Band::L5: cnoL5.push_back(sig.cno); if (sig.use >= SigUse::CODELOCK) { row.cno_nav_l5++; } if (sig.use >= SigUse::ACQUIRED) { row.cno_trk_l5++; } break;
                    case Band::UNKNOWN: break;
                }
            }
        }
        std::sort(cnoL1.rbegin(), cnoL1.rend());
        std::sort(cnoE6.rbegin(), cnoE6.rend());
        std::sort(cnoL2.rbegin(), cnoL2.rend());
        std::sort(cnoL5.rbegin(), cnoL5.rend());
        if (cnoL1.size() >=  5) { row.cno_avg_top5_l1  = std::accumulate(cnoL1.begin(), cnoL1.begin() +  5, 0.0f) /  5.0f; }
        if (cnoE6.size() >=  5) { row.cno_avg_top5_e6  = std::accumulate(cnoE6.begin(), cnoE6.begin() +  5, 0.0f) /  5.0f; }
        if (cnoL2.size() >=  5) { row.cno_avg_top5_l2  = std::accumulate(cnoL2.begin(), cnoL2.begin() +  5, 0.0f) /  5.0f; }
        if (cnoL5.size() >=  5) { row.cno_avg_top5_l5  = std::accumulate(cnoL5.begin(), cnoL5.begin() +  5, 0.0f) /  5.0f; }
        if (cnoL1.size() >= 10) { row.cno_avg_top10_l1 = std::accumulate(cnoL1.begin(), cnoL1.begin() + 10, 0.0f) / 10.0f; }
        if (cnoE6.size() >= 10) { row.cno_avg_top10_e6 = std::accumulate(cnoE6.begin(), cnoE6.begin() + 10, 0.0f) / 10.0f; }
        if (cnoL2.size() >= 10) { row.cno_avg_top10_l2 = std::accumulate(cnoL2.begin(), cnoL2.begin() + 10, 0.0f) / 10.0f; }
        if (cnoL5.size() >= 10) { row.cno_avg_top10_l5 = std::accumulate(cnoL5.begin(), cnoL5.begin() + 10, 0.0f) / 10.0f; }
    } // clang-format on

    rows_.push_back(row);

    _Sync();
}

// ---------------------------------------------------------------------------------------------------------------------

// clang-format off
using Accumulator = boost::accumulators::accumulator_set<double, boost::accumulators::stats<
    boost::accumulators::tag::count,
    boost::accumulators::tag::mean(boost::accumulators::lazy),
    boost::accumulators::tag::min,
    boost::accumulators::tag::max,
    boost::accumulators::tag::variance(boost::accumulators::lazy)>>;
// clang-format on

// Define accumulator                  Accumulator acc_foo;
#define _DB_ST_ACC_DEF(_field_, _type_, _init_, _fmt_, _label_) Accumulator CONCAT(acc_, _field_);

// Reset accumulator                   statsAcc.acc_foo = {};
#define _DB_ST_ACC_RST(_field_, _type_, _init_, _fmt_, _label_) CONCAT(statsAcc.acc_, _field_) = { };

// Add data to accumulator             acc_foo(value);
// Store result from accumulator       count = boost::accumulators::count(acc_foo); ...
#define _DB_ST_ACC_UPD(_var_, _field_, _type_, _init_, _fmt_, _label_)                                      \
    if (std::isfinite(_var_._field_)) {                                                          \
        CONCAT(statsAcc.acc_, _field_)(_var_._field_);                                           \
    }                                                                                                \
    info.stats._field_.count = boost::accumulators::count(CONCAT(statsAcc.acc_, _field_));           \
    if (info.stats._field_.count > 0) {                                                              \
        info.stats._field_.min = boost::accumulators::min(CONCAT(statsAcc.acc_, _field_));           \
        info.stats._field_.max = boost::accumulators::max(CONCAT(statsAcc.acc_, _field_));           \
        info.stats._field_.mean = boost::accumulators::mean(CONCAT(statsAcc.acc_, _field_));         \
    }                                                                                                \
    if (info.stats._field_.count > 9) {                                                              \
        const double std = std::sqrt(boost::accumulators::variance(CONCAT(statsAcc.acc_, _field_))); \
        info.stats._field_.std = (std::isfinite(std) ? std : 0.0);                                   \
    }

#define _DB_ST_ACC_UPD_LATEST(_field_, _type_, _init_, _fmt_, _label_) _DB_ST_ACC_UPD(latestRow, _field_, _type_, _init_, _fmt_, _label_)
#define _DB_ST_ACC_UPD_ROW(_field_, _type_, _init_, _fmt_, _label_) _DB_ST_ACC_UPD(row, _field_, _type_, _init_, _fmt_, _label_)

struct Database::StatsAcc
{
    DATABASE_COLUMNS(_DB_SKIP, _DB_SKIP, _DB_ST_ACC_DEF, _DB_ST_ACC_DEF, _DB_ST_ACC_DEF);
};

void Database::_Sync()
{
    while ((int)rows_.size() > cfg_.dbSize) {
        rows_.pop_front();
    }

    Info info;
    StatsAcc& statsAcc = *statsAcc_;
    const auto& latestRow = rows_.back();

    // Calculate statistics (pass 1 variables)
    DATABASE_COLUMNS(_DB_SKIP, _DB_SKIP, _DB_ST_ACC_UPD_LATEST, _DB_SKIP, _DB_SKIP);

    // Update user's selected reference position for row.pos_enu_ref_*
    switch (cfg_.refPos)
    {
        // Use mean position from statistics calculated above
        case RefPos::MEAN:
        {
            cfg_.refPosXyz[0] = info.stats.pos_ecef_x.mean;
            cfg_.refPosXyz[1] = info.stats.pos_ecef_y.mean;
            cfg_.refPosXyz[2] = info.stats.pos_ecef_z.mean;
            cfg_.refPosLlh = TfWgs84LlhEcef(cfg_.refPosXyz); // xyz2llh
            break;
        }
        // Use the last position
        case RefPos::LAST:
        {
            _ProcRows([&](const Row &row) {
                if (row.pos_avail) {
                    cfg_.refPosXyz[0] = row.pos_ecef_x;
                    cfg_.refPosXyz[1] = row.pos_ecef_y;
                    cfg_.refPosXyz[2] = row.pos_ecef_z;
                    cfg_.refPosLlh = TfWgs84LlhEcef(cfg_.refPosXyz); // xyz2llh
                    return false;
                }
                return true;
            }, true);
            break;
        }
        case RefPos::USER:
            // cfg_.refPosXyz/cfg_.refPosLlh set by user
            break;
    }

    // Calculate row.pos_enu_ref_* (ENU relative to user's selected reference position)
    const Eigen::Matrix3d rot_ref = RotEnuEcef(cfg_.refPosLlh[0], cfg_.refPosLlh[1]);
    // Calculate row.pos_enu_mean_* (ENU relative to mean position)
    const Eigen::Matrix3d rot_mean = RotEnuEcef(info.stats.pos_llh_lat.mean, info.stats.pos_llh_lon.mean);
    const Eigen::Vector3d ref_xyz(info.stats.pos_ecef_x.mean, info.stats.pos_ecef_y.mean, info.stats.pos_ecef_z.mean);
    // Also, re-calculate these stats completely every time
    DATABASE_COLUMNS(_DB_SKIP, _DB_SKIP, _DB_SKIP, _DB_SKIP, _DB_ST_ACC_RST);
    statsAcc.acc_pos_enu_mean_east = {};
    for (auto& row : rows_) {
        if (!std::isnan(row.pos_ecef_x)) {
            Eigen::Map<Eigen::Vector3d>(&row.pos_enu_ref_east) = rot_ref * (Eigen::Map<Eigen::Vector3d>(&row.pos_ecef_x) - cfg_.refPosXyz);
            Eigen::Map<Eigen::Vector3d>(&row.pos_enu_mean_east) = rot_mean * (Eigen::Map<Eigen::Vector3d>(&row.pos_ecef_x) - ref_xyz);
            DATABASE_COLUMNS(_DB_SKIP, _DB_SKIP, _DB_SKIP, _DB_SKIP, _DB_ST_ACC_UPD_ROW);
        }
    }

    // Calculate statistics (pass 2 variables)
    DATABASE_COLUMNS(_DB_SKIP, _DB_SKIP, _DB_SKIP, _DB_ST_ACC_UPD_LATEST, _DB_SKIP);

    // Calculate East/North error ellipse
    if (info.stats.pos_enu_mean_east.count > 2) {
        double corr_en = 0.0;
        for (const auto &row: rows_) {
            if (row.pos_avail) {
                corr_en += (row.pos_enu_mean_east * row.pos_enu_mean_north);
            }
        }
        const auto &s_e = info.stats.pos_enu_mean_east;
        const auto &s_n = info.stats.pos_enu_mean_north;
        const double qxx = s_e.std * s_e.std;
        const double qyy = s_n.std * s_n.std;
        const double qxy = (corr_en / (double)s_e.count) - (s_e.mean * s_n.mean);
        const double tmp1 = 0.5 * (qxx - qyy);
        const double tmp2 = std::sqrt( (tmp1 * tmp1) + (qxy * qxy) );
        const double tmp3 = 0.5 * (qxx + qyy);
        const double tmp4 = tmp3 + tmp2;
        const double tmp5 = tmp3 - tmp2;
        info.err_ell.a = std::sqrt(std::max(tmp4, 0.0));
        info.err_ell.b = std::sqrt(std::max(tmp5, 0.0));
        info.err_ell.omega = 0.5 * std::atan2(2.0 * qxy, qxx - qyy);
    }

    // Signal level histogram
#define _HELPER(_to_, _ix_, _from_) \
    _to_.mins[_ix_] = _from_.min;   \
    _to_.maxs[_ix_] = _from_.max;   \
    _to_.means[_ix_] = _from_.mean; \
    _to_.stds[_ix_] = _from_.std;
    // clang-format off
    _HELPER(info.cno_trk,  0, info.stats.cno_trk_00);
    _HELPER(info.cno_trk,  1, info.stats.cno_trk_05);
    _HELPER(info.cno_trk,  2, info.stats.cno_trk_10);
    _HELPER(info.cno_trk,  3, info.stats.cno_trk_15);
    _HELPER(info.cno_trk,  4, info.stats.cno_trk_20);
    _HELPER(info.cno_trk,  5, info.stats.cno_trk_25);
    _HELPER(info.cno_trk,  6, info.stats.cno_trk_30);
    _HELPER(info.cno_trk,  7, info.stats.cno_trk_35);
    _HELPER(info.cno_trk,  8, info.stats.cno_trk_40);
    _HELPER(info.cno_trk,  9, info.stats.cno_trk_45);
    _HELPER(info.cno_trk, 10, info.stats.cno_trk_50);
    _HELPER(info.cno_trk, 11, info.stats.cno_trk_55);
    _HELPER(info.cno_nav,  0, info.stats.cno_nav_00);
    _HELPER(info.cno_nav,  1, info.stats.cno_nav_05);
    _HELPER(info.cno_nav,  2, info.stats.cno_nav_10);
    _HELPER(info.cno_nav,  3, info.stats.cno_nav_15);
    _HELPER(info.cno_nav,  4, info.stats.cno_nav_20);
    _HELPER(info.cno_nav,  5, info.stats.cno_nav_25);
    _HELPER(info.cno_nav,  6, info.stats.cno_nav_30);
    _HELPER(info.cno_nav,  7, info.stats.cno_nav_35);
    _HELPER(info.cno_nav,  8, info.stats.cno_nav_40);
    _HELPER(info.cno_nav,  9, info.stats.cno_nav_45);
    _HELPER(info.cno_nav, 10, info.stats.cno_nav_50);
    _HELPER(info.cno_nav, 11, info.stats.cno_nav_55);
    // clang-format on
#undef _HELPER

    info_ = info;
    serial_++;
    CollectData(std::make_shared<DataEvent>(this, DataEvent::DBUPDATE));
}

#undef _DB_ST_ACC_DEF
#undef _DB_ST_ACC_ACC
#undef _DB_ST_ACC_RES

/* ****************************************************************************************************************** */
}  // namespace ffgui
