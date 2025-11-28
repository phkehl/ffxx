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

#ifndef __DATABASE_HPP__
#define __DATABASE_HPP__

//
#include "ffgui_inc.hpp"
//
#include <cmath>
#include <mutex>
#include <deque>
#include <functional>
#include <unordered_map>
//
#include <ffxx/epoch.hpp>
using namespace ffxx;
#include <fpsdk_common/ext/eigen_core.hpp>
//

namespace ffgui {
/* ****************************************************************************************************************** */

// PN: no plot, no statistics (for types other than float or double)
// P0: no statistics (must be float or double)
// P1: statistics pass 1 (must be float or double), incrementally updated
// P2: statistics pass 2 (must be float or double), incrementally updated
// PF: statistics (must be float or double), fully updated after every epoch, see _Sync()
#define DATABASE_COLUMNS(_PN_, _P0_, _P1_, _P2_, _PF_) \
    /*  Field                Type     Init   Fmt       Label */ \
    _PN_(seq_,               uint64_t, 0,    NULL,     NULL /* sequence number */                   ) \
    _PN_(time_in,            Time,    ,      NULL,     NULL /* reception/processing time */         ) \
    _PN_(time_fix,           Time,    ,      NULL,     NULL /* epoch time */                        ) \
    _PN_(time_str,           std::string, ,  NULL,     NULL /* epoch time string */                 ) \
    _P0_(time_monotonic,     double,  NAN,   "%.3f",   "Monotonic time [s]"                         ) \
    _P0_(time_posix,         double,  NAN,   "%.3f",   "POSIX time [s]"                             ) \
    _P0_(time_tai,           double,  NAN,   "%.3f",   "TAI time [s]"                               ) \
    _P0_(time_gps_tow,       double,  NAN,   "%.3f",   "GPS time of week [s]"                       ) \
    _P1_(time_acc,           double,  NAN,   "%.3e",   "Time accuracy estimate [s]"                 ) \
    \
    _PN_(fix_type,           FixType, FixType::UNKNOWN, NULL, NULL                                  ) \
    _PN_(fix_ok,             bool,    false, NULL,     NULL                                         ) \
    _PN_(fix_colour,         ImU32, C_FIX_INVALID(), NULL, NULL                                     ) \
    _PN_(fix_str,            const char *, "UNKNOWN", NULL,  NULL                                   ) \
    \
    _P0_(fix_type_val,       float,   NAN,   NULL,     "Fix type [-]"                               ) \
    _P0_(fix_ok_val,         float,   NAN,   NULL,     "Fix OK flag [-]"                            ) \
    \
    _P1_(fix_interval,       float,   NAN,   "%.3f",   "Fix interval [s]"                           ) \
    _P1_(fix_rate,           float,   NAN,   "%.1f",   "Fix rate [Hz]"                              ) \
    _P1_(fix_latency,        float,   NAN,   "%.3f",   "Fix latency [s]"                            ) \
    _P1_(fix_mean_interval,  float,   NAN,   "%.3f",   "Fix average interval (10 fixes window) [s]" ) \
    _P1_(fix_mean_rate,      float,   NAN,   "%.1f",   "Fix average rate (10 fixes window) [Hz]"    ) \
    _P1_(fix_mean_latency,   float,   NAN,   "%.3f",   "Fix average latency (10 fixes window) [s]"  ) \
    \
    _PN_(pos_avail,          bool,    false, NULL,     NULL /* pos_* below have values */           ) \
    _P1_(pos_ecef_x,         double,  NAN,   "%.3f",   "Position ECEF X [m]"                        ) /* pos_ecef_* must be adjacent! */ \
    _P1_(pos_ecef_y,         double,  NAN,   "%.3f",   "Position ECEF Y [m]"                        ) \
    _P1_(pos_ecef_z,         double,  NAN,   "%.3f",   "Position ECEF Z [m]"                        ) \
    _P1_(pos_llh_lat_deg,    double,  NAN,   "%.12f",  "Position LLH latitude [deg]"                ) \
    _P1_(pos_llh_lon_deg,    double,  NAN,   "%.12f",  "Position LLH longitude [deg]"               ) \
    _P1_(pos_llh_lat,        double,  NAN,   NULL,     NULL                                         ) /* pos_llh_* must be adjacent! */ \
    _P1_(pos_llh_lon,        double,  NAN,   NULL,     NULL                                         ) \
    _P1_(pos_llh_height,     double,  NAN,   "%.3f",   "Position LLH height [m]"                    ) \
    _PF_(pos_enu_ref_east,   double,  NAN,   "%.3f",   "Position ENU (ref) East [m]"                ) /* pos_enu_ref_* must be adjacent! */ \
    _PF_(pos_enu_ref_north,  double,  NAN,   "%.3f",   "Position ENU (ref) North [m]"               ) \
    _PF_(pos_enu_ref_up,     double,  NAN,   "%.3f",   "Position ENU (ref) up [m]"                  ) \
    _PF_(pos_enu_mean_east,  double,  NAN,   "%.3f",   "Position ENU (mean) East [m]"               ) /* pos_enu_mean_* must be adjacent! */ \
    _PF_(pos_enu_mean_north, double,  NAN,   "%.3f",   "Position ENU (mean) North [m]"              ) \
    _PF_(pos_enu_mean_up,    double,  NAN,   "%.3f",   "Position ENU (mean) up [m]"                 ) \
    _P1_(pos_acc_3d,         double,  NAN,   "%.3f",   "Position accuracy estimate (3d) [m]"        ) \
    _P1_(pos_acc_horiz,      double,  NAN,   "%.3f",   "Position accuracy estimate (horiz.) [m]"    ) \
    _P1_(pos_acc_vert,       double,  NAN,   "%.3f",   "Position accuracy estimate (vert.) [m]"     ) \
    \
    _PN_(vel_avail,          bool,    false, NULL,     NULL /* vel_* below have values */           ) \
    _P1_(vel_enu_east,       double,  NAN,   "%.3f",   "Velocity ENU East [m/s]"                    ) \
    _P1_(vel_enu_north,      double,  NAN,   "%.3f",   "Velocity ENU North [m/s]"                   ) \
    _P1_(vel_enu_down,       double,  NAN,   "%.3f",   "Velocity ENU down [m/s]"                    ) \
    _P1_(vel_3d,             double,  NAN,   "%.3f",   "Velocity (3d) [m/s]"                        ) \
    _P1_(vel_horiz,          double,  NAN,   "%.3f",   "Velocity (horiz) [m/s]"                     ) \
    \
    _P1_(clock_bias,         double,  NAN,   "%.3e",   "Receiver clock bias [s]"                    ) \
    _P1_(clock_drift,        double,  NAN,   "%.3e",   "Receiver clock drift [s/s]"                 ) \
    \
    _PN_(relpos_avail,       bool,    false, NULL,     NULL                                         ) \
    _P1_(relpos_dist,        double,  NAN,   "%.3f",   "Relative position distance [m]"             ) \
    _P1_(relpos_ned_north,   double,  NAN,   "%.3f",   "Relative position North [m]"                ) /* relpos_ned_* must be adjacent! */ \
    _P1_(relpos_ned_east,    double,  NAN,   "%.3f",   "Relative position East [m]"                 ) \
    _P1_(relpos_ned_down,    double,  NAN,   "%.3f",   "Relative position Down [m]"                 ) \
    \
    _P1_(dop_pdop,           float,   NAN,   "%.2f",   "Position DOP [-]"                           ) \
    \
    _PN_(sol_numsig_avail,   bool,    false, NULL,     NULL                                         ) \
    _P1_(sol_numsig_tot,     float,   NAN,   "%.1f",   "Number of signals used (total) [-]"         ) \
    _P1_(sol_numsig_gps,     float,   NAN,   "%.1f",   "Number of signals used (GPS) [-]"           ) \
    _P1_(sol_numsig_sbas,    float,   NAN,   "%.1f",   "Number of signals used (SBAS) [-]"          ) \
    _P1_(sol_numsig_gal,     float,   NAN,   "%.1f",   "Number of signals used (Galileo) [-]"       ) \
    _P1_(sol_numsig_bds,     float,   NAN,   "%.1f",   "Number of signals used (BeiDou) [-]"        ) \
    _P1_(sol_numsig_qzss,    float,   NAN,   "%.1f",   "Number of signals used (QZSS) [-]"          ) \
    _P1_(sol_numsig_glo,     float,   NAN,   "%.1f",   "Number of signals used (GLONASS) [-]"       ) \
    _P1_(sol_numsig_navic,   float,   NAN,   "%.1f",   "Number of signals used (NavIC) [-]"         ) \
    _PN_(sol_numsat_avail,   bool,    false, NULL,     NULL                                         ) \
    _P1_(sol_numsat_tot,     float,   NAN,   "%.1f",   "Number of satellites used (total) [-]"      ) \
    _P1_(sol_numsat_gps,     float,   NAN,   "%.1f",   "Number of satellites used (GPS) [-]"        ) \
    _P1_(sol_numsat_sbas,    float,   NAN,   "%.1f",   "Number of satellites used (SBAS) [-]"       ) \
    _P1_(sol_numsat_gal,     float,   NAN,   "%.1f",   "Number of satellites used (Galileo) [-]"    ) \
    _P1_(sol_numsat_bds,     float,   NAN,   "%.1f",   "Number of satellites used (BeiDou) [-]"     ) \
    _P1_(sol_numsat_qzss,    float,   NAN,   "%.1f",   "Number of satellites used (QZSS) [-]"       ) \
    _P1_(sol_numsat_glo,     float,   NAN,   "%.1f",   "Number of satellites used (GLONASS) [-]"    ) \
    _P1_(sol_numsat_navic,   float,   NAN,   "%.1f",   "Number of satellites used (NavIC) [-]"      ) \
    \
    _P1_(cno_nav_l1,         float,   NAN,   "%.1f",   "Number of L1 signals used [-]"              ) \
    _P1_(cno_nav_e6,         float,   NAN,   "%.1f",   "Number of E6 signals used [-]"              ) \
    _P1_(cno_nav_l2,         float,   NAN,   "%.1f",   "Number of L2 signals used [-]"              ) \
    _P1_(cno_nav_l5,         float,   NAN,   "%.1f",   "Number of L5 signals used [-]"              ) \
    _P1_(cno_trk_l1,         float,   NAN,   "%.1f",   "Number of L1 signals tracked [-]"           ) \
    _P1_(cno_trk_e6,         float,   NAN,   "%.1f",   "Number of E6 signals tracked [-]"           ) \
    _P1_(cno_trk_l2,         float,   NAN,   "%.1f",   "Number of L2 signals tracked [-]"           ) \
    _P1_(cno_trk_l5,         float,   NAN,   "%.1f",   "Number of L5 signals tracked [-]"           ) \
    \
    _P1_(cno_nav_00,         float,   NAN,   "%.1f",   "Number of signals used (0-4 dBHz) [-]"      ) /* cno_nav_* must be adjacent! */ \
    _P1_(cno_nav_05,         float,   NAN,   "%.1f",   "Number of signals used (5-9 dBHz) [-]"      ) \
    _P1_(cno_nav_10,         float,   NAN,   "%.1f",   "Number of signals used (10-14 dBHz) [-]"    ) \
    _P1_(cno_nav_15,         float,   NAN,   "%.1f",   "Number of signals used (15-19 dBHz) [-]"    ) \
    _P1_(cno_nav_20,         float,   NAN,   "%.1f",   "Number of signals used (20-24 dBHz) [-]"    ) \
    _P1_(cno_nav_25,         float,   NAN,   "%.1f",   "Number of signals used (25-29 dBHz) [-]"    ) \
    _P1_(cno_nav_30,         float,   NAN,   "%.1f",   "Number of signals used (30-34 dBHz) [-]"    ) \
    _P1_(cno_nav_35,         float,   NAN,   "%.1f",   "Number of signals used (35-39 dBHz) [-]"    ) \
    _P1_(cno_nav_40,         float,   NAN,   "%.1f",   "Number of signals used (40-44 dBHz) [-]"    ) \
    _P1_(cno_nav_45,         float,   NAN,   "%.1f",   "Number of signals used (45-49 dBHz) [-]"    ) \
    _P1_(cno_nav_50,         float,   NAN,   "%.1f",   "Number of signals used (50-54 dBHz) [-]"    ) \
    _P1_(cno_nav_55,         float,   NAN,   "%.1f",   "Number of signals used (55- dBHz) [-]"      ) \
    _P1_(cno_trk_00,         float,   NAN,   "%.1f",   "Number of signals tracked (0-4 dBHz) [-]"   ) /* cno_trk_* must be adjacent! */ \
    _P1_(cno_trk_05,         float,   NAN,   "%.1f",   "Number of signals tracked (5-9 dBHz) [-]"   ) \
    _P1_(cno_trk_10,         float,   NAN,   "%.1f",   "Number of signals tracked (10-14 dBHz) [-]" ) \
    _P1_(cno_trk_15,         float,   NAN,   "%.1f",   "Number of signals tracked (15-19 dBHz) [-]" ) \
    _P1_(cno_trk_20,         float,   NAN,   "%.1f",   "Number of signals tracked (20-24 dBHz) [-]" ) \
    _P1_(cno_trk_25,         float,   NAN,   "%.1f",   "Number of signals tracked (25-29 dBHz) [-]" ) \
    _P1_(cno_trk_30,         float,   NAN,   "%.1f",   "Number of signals tracked (30-34 dBHz) [-]" ) \
    _P1_(cno_trk_35,         float,   NAN,   "%.1f",   "Number of signals tracked (35-39 dBHz) [-]" ) \
    _P1_(cno_trk_40,         float,   NAN,   "%.1f",   "Number of signals tracked (40-44 dBHz) [-]" ) \
    _P1_(cno_trk_45,         float,   NAN,   "%.1f",   "Number of signals tracked (45-49 dBHz) [-]" ) \
    _P1_(cno_trk_50,         float,   NAN,   "%.1f",   "Number of signals tracked (50-54 dBHz) [-]" ) \
    _P1_(cno_trk_55,         float,   NAN,   "%.1f",   "Number of signals tracked (55- dBHz) [-]"   ) \
    \
    _P1_(cno_avg_top5_l1,    float,   NAN,   "%.1f",   "Average of 5 strongest signals (L1) [dBHz]" ) \
    _P1_(cno_avg_top5_e6,    float,   NAN,   "%.1f",   "Average of 5 strongest signals (E6) [dBHz]" ) \
    _P1_(cno_avg_top5_l2,    float,   NAN,   "%.1f",   "Average of 5 strongest signals (L2) [dBHz]" ) \
    _P1_(cno_avg_top5_l5,    float,   NAN,   "%.1f",   "Average of 5 strongest signals (L5) [dBHz]" ) \
    _P1_(cno_avg_top10_l1,   float,   NAN,   "%.1f",   "Average of 10 strongest signals (L1) [dBHz]" ) \
    _P1_(cno_avg_top10_e6,   float,   NAN,   "%.1f",   "Average of 10 strongest signals (E6) [dBHz]" ) \
    _P1_(cno_avg_top10_l2,   float,   NAN,   "%.1f",   "Average of 10 strongest signals (L2) [dBHz]" ) \
    _P1_(cno_avg_top10_l5,   float,   NAN,   "%.1f",   "Average of 10 strongest signals (L5) [dBHz]" ) \
    /* end*/


#define _DB_ROW_FIELDS(  _field_, _type_, _init_, _fmt_, _label_) _type_ _field_;
#define _DB_ROW_CTOR(    _field_, _type_, _init_, _fmt_, _label_) _field_ { _init_ },
#define _DB_STATS_FIELDS(_field_, _type_, _init_, _fmt_, _label_) Stats _field_;
#define _DB_FIELDS_ENUM( _field_, _type_, _init_, _fmt_, _label_) CONCAT(ix_, _field_),
#define _DB_SKIP(        _field_, _type_, _init_, _fmt_, _label_) /* nothing */
#define _DB_COUNT(       _field_, _type_, _init_, _fmt_, _label_) + 1
#define _DB_FIELD_INFO(  _field_, _type_, _init_, _fmt_, _label_) { _label_, # _field_, _fmt_, CONCAT(ix_, _field_) },
// undef'd at the end of this file

class Database
{
   public:
    Database(const std::string& name);
    ~Database();

    // clang-format off
    enum FieldIx { ix_none, DATABASE_COLUMNS(_DB_SKIP, _DB_FIELDS_ENUM, _DB_FIELDS_ENUM, _DB_FIELDS_ENUM, _DB_FIELDS_ENUM) };
    struct FieldDef { const char* label; const char* name; const char* fmt; FieldIx field; };
    static constexpr std::size_t NUM_FIELDS = 0 DATABASE_COLUMNS(_DB_SKIP, _DB_COUNT, _DB_COUNT, _DB_COUNT, _DB_COUNT);
    static constexpr std::array<FieldDef, NUM_FIELDS> FIELDS = {{ DATABASE_COLUMNS(_DB_SKIP, _DB_FIELD_INFO, _DB_FIELD_INFO, _DB_FIELD_INFO, _DB_FIELD_INFO) }};
    // clang-format on

    // One row in the database
    struct Row
    {  // clang-format off
        Row() : DATABASE_COLUMNS(_DB_ROW_CTOR, _DB_ROW_CTOR, _DB_ROW_CTOR, _DB_ROW_CTOR, _DB_ROW_CTOR) dummy{0} {}

        // All fields, e.g.:
        // uint32_t    time_ts
        // EPOCH_FIX_T fix_type;
        // double       pos_ecef_x;
        // ...
        DATABASE_COLUMNS(_DB_ROW_FIELDS, _DB_ROW_FIELDS, _DB_ROW_FIELDS, _DB_ROW_FIELDS, _DB_ROW_FIELDS)
        int dummy;

        // Access fields by enum value (no "PN" fields, ony P0, P1, P2)
        // FieldIx: ix_fix_type_val, ix_pos_ecef_x, etc.
        double operator[](const FieldIx field) const;

        Epoch epoch_;
    };  // clang-format on

    // Statistics for a database value
    struct Stats  // clang-format off
    {
        double min   = NAN;  // Minimum value (NAN if count < 1)
        double max   = NAN;  // Maximum value (NAN if count < 1)
        double mean  = NAN;  // Mean value (NAN if count < 1)
        double std   = NAN;  // Standard deviation (NAN if count < 2)
        int    count = 0;    // Number of samples
    };  // clang-format on

    // Database info, statistics, other derived data
    struct Info
    {
        // Statistics for each column (that has statistics)
        struct
        {
            DATABASE_COLUMNS(_DB_SKIP, _DB_SKIP, _DB_STATS_FIELDS, _DB_STATS_FIELDS, _DB_STATS_FIELDS)
            // Stats pos_llh_lat;
            // Stats pos_llh_lon;
            // ...
        } stats;

        // Error ellipse (East-North)
        struct
        {
            double a = NAN;
            double b = NAN;
            double omega = NAN;
        } err_ell;

        static constexpr std::size_t CNO_BINS_NUM = NumOf<SigCnoHist>();
        // Histogram data for number of signals tracked
        struct  // clang-format off
        {
            std::array<float, CNO_BINS_NUM> mins  = { { NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN } };
            std::array<float, CNO_BINS_NUM> maxs  = { { NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN } };
            std::array<float, CNO_BINS_NUM> means = { { NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN } };
            std::array<float, CNO_BINS_NUM> stds  = { { NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN } };
        } cno_trk;  // clang-format on

        // Histogram data for number of signals used
        struct  // clang-format off
        {
            std::array<float, CNO_BINS_NUM> mins  = { { NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN } };
            std::array<float, CNO_BINS_NUM> maxs  = { { NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN } };
            std::array<float, CNO_BINS_NUM> means = { { NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN } };
            std::array<float, CNO_BINS_NUM> stds  = { { NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN } };
        } cno_nav;  // clang-format on

        // Histogram bins
        //static const std::array<float, CNO_BINS_NUM> CNO_BINS_LO;  // lower bound
        // - lower bound
        static constexpr std::array<float, CNO_BINS_NUM> CNO_BINS_LO = /* clang-format off */ {{
            IxToCnoLo( 0),
            IxToCnoLo( 1),
            IxToCnoLo( 2),
            IxToCnoLo( 3),
            IxToCnoLo( 4),
            IxToCnoLo( 5),
            IxToCnoLo( 6),
            IxToCnoLo( 7),
            IxToCnoLo( 8),
            IxToCnoLo( 9),
            IxToCnoLo(10),
            IxToCnoLo(11),
        }}; // clang-format on
        // - bin centre
        static constexpr std::array<float, CNO_BINS_NUM> CNO_BINS_MI = /* clang-format off */ {{
            IxToCnoLo( 0) + (0.5f * (IxToCnoHi( 0) - IxToCnoLo( 0) + 1.0f)),
            IxToCnoLo( 1) + (0.5f * (IxToCnoHi( 1) - IxToCnoLo( 1) + 1.0f)),
            IxToCnoLo( 2) + (0.5f * (IxToCnoHi( 2) - IxToCnoLo( 2) + 1.0f)),
            IxToCnoLo( 3) + (0.5f * (IxToCnoHi( 3) - IxToCnoLo( 3) + 1.0f)),
            IxToCnoLo( 4) + (0.5f * (IxToCnoHi( 4) - IxToCnoLo( 4) + 1.0f)),
            IxToCnoLo( 5) + (0.5f * (IxToCnoHi( 5) - IxToCnoLo( 5) + 1.0f)),
            IxToCnoLo( 6) + (0.5f * (IxToCnoHi( 6) - IxToCnoLo( 6) + 1.0f)),
            IxToCnoLo( 7) + (0.5f * (IxToCnoHi( 7) - IxToCnoLo( 7) + 1.0f)),
            IxToCnoLo( 8) + (0.5f * (IxToCnoHi( 8) - IxToCnoLo( 8) + 1.0f)),
            IxToCnoLo( 9) + (0.5f * (IxToCnoHi( 9) - IxToCnoLo( 9) + 1.0f)),
            IxToCnoLo(10) + (0.5f * (IxToCnoHi(10) - IxToCnoLo(10) + 1.0f)),
            IxToCnoLo(11) + (0.5f * (IxToCnoHi(11) - IxToCnoLo(11) + 1.0f)),
        }}; // clang-format on
    };

    const std::string& GetName() const;
    void Clear();

    Info GetInfo();
    const Row& LatestRow();

    // Database stize
    // clang-format off
    static constexpr int DB_SIZE_MIN =   1000;
    static constexpr int DB_SIZE_DEF =  20000;
    static constexpr int DB_SIZE_MAX = 100000;
    int Size() const { return cfg_.dbSize; }
    int Usage() const { return rows_.size(); }
    void SetSize(const int size = DB_SIZE_DEF);
    // clang-format on

    bool Changed(const void* uid);

    inline DataSrcDst GetDataSrcDst() { return this; }

    // Reference position
    enum class RefPos
    {
        MEAN,
        LAST,
        USER
    };
    RefPos GetRefPos() const;
    RefPos GetRefPos(Eigen::Vector3d& llh, Eigen::Vector3d& xyz) const;
    void SetRefPosMean();
    void SetRefPosLast();
    void SetRefPosLlh(const Eigen::Vector3d& llh);
    void SetRefPosXyz(const Eigen::Vector3d& xyz);

    // Process data
    void ProcRows(std::function<bool(const Row&)> cb, const bool backwards = false);
    void BeginGetRows();                        // Lock mutex
    const Row& operator[](const int ix) const;  // Access rows
    const Row& GetRow(const int ix) const;      // Access rows
    void EndGetRows();                          // Unlock mutex

    // Add data
    void AddEpoch(const DataEpoch& inEpoch, const bool isRealTime);

   private:
    std::string name_;

    struct Config
    {
        int dbSize = DB_SIZE_DEF;
        RefPos refPos = RefPos::MEAN;
        Eigen::Vector3d refPosXyz = { 0.0, 0.0, 0.0 };
        Eigen::Vector3d refPosLlh = { 0.0, 0.0, 0.0 };
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, dbSize, refPos, refPosXyz, refPosLlh)
    Config cfg_;

    std::mutex mutex_;
    std::size_t serial_ = 0;
    std::unordered_map<const void*, std::size_t> changed_;
    std::deque<Row> rows_;  // Database rows of values (rows = epochs)
    Time rowsT0_;           // Start time for Row.time_monotonic
    Info info_;             // Info (statistics, error ellipse, ...)
    uint64_t seq_ = 0;

    struct StatsAcc;
    std::unique_ptr<StatsAcc> statsAcc_;

    void _ProcRows(std::function<bool(const Row&)> cb, const bool backwards = false);
    void _Sync();
};

#undef _DB_ROW_FIELDS
#undef _DB_ROW_CTOR
#undef _DB_STATS_FIELDS
#undef _DB_SKIP
#undef _DB_COUNT
#undef _DB_FIELD_INFO

using DatabasePtr = std::shared_ptr<Database>;

/* ****************************************************************************************************************** */
} // ffgui
#endif // __DATABASE_HPP__
