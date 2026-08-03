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
 * @brief stream multiplexer
 */
#ifndef __FFAPPS_STREAMTOOL_STREAMMUX_UTILS_HPP__
#define __FFAPPS_STREAMTOOL_STREAMMUX_UTILS_HPP__

/* LIBC/STL */
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

/* EXTERNAL */
#include <ffxx/epoch.hpp>
#include <ffxx/http.hpp>
#include <ffxx/stream.hpp>
#include <ffxx/utils.hpp>
#include <fpsdk_common/app.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/parser.hpp>
#include <fpsdk_common/parser/types.hpp>
#include <fpsdk_common/path.hpp>
#include <fpsdk_common/string.hpp>
#include <fpsdk_common/thread.hpp>
#include <fpsdk_common/time.hpp>
#include <fpsdk_common/types.hpp>
#include <fpsdk_common/yaml.hpp>
#include <nlohmann/json.hpp>

/* PACKAGE */

namespace ffapps::streammux {
/* ****************************************************************************************************************** */

using namespace ffxx;
using namespace fpsdk::common::app;
using namespace fpsdk::common::parser;
using namespace fpsdk::common::time;
using namespace nlohmann;

// ---------------------------------------------------------------------------------------------------------------------

// Program options
class StreamMuxOptions : public ProgramOptions
{
   public:
     StreamMuxOptions();

    std::vector<std::string> streams_;
    std::vector<std::string> muxes_;
    std::vector<std::string> monitors_;
    std::string api_;
    std::string assets_path_;
    std::string report_path_;
    std::string name_;
    bool systemd_ = false;

   private:
    static const std::vector<Option> OPTIONS;
    void PrintHelp() override final;
    bool HandleOption(const Option& option, const std::string& argument) final;
    bool CheckOptions(const std::vector<std::string>& args) final;

    struct KeyVal { std::string key_; std::string val_; };
    using KeyValList = std::vector<KeyVal>;
    bool GetKeyValList(const YAML::Node& node, KeyValList& kvList) const;
    KeyValList yaml_vars_;
    bool LoadConfigYaml(const std::string& file);
};

/* ****************************************************************************************************************** */

struct SmStats : public ParserStats
{
    uint64_t n_err_ = 0;
    uint64_t n_filt_ = 0;
    uint64_t s_filt_ = 0;

    json ToJson() const;
};

// ---------------------------------------------------------------------------------------------------------------------

class SmFilter
{
   public:
    bool Set(const std::string& spec);
    bool Pass(const ParserMsg& msg) const;
    std::size_t Size() const;
    const std::string& Spec() const;

   private:
    std::string spec_;
    enum class Type { MSG_NAME, NMEA_TOS, UBXNAV_TOS, RTCM3_MSM, RTCM3_STA, RTCM3_BIAS };
    enum class Mode { INCLUDE, EXCLUDE };
    struct Entry {
        Type type_;
        Mode mode_;
        std::string str_;
    };
    std::vector<Entry> entries_;
};

// ---------------------------------------------------------------------------------------------------------------------

struct SmMon;
using SmMonPtr = std::shared_ptr<SmMon>;

struct SmStr
{
    std::string name_;
    std::shared_ptr<Stream> stream_;
    SmFilter filter_read_;
    SmFilter filter_write_;
    SmStats stats_read_;
    SmStats stats_write_;
    bool can_read_ = true;
    bool can_write_ = true;
    std::atomic<bool> ena_read_ = true;
    std::atomic<bool> ena_write_ = true;
    bool used_ = false;
    std::atomic<bool> connected_ = false;
    std::list<std::string> statestrs_;
    Time state_ts_;
    std::vector<SmMonPtr> mons_;

    json ToJson();
};

using SmStrPtr = std::shared_ptr<SmStr>;

// ---------------------------------------------------------------------------------------------------------------------

struct SmMux
{
    std::string name_;
    SmFilter filter_fwd_;
    SmFilter filter_rev_;
    SmStats stats_fwd_;
    SmStats stats_rev_;
    bool can_fwd_ = true;
    bool can_rev_ = true;
    std::atomic<bool> ena_fwd_ = true;
    std::atomic<bool> ena_rev_ = true;
    SmStrPtr src_;
    SmStrPtr dst_;

    json ToJson();
};

using SmMuxPtr = std::shared_ptr<SmMux>;

// ---------------------------------------------------------------------------------------------------------------------

struct SmMon
{
    std::string name_;
    SmStrPtr src_;
    std::unique_ptr<EpochCollector> coll_;
    std::array<Duration, 3> winSizes_;

    void Add(const ParserMsg& msg);
    json ToJson();

   private:
    std::mutex mutex_;
    using QueueEntry = std::pair<ParserMsg, Time>;
    std::vector<QueueEntry> queue_;
    static constexpr std::size_t QUEUE_MAX = 250;

    void Process();

    struct Meas
    {
        Time time_;
        float latency_ = -1.0f;
        float fixType_ = -1.0f;
        float pDOP_ = -1.0f;
        float tDOP_ = -1.0f;
        float gDOP_ = -1.0f;
        int numSigUsed_ = -1;
        int numSigUsedL1_ = -1;
        int numSigUsedE6_ = -1;
        int numSigUsedL2_ = -1;
        int numSigUsedL5_ = -1;
        int numSigUsedGps_ = -1;
        int numSigUsedSbas_ = -1;
        int numSigUsedGal_ = -1;
        int numSigUsedBds_ = -1;
        int numSigUsedQzss_ = -1;
        int numSigUsedGlo_ = -1;
        int numSigUsedNavic_ = -1;
        float rateMsgs_ = -1.0f;
        float rateBytes_ = -1.0f;
        float cnoAvgTop5L1_ = -1.0f;
        float cnoAvgTop5E6_ = -1.0f;
        float cnoAvgTop5L2_ = -1.0f;
        float cnoAvgTop5L5_ = -1.0f;
    };

    std::list<Meas> meas_;
    static constexpr std::size_t MAX_MEAS = (305 * 10);  // 5min5sec at 10Hz
};

// ---------------------------------------------------------------------------------------------------------------------

static constexpr uint32_t STATUS_PERIOD = 1000; // [ms]

struct SmProc
{
    std::string name_;

    void Update();
    json ToJson();

   private:
    PerfStats perf_;

    struct Meas {
        float cpu_;
        float mem_;
    };
    std::list<Meas> meas_;
    static constexpr std::size_t NUM_MEAS = (300000 / STATUS_PERIOD); // 5min
};

// ---------------------------------------------------------------------------------------------------------------------

bool CheckName(const std::string& name);
std::string ConsumeOption(std::string& spec, const std::string& option, const std::string& def = "");

/* ****************************************************************************************************************** */
}  // namespace ffapps::streammux
#endif  // __FFAPPS_STREAMTOOL_STREAMMUX_UTILS_HPP__
