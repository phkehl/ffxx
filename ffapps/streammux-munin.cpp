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
 * @brief stream multiplexer munin plugin
 */

/* LIBC/STL */
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <exception>
#include <optional>
#include <string>
#include <vector>

/* EXTERNAL */
#include <ffxx/utils.hpp>
#include <fpsdk_common/app.hpp>
#include <fpsdk_common/path.hpp>
#include <fpsdk_common/string.hpp>
#include <fpsdk_common/time.hpp>
#include <fpsdk_common/types.hpp>
#ifndef NDEBUG // apparently, this doesn't work in release build?!
#  define JSON_DIAGNOSTICS 1
#endif
#include <nlohmann/json.hpp>

/* PACKAGE */

/* ****************************************************************************************************************** */

// Support for std::optional in NLOHMANN_DEFINE_TYPE_... below.
// à la https://json.nlohmann.me/features/arbitrary_types/#how-do-i-convert-third-party-types

NLOHMANN_JSON_NAMESPACE_BEGIN
template <typename T>
struct adl_serializer<std::optional<T>>
{
    static void to_json(json& j, const std::optional<T>& opt)
    {
        if (opt == std::nullopt) {
            j = nullptr;
        } else {
            j = *opt;
        }
    }

    static void from_json(const json& j, std::optional<T>& opt)
    {
        if (j.is_null()) {
            opt = std::nullopt;
        } else {
            opt = j.get<T>();
        }
    }
};
NLOHMANN_JSON_NAMESPACE_END

namespace ffapps::streammux {
/* ****************************************************************************************************************** */

using namespace ffxx;
using namespace fpsdk::common::app;
using namespace fpsdk::common::path;
using namespace fpsdk::common::string;
using namespace fpsdk::common::time;
using namespace fpsdk::common::types;
using namespace nlohmann;

// ---------------------------------------------------------------------------------------------------------------------

// Program options
class SmMuninOptions : public ProgramOptions
{
   public:
    SmMuninOptions() /* clang-format off */ :
        ProgramOptions("streammux-munin",
            { { 'r', true, "report" }, { 'w', false, "nowarn" }, { 'd', false, "days" } })  // clang-format on
    {
        version_str_ = ffxx::GetVersionString();
        copy_str_ = ffxx::GetCopyrightString();
        lic_str_ = ffxx::GetLicenseString();
    }

    enum class Command : int
    {
        FETCH,
        CONFIG
    };
    Command command_ = Command::FETCH;
    std::vector<std::string> reports_;
    bool nowarn_ = false;
    bool days_ = false;

    void PrintHelp() override final
    {
        // clang-format off
        std::fputs(
            "\n"
            "Munin plugin to monitor streammux\n"
            "\n"
            "Usage:\n"
            "\n"
            "    streammux-munin [flags] -r <report.json> [-r ... ] <command>\n"
            "\n"
            "Where:\n"
            "\n", stdout);
        std::fputs(COMMON_FLAGS_HELP, stdout);
        std::fputs(
            "    -r <path>, --report <path> -- StreamMux report file, multiple can be specified\n"
            "    -w, --nowarn               -- Do not generate warning/critical output for <command> 'config')\n"
            "    -d, --days                 -- Use [d] (days) instead of [h] (hours) for uptime graphs\n"
            "    <command is 'fetch' (default) or 'config'\n"
            "\n"
            "\n", stdout);
    }

    bool HandleOption(const Option& option, const std::string& argument) final
    {
        bool ok = true;
        switch (option.flag) {  // clang-format off
            case 'r':
                if (PathIsReadable(argument) && PathIsFile(argument)) {
                    reports_.push_back(argument);
                } else {
                    WARNING("Bad report file path: %s", argument.c_str());
                    ok = false;
                }
                break;
            case 'w':
                nowarn_ = true;
                break;
            case 'd':
                days_ = true;
                break;
            default: ok = false; break;
        }  // clang-format on
        return ok;
    }

    bool CheckOptions(const std::vector<std::string>& args) final
    {
        bool ok = true;

        if (args.size() == 1) {
            if (args[0] == "fetch") {
                command_ = Command::FETCH;
            } else if (args[0] == "config") {
                command_ = Command::CONFIG;
            }
        }
        switch (command_) {
            case Command::FETCH:
            case Command::CONFIG:
                if (reports_.empty()) {
                    ok = false;
                }
                break;
        }

        DEBUG("command = %d", EnumToVal(command_));
        for (std::size_t ix = 0; ix < reports_.size(); ix++) {
            DEBUG("reports[%" PRIuMAX "] = %s", ix, reports_[ix].c_str());
        }
        DEBUG("nowarn = %s", ToStr(nowarn_));

        return ok;
    }
};

/* ****************************************************************************************************************** */

struct SmReportMeas
{
    double mean = 0.0;
    double min = 0.0;
    double max = 0.0;
    double std = 0.0;
    int num = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SmReportMeas, mean, min, max, std, num)

struct SmReportProc
{
    SmReportMeas cpu_stats;
    SmReportMeas mem_stats;
    std::string name;
    time_t time = 0;
    time_t uptime = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SmReportProc, cpu_stats, mem_stats, name, time, uptime)

struct SmReportStrStats
{
    std::size_t n_msgs = 0;
    std::size_t n_filt = 0;
    std::size_t n_other = 0;
    std::size_t s_filt = 0;
    std::size_t s_msgs = 0;
    std::size_t s_other = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SmReportStrStats, n_msgs, n_filt, n_other, s_msgs, s_filt, s_other)

struct SmReportStr
{
    std::string name;
    std::string type;
    std::string disp;
    std::string state;
    int stateage = 0;
    std::array<SmReportStrStats, 2> stats;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SmReportStr, name, state, stateage, stats)

struct SmReportMonStats
{
    double winSize = 0.0;
    std::optional<SmReportMeas> latency;
    std::optional<SmReportMeas> fixType;
    std::optional<SmReportMeas> pDOP;
    std::optional<SmReportMeas> tDOP;
    std::optional<SmReportMeas> gDOP;
    std::optional<SmReportMeas> numSigUsed;
    std::optional<SmReportMeas> numSigUsedL1;
    std::optional<SmReportMeas> numSigUsedE6;
    std::optional<SmReportMeas> numSigUsedL2;
    std::optional<SmReportMeas> numSigUsedL5;
    std::optional<SmReportMeas> numSigUsedGps;
    std::optional<SmReportMeas> numSigUsedSbas;
    std::optional<SmReportMeas> numSigUsedGal;
    std::optional<SmReportMeas> numSigUsedBds;
    std::optional<SmReportMeas> numSigUsedQzss;
    std::optional<SmReportMeas> numSigUsedGlo;
    std::optional<SmReportMeas> numSigUsedNavic;
    std::optional<SmReportMeas> cnoAvgTop5;
    std::optional<SmReportMeas> cnoAvgTop5L1;
    std::optional<SmReportMeas> cnoAvgTop5E6;
    std::optional<SmReportMeas> cnoAvgTop5L2;
    std::optional<SmReportMeas> cnoAvgTop5L5;
    std::optional<SmReportMeas> rateMsgs;
    std::optional<SmReportMeas> rateBytes;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SmReportMonStats, winSize, latency, fixType, pDOP, tDOP, gDOP, numSigUsed,
    numSigUsedL1, numSigUsedE6, numSigUsedL2, numSigUsedL5, numSigUsedGps, numSigUsedSbas, numSigUsedGal, numSigUsedBds,
    numSigUsedQzss, numSigUsedGlo, numSigUsedNavic, cnoAvgTop5, cnoAvgTop5L1, cnoAvgTop5E6, cnoAvgTop5L2, cnoAvgTop5L5,
    rateMsgs, rateBytes)

struct SmReportMon
{
    std::string name;
    std::string src;
    std::vector<SmReportMonStats> stats;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SmReportMon, name, src, stats)

struct SmReport
{
    SmReportProc proc;
    std::vector<SmReportStr> strs;
    std::vector<SmReportMon> mons;
    bool Load(const std::string& file);
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SmReport, proc, strs, mons)

bool SmReport::Load(const std::string& file)
{
    DEBUG("SmReport: Load %s", file.c_str());
    std::vector<uint8_t> data;
    if (!FileSlurp(file, data)) {
        return false;
    }

    try {
        const json report = nlohmann::json::parse(BufToStr(data));
        *this = report;

        if (proc.name.empty()) {
            throw std::runtime_error(".proc.name empty");
        }
        if (strs.empty()) {
            throw std::runtime_error(".strs empty");
        }
    } catch (std::exception& ex) {
        WARNING("Fail load %s: %s", file.c_str(), ex.what());
        return false;
    }

    return true;
}

/* ****************************************************************************************************************** */

class SmMunin
{
   public:
    SmMunin(const SmMuninOptions& opts) /* clang-format off */ :
        opts_   { opts }  // clang-format on
    {
    }

    ~SmMunin()
    {
    }

    bool Run();
    bool DoConfigOrFetch(const SmReport& report, const bool doConfig);

   private:
    SmMuninOptions opts_;
};

// ---------------------------------------------------------------------------------------------------------------------

bool SmMunin::Run()
{
    bool ok = true;
    switch (opts_.command_) {
        case SmMuninOptions::Command::CONFIG:
        case SmMuninOptions::Command::FETCH:
            for (const auto& reportFile : opts_.reports_) {
                SmReport report;
                if (!report.Load(reportFile) ||
                    !DoConfigOrFetch(report, opts_.command_ == SmMuninOptions::Command::CONFIG)) {
                    ok = false;
                }
            }
            break;
    }
    return ok;
}

// ---------------------------------------------------------------------------------------------------------------------

bool SmMunin::DoConfigOrFetch(const SmReport& report, const bool doConfig)
{
    // http://guide.munin-monitoring.org/en/latest/reference/plugin.html
    // http://guide.munin-monitoring.org/en/latest/example/graph/graph_args.html

    const std::string multigraph_name = "streammux_" + StrToLower(report.proc.name);
    DEBUG("Generating %s", multigraph_name.c_str());
    bool ok = true;

    std::array<const char*, 8> C_DIV = /* clang-format off */ {{
        "00CC00", // #00CC00
        "FF8000", // #FF8000
        "FFCC00", // #FFCC00
        "80C9FF", // #80C9FF
        "FFBFFF", // #FFBFFF
        "FF8020", // #FF6060
        "60FFF7", // #60fff7
        "FFE680", // #FFE680
    }};  // clang-format on
    static constexpr const char* C_UPTIME = "008F00"; // #008F00
    static constexpr const char* C_MEAN   = "FF1000"; // #ff1000
    static constexpr const char* C_STD    = "C08000"; // #c08000
    static constexpr const char* C_MINMAX = "0010FF"; // #0010ff
    static constexpr const char* C_MSGS   = "008F00"; // #008F00
    static constexpr const char* C_FILT   = "C08000"; // #c08000
    static constexpr const char* C_OTHER  = "FF1000"; // #ff1000
    static constexpr const char* C_GDOP   = "008F00"; // #008F00
    static constexpr const char* C_PTDOP  = "FF1000"; // #ff1000
    static constexpr const char* C_TOTAL  = "000000"; // #000000

    const double UPTIME_DIV    = (opts_.days_ ? SEC_IN_DAY_D : SEC_IN_HOUR_D);
    const char*  UPTIME_PRINTF = (opts_.days_ ? "%6.2lf"     : "%6.1lf");
    const char*  UPTIME_VLABEL = (opts_.days_ ? "Uptime [d]" : "Uptime [h]");


    // ----- Main graph: process uptime and "connected" uptime for each stream

    std::printf("multigraph %s\n", multigraph_name.c_str());
    if (doConfig) {
        std::printf("graph_title     StreamMux %s\n", report.proc.name.c_str());
        std::printf("graph_category  streammux\n");
        std::printf("graph_args      --base 1000 --lower-limit 0.1 --logarithmic\n");
        std::printf("graph_scale     no\n");
        std::printf("graph_printf    %s\n", UPTIME_PRINTF);
        std::printf("graph_vlabel    %s\n", UPTIME_VLABEL);
        std::printf("graph_info      StreamMux %s process uptime and connected uptime of streams\n", report.proc.name.c_str());
        std::string order;
        for (const auto& str : report.strs) {
            order += " " + StrToLower(str.name);
        }
        std::printf("graph_order    %s uptime\n", order.c_str());

        std::printf("\n");
        std::printf("uptime.label    Process uptime\n");
        std::printf("uptime.draw     LINE2\n");
        std::printf("uptime.colour   %s\n", C_UPTIME);
        std::printf("uptime.cdef     uptime,%.1f,/\n", UPTIME_DIV);

        std::size_t colourIx = 0;
        for (const auto& str : report.strs) {
            const auto name = StrToLower(str.name);
            std::printf("\n");
            std::printf("%s.label      Stream %s connected\n", str.name.c_str(), str.name.c_str());
            std::printf("%s.draw       LINE2\n", name.c_str());
            std::printf("%s.colour     %s\n", name.c_str(), C_DIV[colourIx++]);
            std::printf("%s.cdef       %s,%.1f,/\n", name.c_str(), name.c_str(), UPTIME_DIV);
            colourIx %= C_DIV.size();
        }

        std::printf("\n\n");

    } else {
        std::printf("uptime.value    %" PRIiMAX "\n", report.proc.uptime);
        for (const auto& str : report.strs) {
            std::printf("%s.value    %d\n", str.name.c_str(), str.state == "CONNECTED" ? str.stateage : 0);
        }
    }


    // ----- Process graph 1: Uptime

    std::printf("multigraph %s.uptime\n", multigraph_name.c_str());
    if (doConfig) {
        std::printf("graph_title     1) StreamMux %s process uptime\n", report.proc.name.c_str());
        std::printf("graph_category  sm1_proc\n");
        std::printf("graph_args      --base 1000 --lower-limit 0.1 --logarithmic\n");
        std::printf("graph_scale     no\n");
        std::printf("graph_printf    %s\n", UPTIME_PRINTF);
        std::printf("graph_vlabel    %s\n", UPTIME_VLABEL);
        std::printf("graph_info      StreamMux %s process uptime\n", report.proc.name.c_str());

        std::printf("\n");
        std::printf("uptime.label    Process uptime\n");
        std::printf("uptime.draw     LINE2\n");
        std::printf("uptime.colour   %s\n", C_UPTIME);
        std::printf("uptime.cdef     uptime,%.1f,/\n", UPTIME_DIV);
        if (!opts_.nowarn_) {
            std::printf("uptime.warning  %.3f:\n", 5.0 * SEC_IN_MIN_D / UPTIME_DIV);  // Less than 5'
            std::printf("uptime.critical %.3f:\n", 1.0 * SEC_IN_MIN_D / UPTIME_DIV);  // Less than 1'
        }

        std::printf("\n\n");
    } else {
        std::printf("uptime.value    %" PRIiMAX "\n", report.proc.uptime);
    }


    // ----- Process graph 2: CPU usage

    std::printf("multigraph %s.cpu\n", multigraph_name.c_str());
    if (doConfig) {
        std::printf("graph_title     2) StreamMux %s CPU usage\n", report.proc.name.c_str());
        std::printf("graph_category  sm1_proc\n");
        std::printf("graph_args      --base 1000 --lower-limit 0 --upper-limit 20 --rigid --allow-shrink\n");
        std::printf("graph_scale     no\n");
        std::printf("graph_printf    %%6.1lf\n");
        std::printf("graph_vlabel    CPU usage [%%]\n");
        std::printf("graph_info      StreamMux %s CPU usage (5 minutes statistics)\n", report.proc.name.c_str());
        std::printf("graph_order     max meanps mean meanms min\n");

        std::printf("\n");
        std::printf("mean.label      Mean\n");
        std::printf("mean.draw       LINE2\n");
        std::printf("mean.colour     %s\n", C_MEAN);
        if (!opts_.nowarn_) {
#ifdef NDEBUG
            std::printf("mean.warning    :5\n");
            std::printf("mean.critical   :5\n");
#else
            std::printf("mean.warning    :10\n");
            std::printf("mean.critical   :10\n");
#endif
        }
        std::printf("\n");
        std::printf("max.label       Max\n");
        std::printf("max.draw        LINE1\n");
        std::printf("max.colour      %s\n", C_MINMAX);
        if (!opts_.nowarn_) {
#ifdef NDEBUG
            std::printf("max.warning     :10\n");
            std::printf("max.critical    :10\n");
#else
            std::printf("max.warning     :20\n");
            std::printf("max.critical    :20\n");
#endif
        }
        std::printf("\n");
        std::printf("min.label       Min\n");
        std::printf("min.draw        LINE1\n");
        std::printf("min.colour      %s\n", C_MINMAX);
        std::printf("\n");
        std::printf("meanps.label    Mean + std\n");
        std::printf("meanps.draw     LINE1\n");
        std::printf("meanps.colour   %s\n", C_STD);
        std::printf("\n");
        std::printf("meanms.label    Mean - std\n");
        std::printf("meanms.draw     LINE1\n");
        std::printf("meanms.colour   %s\n", C_STD);

        std::printf("\n\n");
    } else {
        if (report.proc.cpu_stats.num > 60) { // at least one minute
            std::printf("mean.value      %7.3f\n", report.proc.cpu_stats.mean);
            std::printf("min.value       %7.3f\n", report.proc.cpu_stats.min);
            std::printf("max.value       %7.3f\n", report.proc.cpu_stats.max);
            std::printf("std.value       %7.3f\n", report.proc.cpu_stats.std);
            std::printf("meanps.value    %7.3f\n", report.proc.cpu_stats.mean + report.proc.cpu_stats.std);
            std::printf("meanms.value    %7.3f\n", report.proc.cpu_stats.mean - report.proc.cpu_stats.std);
        } else {
            std::printf("mean.value      U\n");
            std::printf("min.value       U\n");
            std::printf("max.value       U\n");
            std::printf("std.value       U\n");
            std::printf("meanps.value    U\n");
            std::printf("meanms.value    U\n");
        }
    }


    // ----- Process graph 3: memory usage

    std::printf("multigraph %s.mem\n", multigraph_name.c_str());
    if (doConfig) {
        std::printf("graph_title     3) StreamMux %s memory usage\n", report.proc.name.c_str());
        std::printf("graph_category  sm1_proc\n");
        std::printf("graph_args      --base 1000 --lower-limit 0 --upper-limit 100 --rigid --allow-shrink\n");
        std::printf("graph_scale     no\n");
        std::printf("graph_printf    %%6.1lf\n");
        std::printf("graph_vlabel    Memory usage [MiB]\n");
        std::printf("graph_info      StreamMux %s memory usage (5 minutes statistics)\n", report.proc.name.c_str());
        std::printf("graph_order     max meanps mean meanms min\n");

        std::printf("\n");
        std::printf("mean.label      Mean\n");
        std::printf("mean.draw       LINE2\n");
        std::printf("mean.colour     %s\n", C_MEAN);
        if (!opts_.nowarn_) {
#ifdef NDEBUG
            std::printf("mean.warning    :50\n");
            std::printf("mean.critical   :50\n");
#else
            std::printf("mean.warning    :75\n");
            std::printf("mean.critical   :75\n");
#endif
        }
        std::printf("\n");
        std::printf("max.label       Max\n");
        std::printf("max.draw        LINE1\n");
        std::printf("max.colour      %s\n", C_MINMAX);
        if (!opts_.nowarn_) {
#ifdef NDEBUG
            std::printf("max.warning     :100\n");
            std::printf("max.critical    :100\n");
#else
            std::printf("max.warning     :150\n");
            std::printf("max.critical    :150\n");
#endif
        }
        std::printf("\n");
        std::printf("min.label       Min\n");
        std::printf("min.draw        LINE1\n");
        std::printf("min.colour      %s\n", C_MINMAX);
        std::printf("\n");
        std::printf("meanps.label    Mean + std\n");
        std::printf("meanps.draw     LINE1\n");
        std::printf("meanps.colour   %s\n", C_STD);
        std::printf("\n");
        std::printf("meanms.label    Mean - std\n");
        std::printf("meanms.draw     LINE1\n");
        std::printf("meanms.colour   %s\n", C_STD);

        std::printf("\n\n");
    } else {
        if (report.proc.cpu_stats.num > 60) { // at least one minute
            std::printf("mean.value      %7.3f\n", report.proc.mem_stats.mean);
            std::printf("min.value       %7.3f\n", report.proc.mem_stats.min);
            std::printf("max.value       %7.3f\n", report.proc.mem_stats.max);
            std::printf("std.value       %7.3f\n", report.proc.mem_stats.std);
            std::printf("meanps.value    %7.3f\n", report.proc.mem_stats.mean + report.proc.mem_stats.std);
            std::printf("meanms.value    %7.3f\n", report.proc.mem_stats.mean - report.proc.mem_stats.std);
        } else {
            std::printf("mean.value      U\n");
            std::printf("min.value       U\n");
            std::printf("max.value       U\n");
            std::printf("std.value       U\n");
            std::printf("meanps.value    U\n");
            std::printf("meanms.value    U\n");
        }
    }

    // ----- Stream graphs

    for (std::size_t ix = 0; ix < report.strs.size(); ix++) {
        const auto& str = report.strs[ix];
        const auto name = StrToLower(str.name);
        const auto category = Sprintf("sm3_str%02" PRIuMAX "_%s", ix + 1, name.c_str());

        // ----- Stream graph 1: connected uptime

        std::printf("multigraph %s.%s_uptime\n", multigraph_name.c_str(), name.c_str());
        if (doConfig) {
            std::printf("graph_title     1) StreamMux %s stream %s connected uptime\n", report.proc.name.c_str(), str.name.c_str());
            std::printf("graph_category  %s\n", category.c_str());
            std::printf("graph_args      --base 1000 --lower-limit 0.1 --logarithmic\n");
            std::printf("graph_scale     no\n");
            std::printf("graph_printf    %s\n", UPTIME_PRINTF);
            std::printf("graph_vlabel    %s\n", UPTIME_VLABEL);
            std::printf("graph_info      StreamMux %s stream %s connected uptime\n", report.proc.name.c_str(), str.name.c_str());

            std::printf("\n");
            std::printf("uptime.label    Connected uptime\n");
            std::printf("uptime.draw     LINE2\n");
            std::printf("uptime.colour   %s\n", C_UPTIME);
            std::printf("uptime.cdef     uptime,%.1f,/\n", UPTIME_DIV);
            if (!opts_.nowarn_) {
                std::printf("uptime.warning  %.3f:\n", 5.0 * SEC_IN_MIN_D / UPTIME_DIV); // Less than 5'
                std::printf("uptime.critical %.3f:\n", 1.0 * SEC_IN_MIN_D / UPTIME_DIV); // Less than 1'
            }

            std::printf("\n\n");
        } else {
            std::printf("uptime.value    %d\n", str.state == "CONNECTED" ? str.stateage : 0);
        }


        // ----- Stream graph 2: messages

        std::printf("multigraph %s.%s_msgs\n", multigraph_name.c_str(), name.c_str());
        if (doConfig) {
            std::printf("graph_title     2) StreamMux %s stream %s messages\n", report.proc.name.c_str(), str.name.c_str());
            std::printf("graph_category  %s\n", category.c_str());
            std::printf("graph_args      --base 1000\n");
            std::printf("graph_printf    %%6.0lf\n");
            std::printf("graph_vlabel    Messages read (-) / write (+) [1/s]\n");
            std::printf("graph_info      StreamMux %s stream %s messages\n", report.proc.name.c_str(), str.name.c_str());

            // https://guide.munin-monitoring.org/en/latest/develop/plugins/plugin-bcp.html#plugin-bcp-direction

            std::printf("\n");
            std::printf("msgsr.label     Messages\n"); // doesn't show
            std::printf("msgsr.graph     no\n");
            std::printf("msgsr.type      DERIVE\n");
            std::printf("msgsr.min       0\n");
            std::printf("msgsw.label     Messages\n"); // only this shows
            std::printf("msgsw.type      DERIVE\n");
            std::printf("msgsw.min       0\n");
            std::printf("msgsw.draw      LINE2\n");
            std::printf("msgsw.colour    %s\n", C_MSGS);
            std::printf("msgsw.negative  msgsr\n");

            std::printf("\n");
            std::printf("filtr.label     Filtered\n");
            std::printf("filtr.graph     no\n");
            std::printf("filtr.type      DERIVE\n");
            std::printf("filtr.min       0\n");
            std::printf("filtw.label     Filtered\n");
            std::printf("filtw.type      DERIVE\n");
            std::printf("filtw.min       0\n");
            std::printf("filtw.draw      LINE1\n");
            std::printf("filtw.colour    %s\n", C_FILT);
            std::printf("filtw.negative  filtr\n");

            std::printf("\n");
            std::printf("otherr.label    Other\n");
            std::printf("otherr.graph    no\n");
            std::printf("otherr.type     DERIVE\n");
            std::printf("otherr.min      0\n");
            std::printf("otherw.label    Other\n");
            std::printf("otherw.type     DERIVE\n");
            std::printf("otherw.min      0\n");
            std::printf("otherw.draw     LINE1\n");
            std::printf("otherw.colour   %s\n", C_OTHER);
            std::printf("otherw.negative otherr\n");

            std::printf("\n\n");
        } else {
            std::printf("msgsr.value  %" PRIuMAX "\n", str.stats[0].n_msgs);
            std::printf("msgsw.value  %" PRIuMAX "\n", str.stats[1].n_msgs);
            std::printf("filtr.value  %" PRIuMAX "\n", str.stats[0].n_filt);
            std::printf("filtw.value  %" PRIuMAX "\n", str.stats[1].n_filt);
            std::printf("otherr.value %" PRIuMAX "\n", str.stats[0].n_other);
            std::printf("otherw.value %" PRIuMAX "\n", str.stats[1].n_other);
        }


        // ----- Stream graph 3: sizes

        std::printf("multigraph %s.%s_size\n", multigraph_name.c_str(), name.c_str());
        if (doConfig) {
            std::printf("graph_title     3) StreamMux %s stream %s size\n", report.proc.name.c_str(), str.name.c_str());
            std::printf("graph_category  %s\n", category.c_str());
            std::printf("graph_args      --base 1024\n");
            // std::printf("graph_scale     no\n");
            std::printf("graph_printf    %%6.0lf\n");
            std::printf("graph_vlabel    Size read (-) / write (+) [bytes/s]\n");
            std::printf("graph_info      StreamMux %s stream %s size [bytes]\n", report.proc.name.c_str(), str.name.c_str());

            std::printf("\n");
            std::printf("msgsr.label     Messages\n");
            std::printf("msgsr.graph     no\n");
            std::printf("msgsr.type      DERIVE\n");
            std::printf("msgsr.min       0\n");
            std::printf("msgsw.label     Messages\n");
            std::printf("msgsw.type      DERIVE\n");
            std::printf("msgsw.min       0\n");
            std::printf("msgsw.draw      LINE2\n");
            std::printf("msgsw.colour    %s\n", C_MSGS);
            std::printf("msgsw.negative  msgsr\n");

            std::printf("\n");
            std::printf("filtr.label     Filtered\n");
            std::printf("filtr.graph     no\n");
            std::printf("filtr.type      DERIVE\n");
            std::printf("filtr.min       0\n");
            std::printf("filtw.label     Filtered\n");
            std::printf("filtw.type      DERIVE\n");
            std::printf("filtw.min       0\n");
            std::printf("filtw.draw      LINE1\n");
            std::printf("filtw.colour    %s\n", C_FILT);
            std::printf("filtw.negative  filtr\n");

            std::printf("\n");
            std::printf("otherr.label    Other\n");
            std::printf("otherr.graph    no\n");
            std::printf("otherr.type     DERIVE\n");
            std::printf("otherr.min      0\n");
            std::printf("otherw.label    Other\n");
            std::printf("otherw.type     DERIVE\n");
            std::printf("otherw.min      0\n");
            std::printf("otherw.draw     LINE1\n");
            std::printf("otherw.colour   %s\n", C_OTHER);
            std::printf("otherw.negative otherr\n");

            std::printf("\n\n");
        } else {
            std::printf("msgsr.value     %" PRIuMAX "\n", str.stats[0].s_msgs);
            std::printf("msgsw.value     %" PRIuMAX "\n", str.stats[1].s_msgs);
            std::printf("filtr.value     %" PRIuMAX "\n", str.stats[0].s_filt);
            std::printf("filtw.value     %" PRIuMAX "\n", str.stats[1].s_filt);
            std::printf("otherr.value    %" PRIuMAX "\n", str.stats[0].s_other);
            std::printf("otherw.value    %" PRIuMAX "\n", str.stats[1].s_other);
        }

    }  // report.strs


    // ----- Monitor graphs

    for (std::size_t ix = 0; ix < report.mons.size(); ix++) {
        const auto& mon = report.mons[ix];
        const auto name = StrToLower(mon.name);
        const auto category = Sprintf("sm2_mon%02" PRIuMAX "_%s", ix + 1, name.c_str());

        // We'll need a ~300s stats
        auto pStats = std::find_if(
            mon.stats.begin(), mon.stats.end(), [](const auto& cand) { return std::fabs(cand.winSize - 300.0) < 5.0; });
        SmReportMonStats stats; // empty
        if (pStats == mon.stats.end()) {
            DEBUG("No suitable stats for %s", mon.name.c_str());
        } else {
            stats = *pStats;
            DEBUG("have stats for %s %.0f", name.c_str(), stats.winSize);
        }

        // ----- Monitor graph 1: latency

        std::printf("multigraph %s.%s_latency\n", multigraph_name.c_str(), name.c_str());
        if (doConfig) {
            std::printf("graph_title     1) StreamMux %s monitor %s (%s) latency\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_category  %s\n", category.c_str());
            std::printf("graph_args      --base 1000 --lower-limit 0.01 --upper-limit 2 --logarithmic --rigid --allow-shrink\n");
            std::printf("graph_scale     no\n");
            std::printf("graph_printf    %%6.3lf\n");
            std::printf("graph_vlabel    Latency [s]\n");
            std::printf("graph_info      StreamMux %s monitor %s (stream %s) latency (5 minutes statistics)\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_order     max meanps mean meanms min\n");

            std::printf("\n");
            std::printf("mean.label      Mean\n");
            std::printf("mean.draw       LINE2\n");
            std::printf("mean.colour     %s\n", C_MEAN);
            if (!opts_.nowarn_) {
                std::printf("mean.warning    :0.35\n");
                std::printf("mean.critical   :0.50\n");
            }
            std::printf("\n");
            std::printf("max.label       Max\n");
            std::printf("max.draw        LINE1\n");
            std::printf("max.colour      %s\n", C_MINMAX);
            if (!opts_.nowarn_) {
                std::printf("max.warning     :0.75\n");
                std::printf("max.critical    :1.00\n");
            }
            std::printf("\n");
            std::printf("min.label       Min\n");
            std::printf("min.draw        LINE1\n");
            std::printf("min.colour      %s\n", C_MINMAX);
            std::printf("\n");
            std::printf("meanps.label    Mean + std\n");
            std::printf("meanps.draw     LINE1\n");
            std::printf("meanps.colour   %s\n", C_STD);
            std::printf("\n");
            std::printf("meanms.label    Mean - std\n");
            std::printf("meanms.draw     LINE1\n");
            std::printf("meanms.colour   %s\n", C_STD);

            std::printf("\n\n");
        } else {
            if (stats.latency) { // at least one minute
                std::printf("mean.value      %5.3f\n", stats.latency->mean);
                std::printf("min.value       %5.3f\n", stats.latency->min);
                std::printf("max.value       %5.3f\n", stats.latency->max);
                std::printf("std.value       %5.3f\n", stats.latency->std);
                std::printf("meanps.value    %5.3f\n", stats.latency->mean + stats.latency->std);
                std::printf("meanms.value    %5.3f\n", stats.latency->mean - stats.latency->std);
            } else {
                std::printf("mean.value      U\n");
                std::printf("min.value       U\n");
                std::printf("max.value       U\n");
                std::printf("std.value       U\n");
                std::printf("meanps.value    U\n");
                std::printf("meanms.value    U\n");
            }
        }


        // ----- Monitor graph 2: fix type

        std::printf("multigraph %s.%s_fix\n", multigraph_name.c_str(), name.c_str());
        if (doConfig) {
            std::printf("graph_title     2) StreamMux %s monitor %s (%s) fix type\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_category  %s\n", category.c_str());
            std::printf("graph_args      --base 1000 --lower-limit 0.5 --upper-limit 10.5 --rigid\n");
            std::printf("graph_printf    %%6.0lf\n");
            std::printf("graph_scale     no\n");
            std::printf("graph_vlabel    Fix type [-]\n");
            std::printf("graph_info      StreamMux %s monitor %s (stream %s) fix type (5 minutes statistics). 1 = no fix, 2 = DR only, 3 = time, 4 = 2d, 5 = 3d, 6 = 3d+DR, 7 = RTK float, 8 = RTK fixed, 9 = RTK float + DR, 10 = RTK fixed + DR\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_order     min max mean\n");

            std::printf("\n");
            std::printf("mean.label      Mean\n");
            std::printf("mean.draw       LINE3\n");
            std::printf("mean.colour     %s\n", C_MEAN);
            std::printf("mean.critical   2.5:\n"); // < TIME(3)

            std::printf("\n");
            std::printf("max.label       Max\n");
            std::printf("max.draw        LINE1\n");
            std::printf("max.colour      %s\n", C_MINMAX);
            std::printf("\n");
            std::printf("min.label       Min\n");
            std::printf("min.draw        LINE1\n");
            std::printf("min.colour      %s\n", C_MINMAX);
            if (!opts_.nowarn_) {
                std::printf("min.warning     2.5:\n"); // < TIME(3)
            }

            std::printf("\n\n");
        } else {
            if (stats.fixType) { // at least one minute
                std::printf("mean.value      %4.1f\n", stats.fixType->mean);
                std::printf("min.value       %4.1f\n", stats.fixType->mean);
                std::printf("max.value       %4.1f\n", stats.fixType->mean);
            } else {
                std::printf("mean.value      U\n");
                std::printf("min.value       U\n");
                std::printf("max.value       U\n");
            }
        }

        // ----- Monitor graph 3: DOPs

        std::printf("multigraph %s.%s_dop\n", multigraph_name.c_str(), name.c_str());
        if (doConfig) {
            std::printf("graph_title     3) StreamMux %s monitor %s (%s) DOP\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_category  %s\n", category.c_str());
            std::printf("graph_args      --base 1000 --lower-limit 0.1 --upper-limit 20.0 --logarithmic --rigid --allow-shrink\n");
            std::printf("graph_scale     no\n");
            std::printf("graph_printf    %%6.2lf\n");
            std::printf("graph_vlabel    DOP [-]\n");
            std::printf("graph_info      StreamMux %s monitor %s (stream %s) DOP (5 minutes statistics)\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_order     gdop pdop tdop\n");

            std::printf("\n");
            std::printf("gdop.label      gDOP\n");
            std::printf("gdop.draw       LINE2\n");
            std::printf("gdop.colour     %s\n", C_GDOP);
            if (!opts_.nowarn_) {
                std::printf("gdop.warning    :1.0\n"); // > 1.0
                std::printf("gdop.critical   :2.0\n"); // > 2.0
            }

            std::printf("\n");
            std::printf("pdop.label      pDOP\n");
            std::printf("pdop.draw       LINE2\n");
            std::printf("pdop.colour     %s\n", C_PTDOP);
            if (!opts_.nowarn_) {
                std::printf("pdop.warning    :1.0\n"); // > 1.0
                std::printf("pdop.critical   :2.0\n"); // > 2.0
            }

            std::printf("\n");
            std::printf("tdop.label      tDOP\n");
            std::printf("tdop.draw       LINE2\n");
            std::printf("tdop.colour     %s\n", C_PTDOP);
            if (!opts_.nowarn_) {
                std::printf("tdop.warning    :1.0\n"); // > 1.0
                std::printf("tdop.critical   :2.0\n"); // > 2.0
            }

            std::printf("\n\n");
        } else {
            if (stats.gDOP) { // at least one minute
                std::printf("gdop.value      %5.2f\n", stats.gDOP->mean);
            } else {
                std::printf("gdop.value      U\n");
            }
            if (stats.pDOP) { // at least one minute
                std::printf("pdop.value      %5.2f\n", stats.pDOP->mean);
            } else {
                std::printf("pdop.value      U\n");
            }
            if (stats.tDOP) { // at least one minute
                std::printf("tdop.value      %5.2f\n", stats.tDOP->mean);
            } else {
                std::printf("tdop.value      U\n");
            }
        }

        // ----- Monitor graph 4: C/No level

        std::printf("multigraph %s.%s_cno\n", multigraph_name.c_str(), name.c_str());
        if (doConfig) {
            std::printf("graph_title     4) StreamMux %s monitor %s (%s) C/No level\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_category  %s\n", category.c_str());
            std::printf("graph_args      --base 1000 --lower-limit 0 --upper-limit 60 --rigid --allow-shrink\n");
            std::printf("graph_scale     no\n");
            std::printf("graph_printf    %%6.1lf\n");
            std::printf("graph_vlabel    C/No [dbHz]\n");
            std::printf("graph_info      StreamMux %s monitor %s (stream %s) C/No level (mean of 5 strongest signals per band, 5 minutes statistics)\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_order     mean meanl1 meane6 meanl2 meanl5\n");

            std::printf("\n");
            std::printf("meanl1.label    Mean\n");
            std::printf("meanl1.draw     LINE2\n");
            std::printf("meanl1.colour   %s\n", C_MEAN);
            if (!opts_.nowarn_) {
                std::printf("meanl1.warning  42:\n");
                std::printf("meanl1.critical 40:\n");
            }
            std::printf("\n");
            std::printf("meanl1.label    L1\n");
            std::printf("meanl1.draw     LINE2\n");
            std::printf("meanl1.colour   %s\n", C_DIV[0]);
            if (!opts_.nowarn_) {
                std::printf("meanl1.warning  42:\n");
                std::printf("meanl1.critical 40:\n");
            }
            std::printf("\n");
            std::printf("meane6.label    E6\n");
            std::printf("meane6.draw     LINE2\n");
            std::printf("meane6.colour   %s\n", C_DIV[1]);
            if (!opts_.nowarn_) {
                std::printf("meane6.warning  42:\n");
                std::printf("meane6.critical 40:\n");
            }
            std::printf("\n");
            std::printf("meanl2.label    L2\n");
            std::printf("meanl2.draw     LINE2\n");
            std::printf("meanl2.colour   %s\n", C_DIV[2]);
            if (!opts_.nowarn_) {
                std::printf("meanl2.warning  42:\n");
                std::printf("meanl2.critical 40:\n");
            }
            std::printf("\n");
            std::printf("meanl5.label    L5\n");
            std::printf("meanl5.draw     LINE2\n");
            std::printf("meanl5.colour   %s\n", C_DIV[3]);
            if (!opts_.nowarn_) {
                std::printf("meanl5.warning  42:\n");
                std::printf("meanl5.critical 40:\n");
            }

            std::printf("\n\n");
        } else {
            if (stats.cnoAvgTop5) {
                std::printf("mean.value      %4.1f\n", stats.cnoAvgTop5->mean);
            } else {
                std::printf("mean.value      U\n");
            }
            if (stats.cnoAvgTop5L1) {
                std::printf("meanl1.value    %4.1f\n", stats.cnoAvgTop5L1->mean);
            } else {
                std::printf("meanl1.value    U\n");
            }
            if (stats.cnoAvgTop5E6) {
                std::printf("meane6.value    %4.1f\n", stats.cnoAvgTop5E6->mean);
            } else {
                std::printf("meane6.value    U\n");
            }
            if (stats.cnoAvgTop5L2) {
                std::printf("meanl2.value    %4.1f\n", stats.cnoAvgTop5L2->mean);
            } else {
                std::printf("meanl2.value    U\n");
            }
            if (stats.cnoAvgTop5L5) {
                std::printf("meanl5.value    %4.1f\n", stats.cnoAvgTop5L5->mean);
            } else {
                std::printf("meanl5.value    U\n");
            }
        }

        // ----- Monitor graph 5: signals used (per GNSS)

        std::printf("multigraph %s.%s_sigg\n", multigraph_name.c_str(), name.c_str());
        if (doConfig) {
            std::printf("graph_title      5) StreamMux %s monitor %s (%s) signals used (GNSS)\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_category   %s\n", category.c_str());
            std::printf("graph_args       --base 1000 --lower-limit 0 --upper-limit 140 --rigid\n");
            std::printf("graph_scale      no\n");
            std::printf("graph_printf     %%6.1lf\n");
            std::printf("graph_vlabel     Number of signals [-]\n");
            std::printf("graph_info       StreamMux %s monitor %s (stream %s) mean number of signals used (per GNSS, 5 minutes statistics)\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_order      meangps meansbas meangal meanbds meanqzss meanglo meannavic total\n");

            std::printf("\n");
            std::printf("meangps.label    GPS\n");
            std::printf("meangps.draw     AREASTACK\n");
            std::printf("meangps.colour   %s\n", C_DIV[0]);
            if (!opts_.nowarn_) {
                std::printf("meangps.warning  8:\n");
                std::printf("meangps.critical 6:\n");
            }
            std::printf("\n");
            std::printf("meansbas.label   SBAS\n");
            std::printf("meansbas.draw    AREASTACK\n");
            std::printf("meansbas.colour  %s\n", C_DIV[1]);
            std::printf("\n");
            std::printf("meangal.label    Galileo\n");
            std::printf("meangal.draw     AREASTACK\n");
            std::printf("meangal.colour   %s\n", C_DIV[2]);
            if (!opts_.nowarn_) {
                std::printf("meangal.warning  8:\n");
                std::printf("meangal.critical 6:\n");
            }
            std::printf("\n");
            std::printf("meanbds.label    BeiDou\n");
            std::printf("meanbds.draw     AREASTACK\n");
            std::printf("meanbds.colour   %s\n", C_DIV[3]);
            if (!opts_.nowarn_) {
                std::printf("meanbds.warning  4:\n");
                std::printf("meanbds.critical 2:\n");
            }
            std::printf("\n");
            std::printf("meanqzss.label   QZSS\n");
            std::printf("meanqzss.draw    AREASTACK\n");
            std::printf("meanqzss.colour  %s\n", C_DIV[4]);
            std::printf("\n");
            std::printf("meanglo.label    GLONASS\n");
            std::printf("meanglo.draw     AREASTACK\n");
            std::printf("meanglo.colour   %s\n", C_DIV[5]);
            if (!opts_.nowarn_) {
                std::printf("meanglo.warning  4:\n");
                std::printf("meanglo.critical 2:\n");
            }
            std::printf("\n");
            std::printf("meannavic.label  NavIC\n");
            std::printf("meannavic.draw   AREASTACK\n");
            std::printf("meannavic.colour %s\n", C_DIV[6]);
            std::printf("\n");
            std::printf("total.label      Total\n");
            std::printf("total.draw       LINE2\n");
            std::printf("total.colour     %s\n", C_TOTAL);

            std::printf("\n\n");
        } else {
            double total = 0.0;
            if (stats.numSigUsedGps) { // at least one minute
                std::printf("meangps.value   %4.1f\n", stats.numSigUsedGps->mean);
                total += stats.numSigUsedGps->mean;
            } else {
                std::printf("meangps.value   U\n");
            }
            if (stats.numSigUsedSbas) { // at least one minute
                std::printf("meansbas.value  %4.1f\n", stats.numSigUsedSbas->mean);
                total += stats.numSigUsedSbas->mean;
            } else {
                std::printf("meansbas.value  U\n");
            }
            if (stats.numSigUsedGal) { // at least one minute
                std::printf("meangal.value    %4.1f\n", stats.numSigUsedGal->mean);
                total += stats.numSigUsedGal->mean;
            } else {
                std::printf("meangal.value   U\n");
            }
            if (stats.numSigUsedBds) { // at least one minute
                std::printf("meanbds.value   %4.1f\n", stats.numSigUsedBds->mean);
                total += stats.numSigUsedBds->mean;
            } else {
                std::printf("meanbds.value   U\n");
            }
            if (stats.numSigUsedQzss) {  // at least one minute
                std::printf("meanqzss.value %4.1f\n", stats.numSigUsedQzss->mean);
                total += stats.numSigUsedQzss->mean;
            } else {
                std::printf("meanqzss.value  U\n");
            }
            if (stats.numSigUsedGlo) { // at least one minute
                std::printf("meanglo.value   %4.1f\n", stats.numSigUsedGlo->mean);
                total += stats.numSigUsedGlo->mean;
            } else {
                std::printf("meanglo.value   U\n");
            }
            if (stats.numSigUsedNavic) { // at least one minute
                std::printf("meannavic.value %4.1f\n", stats.numSigUsedNavic->mean);
                total += stats.numSigUsedNavic->mean;
            } else {
                std::printf("meannavic.value U\n");
            }
            if (total > 0.0) {
                std::printf("total.value    %4.1f\n", total);
            } else {
                std::printf("total.value    U\n");
            }
        }

        // ----- Monitor graph 6: signals used (per band)

        std::printf("multigraph %s.%s_sigb\n", multigraph_name.c_str(), name.c_str());
        if (doConfig) {
            std::printf("graph_title     6) StreamMux %s monitor %s (%s) signals used (band)\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_category  %s\n", category.c_str());
            std::printf("graph_args      --base 1000 --lower-limit 0 --upper-limit 140 --rigid\n");
            std::printf("graph_scale     no\n");
            std::printf("graph_printf    %%6.1lf\n");
            std::printf("graph_vlabel    Number of signals [-]\n");
            std::printf("graph_info      StreamMux %s monitor %s (stream %s) mean number of signals used (per band, 5 minutes statistics)\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_order     meanl1 meane6 meanl2 meanl5 total\n");

            std::printf("\n");
            std::printf("meanl1.label    L1\n");
            std::printf("meanl1.draw     AREASTACK\n");
            std::printf("meanl1.colour   %s\n", C_DIV[0]);
            std::printf("\n");
            std::printf("meane6.label    E6\n");
            std::printf("meane6.draw     AREASTACK\n");
            std::printf("meane6.colour   %s\n", C_DIV[1]);
            std::printf("\n");
            std::printf("meanl2.label    L2\n");
            std::printf("meanl2.draw     AREASTACK\n");
            std::printf("meanl2.colour   %s\n", C_DIV[2]);
            std::printf("\n");
            std::printf("meanl5.label    L5\n");
            std::printf("meanl5.draw     AREASTACK\n");
            std::printf("meanl5.colour   %s\n", C_DIV[3]);
            std::printf("\n");
            std::printf("total.label     Total\n");
            std::printf("total.draw      LINE2\n");
            std::printf("total.colour    %s\n", C_TOTAL);

            std::printf("\n\n");
        } else {
            double total = 0.0;
            if (stats.numSigUsedL1) { // at least one minute
                std::printf("meanl1.value    %4.1f\n", stats.numSigUsedL1->mean);
                total += stats.numSigUsedL1->mean;
            } else {
                std::printf("meanl1.value    U\n");
            }
            if (stats.numSigUsedE6) { // at least one minute
                std::printf("meane6.value    %4.1f\n", stats.numSigUsedE6->mean);
                total += stats.numSigUsedE6->mean;
            } else {
                std::printf("meane6.value    U\n");
            }
            if (stats.numSigUsedL2) { // at least one minute
                std::printf("meanl2.value    %4.1f\n", stats.numSigUsedL2->mean);
                total += stats.numSigUsedL2->mean;
            } else {
                std::printf("meanl2.value    U\n");
            }
            if (stats.numSigUsedL5) { // at least one minute
                std::printf("meanl5.value    %4.1f\n", stats.numSigUsedL5->mean);
                total += stats.numSigUsedL5->mean;
            } else {
                std::printf("meanl5.value    U\n");
            }
            if (total > 0.0) {
                std::printf("total.value     %4.1f\n", total);
            } else {
                std::printf("total.value     U\n");
            }
        }

        // ----- Monitor graph 7: message per epoch

        std::printf("multigraph %s.%s_msgs\n", multigraph_name.c_str(), name.c_str());
        if (doConfig) {
            std::printf("graph_title     7) StreamMux %s monitor %s (%s) messages per epoch\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_category  %s\n", category.c_str());
            std::printf("graph_args      --base 1000 \n");
            std::printf("graph_scale     no\n");
            std::printf("graph_printf    %%6.1lf\n");
            std::printf("graph_vlabel    Rate [messages/epoch]\n");
            std::printf("graph_info      StreamMux %s monitor %s (stream %s) messages per epoch (5 minutes statistics)\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_order     max meanps mean meanms min\n");

            std::printf("\n");
            std::printf("mean.label      Mean\n");
            std::printf("mean.draw       LINE2\n");
            std::printf("mean.colour     %s\n", C_MEAN);
            std::printf("\n");
            std::printf("max.label       Max\n");
            std::printf("max.draw        LINE1\n");
            std::printf("max.colour      %s\n", C_MINMAX);
            std::printf("\n");
            std::printf("min.label       Min\n");
            std::printf("min.draw        LINE1\n");
            std::printf("min.colour      %s\n", C_MINMAX);
            std::printf("\n");
            std::printf("meanps.label    Mean + std\n");
            std::printf("meanps.draw     LINE1\n");
            std::printf("meanps.colour   %s\n", C_STD);
            std::printf("\n");
            std::printf("meanms.label    Mean - std\n");
            std::printf("meanms.draw     LINE1\n");
            std::printf("meanms.colour   %s\n", C_STD);

            std::printf("\n\n");
        } else {
            if (stats.rateMsgs) { // at least one minute
                std::printf("mean.value      %5.3f\n", stats.rateMsgs->mean);
                std::printf("min.value       %5.3f\n", stats.rateMsgs->min);
                std::printf("max.value       %5.3f\n", stats.rateMsgs->max);
                std::printf("std.value       %5.3f\n", stats.rateMsgs->std);
                std::printf("meanps.value    %5.3f\n", stats.rateMsgs->mean + stats.rateMsgs->std);
                std::printf("meanms.value    %5.3f\n", stats.rateMsgs->mean - stats.rateMsgs->std);
            } else {
                std::printf("mean.value      U\n");
                std::printf("min.value       U\n");
                std::printf("max.value       U\n");
                std::printf("std.value       U\n");
                std::printf("meanps.value    U\n");
                std::printf("meanms.value    U\n");
            }
        }

        // ----- Monitor graph 8: bytes per epoch

        std::printf("multigraph %s.%s_bytes\n", multigraph_name.c_str(), name.c_str());
        if (doConfig) {
            std::printf("graph_title     8) StreamMux %s monitor %s (%s) bytes per epoch\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_category  %s\n", category.c_str());
            std::printf("graph_args      --base 1024\n");
            // std::printf("graph_scale     no\n");
            std::printf("graph_printf    %%6.1lf\n");
            std::printf("graph_vlabel    Rate [bytes/epoch]\n");
            std::printf("graph_info      StreamMux %s monitor %s (stream %s) bytes per epoch (5 minutes statistics)\n", report.proc.name.c_str(), mon.name.c_str(), mon.src.c_str());
            std::printf("graph_order     max meanps mean meanms min\n");

            std::printf("\n");
            std::printf("mean.label      Mean\n");
            std::printf("mean.draw       LINE2\n");
            std::printf("mean.colour     %s\n", C_MEAN);
            std::printf("\n");
            std::printf("max.label       Max\n");
            std::printf("max.draw        LINE1\n");
            std::printf("max.colour      %s\n", C_MINMAX);
            std::printf("\n");
            std::printf("min.label       Min\n");
            std::printf("min.draw        LINE1\n");
            std::printf("min.colour      %s\n", C_MINMAX);
            std::printf("\n");
            std::printf("meanps.label    Mean + std\n");
            std::printf("meanps.draw     LINE1\n");
            std::printf("meanps.colour   %s\n", C_STD);
            std::printf("\n");
            std::printf("meanms.label    Mean - std\n");
            std::printf("meanms.draw     LINE1\n");
            std::printf("meanms.colour   %s\n", C_STD);

            std::printf("\n\n");
        } else {
            if (stats.rateBytes) { // at least one minute
                std::printf("mean.value      %5.3f\n", stats.rateBytes->mean);
                std::printf("min.value       %5.3f\n", stats.rateBytes->min);
                std::printf("max.value       %5.3f\n", stats.rateBytes->max);
                std::printf("std.value       %5.3f\n", stats.rateBytes->std);
                std::printf("meanps.value    %5.3f\n", stats.rateBytes->mean + stats.rateBytes->std);
                std::printf("meanms.value    %5.3f\n", stats.rateBytes->mean - stats.rateBytes->std);
            } else {
                std::printf("mean.value      U\n");
                std::printf("min.value       U\n");
                std::printf("max.value       U\n");
                std::printf("std.value       U\n");
                std::printf("meanps.value    U\n");
                std::printf("meanms.value    U\n");
            }
        }

    }  // mons

    // TODO: add {field}.unknown_limit?
    // TODO: mux graphs?
    // TODO: graph for mon data avail?

    // foo.warning 100:200      --> raise warning outside of range 100-200
    // foo.warning 100:         --> raise warning if < 100
    // foo.warning :100 or 100  --> raise warning if > 100

    return ok;
}

/* ******************************************************************************************************************
 */
}  // namespace ffapps::streammux

int main(int argc, char** argv)
{
    using namespace ffapps::streammux;
#ifndef NDEBUG
    fpsdk::common::app::StacktraceHelper stacktrace;
#endif
    bool ok = true;

    // Parse command line arguments
    SmMuninOptions opts;
    if (!opts.LoadFromArgv(argc, argv)) {
        ok = false;
    }

    if (ok) {
        SmMunin app(opts);
        ok = app.Run();
    }

    // Are we happy?
    if (ok) {
        INFO("Done");
        return EXIT_SUCCESS;
    } else {
        ERROR("Failed");
        return EXIT_FAILURE;
    }
}

/* ****************************************************************************************************************** */
