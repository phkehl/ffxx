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

/* LIBC/STL */
#include <regex>

/* EXTERNAL */
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/math.hpp>
#include <fpsdk_common/parser/nmea.hpp>
#include <fpsdk_common/parser/ubx.hpp>
#include <fpsdk_common/parser/rtcm3.hpp>
#include <fpsdk_common/string.hpp>
#include <fpsdk_common/to_json/parser.hpp>
#include <fpsdk_common/types.hpp>

/* PACKAGE */
#include "streammux_utils.hpp"

namespace ffapps::streammux {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::math;
using namespace fpsdk::common::parser::nmea;
using namespace fpsdk::common::parser::ubx;
using namespace fpsdk::common::parser::rtcm3;
using namespace fpsdk::common::path;
using namespace fpsdk::common::string;
using namespace fpsdk::common::types;
using namespace fpsdk::common::yaml;

// ---------------------------------------------------------------------------------------------------------------------

#define OPT_STREAM  { 's', true, "stream"  }
#define OPT_MUX     { 'm', true, "mux"     }
#define OPT_API     { 'a', true, "api"     }
#define OPT_REPORT  { 'r', true, "report"  }
#define OPT_NAME    { 'n', true, "name"    }
#define OPT_MONITOR { 'M', true, "monitor" }

/*static*/ const std::vector<StreamMuxOptions::Option> StreamMuxOptions::OPTIONS = /* clang-format off */ {
    { 's', true,  "stream"  },
    { 'm', true,  "mux"     },
    { 'a', true,  "api"     },
    { 'r', true,  "report"  },
    { 'n', true,  "name"    },
    { 'M', true,  "monitor" },
    { 'A', true,  "assets"  },
    { 'S', false, "systemd" },
    { 'c', true,  "config"  },
}; // clang-format on

StreamMuxOptions::StreamMuxOptions() : ProgramOptions("streammux", OPTIONS)
{
    version_str_ = ffxx::GetVersionString();
    copy_str_ = ffxx::GetCopyrightString();
    lic_str_ = ffxx::GetLicenseString();
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamMuxOptions::PrintHelp()
{
    // clang-format off
    std::fputs(
        "\n"
        "Tool to connect multiple streams to each other.\n"
        "\n"
        "Usage:\n"
        "\n"
        "    streammux [flags] -s <stream> -s <stream> -m <mux> [...]\n"
        "\n"
        "Where:\n"
        "\n", stdout);
    std::fputs(COMMON_FLAGS_HELP, stdout);
    std::fputs(
        "    -s <stream>, --stream <stream> -- Stream, where <stream> is a stream spec (see below)\n"
        "    -m <mux>, --mux <mux>          -- Mux, where <mux> is a mux spec (see below)\n"
        "    -M <mon>, --monitor <mon>      -- Monitor stream input, where <mon> is a monitor spec (see below)\n"
        "    -r <path>, --report <path>     -- Report stats to JSON file given by <path>, updated once every second.\n"
        "                                      Use a tmpfs RAM disk, such as /run/user/$UID/streammux.json\n"
        "    -a <api>, --api <api>          -- Provide HTTP API (and web UI), see below\n"
        // "    -A <path>, --assets            -- Use web UI assets from path instead of built-in assets (for development)\n"
        "    -n <name>, --name <name>       -- A short name for this streammux instance (default: automatic).\n"
        "                                      The <name> must match the regex /^[a-zA-Z][a-zA-Z0-9_]{1,9}$/.\n"
        "    -S, --systemd-notify           -- Use systemd-notify(1) (READY and WATCHDOG)\n"
        "    -c <file>, --config <file>     -- Load (some) config from YAML <file> (see below)\n"
        "\n"
        "The <mux>es connect the <stream>s to each other. Data is processed on message (frame) level. Therefore,\n"
        "data from multiples inputs muxed to the same output does not interfere with each other, as long as the data\n"
        "consists of messages of supported protocols (UBX, NMEA, RTCM3, etc.).\n"
        "\n"
        "Options can be loaded from one or more config YAML <file>s instead or in addition to specifying them on the\n"
        "command-line. The YAML must contain a key 'streammux', which must contain a list of key-value pairs, where\n"
        "the key is the long option name (e.g., 'stream') and the value the option's value. Optionally, a 'vars' list\n"
        "of key-value pairs can be present. The option values can reference the vars values as '{key}'. Multiple config\n"
        "<file>s can be loaded. The 'vars' from previous config <file>s can be used in later <file>s.\n"
        "\n", stdout);
        std::fputs(StreamHelpScreen(), stdout);
    std::fputs(
        "\n"
        "In addition to the general stream options described above streammux supports the following:\n"
        "\n"
        "- ER=on|off    -- Enable read (input) from stream (irrelevant for WO streams)\n"
        "- EW=on|off    -- Enable write (output) to streams (irrelevant for RO streams)\n"
        "- FR=<filter>  -- Filter read (input) messages from stream (irrelevant for WO streams)\n"
        "- FW=<filter>  -- Filter write (output) messages to stream (irrelevant for RO streams)\n"
        "\n"
        "A <mux> is specified in the form <source>=<dest>[,<option>][,<option>][...]'\n"
        "\n"
        "    <source> and <dest> specify a stream either by its <name> or its numeric ID (1 = first stream\n"
        "    specified on the command line, 2 = second stream, etc.). By default messages are transmitted through\n"
        "    the mux in either direction: forward from <source> to <dest> and reverse from <dest> to <source>.\n"
        "\n"
        "The <option>s for a <mux> are:\n"
        "\n"
        "- N=<name>     -- A short (like -n above) and unique (case insensitive) name for the mux\n"
        "- EF=on|off    -- Enable forward transmission from <source> to <dest>\n"
        "- ER=on|off    -- Enable reverse transmission from <dest> to <source>\n"
        "- FF=<filter>  -- Filter forward messages\n"
        "- FR=<filter>  -- Filter reverse messages\n"
        "\n"
        "The <filter>s for streams and muxes are in the form <msgname>[/<msgname>][...]. If a filter is set, each\n"
        "message is filtered by checking each <msgname> in the order given. If the message name begins with\n"
        "<msgname>, it passes through the filter. The special <msgname> '*' matches all messages names. If <msgname>\n"
        "is prefixed by a '!' then a message with a matching name does not pass the filter. For example:\n"
        "\n"
        "- 'UBX/NMEA'   -- matches all UBX and NMEA messages, i.e. filters out anything but UBX or NMEA messages\n"
        "- '!UBX-NAV/*' -- filters out all UBX-NAV messages, i.e. all but UBX-NAV message pass the filter\n"
        "\n"
        "Some special filter <msgname> are available (prefix with a '!' to negate the match):\n"
        "\n"
        "- 'X-NMEATOS'   -- filters for top-of-second (less than 50ms from .0) NMEA messages (RMC, GGA, GST and ZDA\n"
        "                   sentences only).\n"
        "- 'X-UBXNAVTOS' -- filters for top-of-second UBX-NAV-* messages.\n"
        "- 'X-RTCM3MSM'  -- filters for RTCM3 MSM messages (types 1071-1077, 1081-1087, 1091-1097, etc.)\n"
        "- 'X-RTCM3STA'  -- filters for RTCM3 station messages (types 1005, 1006 and 1032)\n"
        "- 'X-RTCM3BIAS' -- filters for RTCM3 bias messages (type 1230)\n"
        "\n"
        "A monitor generates epoch statistics on the stream input (only useful with -a or -r). A <mon> is specified\n"
        "in the form <str_name>[,<option>][,<option>][...]', where:\n"
        "\n"
        "- <str_name>   -- The name (or number) of the stream to monitor\n"
        "- N=<name>     -- A short (like -n above) and unique (case insensitive) name for the monitor\n"
        "- W=<s>[/<s>[/<s>]] -- Window sizes [s] for epoch statistics, 5.0 - 300.0 each. Up to three window sizes\n"
        "                  with increasing sizes. Default: 5.0/0.0/0.0.\n"
        "- E=<mode>     -- Is the epoch collection mode: 'receiver' (or '1', default) or 'corrections' (or '2')\n"
        "\n"
        "A HTTP API to monitor and control a running streammux can be enabled. The <api> is specified in the form\n"
        "[<host>]:<port>[/<prefix>], where <host> is the address (<IPv4> or [<IPv6>]) or the hostname, or empty\n"
        "to bind to all interfaces, <port> is the port number, and <prefix> is an optional prefix to strip (ignore)\n"
        "from the path in requests. The following API endpoints are available:\n"
        "\n"
        "- GET  /status  -- Get status data (the same as stored by the --report option)\n"
        "- POST /ctrl    -- Control a stream's enable read/write or a mux's enable forward/backwards. The data is\n"
        "                   a JSON array with three elements: [ \"<str_or_mux>\", null|true|false, null|true|false ],\n"
        "                   where <str_or_mux> identifies a stream or mux by its name or number, and the bools are\n"
        "                   used to enable/disable the read/write/forward/reverse, or nulls to leave unchanged\n"
        "- GET  /        -- A web app to monitor and control the running streammux\n"
        "\n"
        "\n"
        "Examples:\n"
        "\n"
        "    Offer a GNSS receiver on a serial port (/dev/ttyUSB1) at baudrate 38400 on a TCP/IP socket\n"
        "    (port 12345 on any interface):\n"
        "\n"
        "        streammux -s serial:///dev/ttyUSB1:38400 -s tcpsvr://:12345 -m 1=2\n"
        "\n"
        "    Optionally, streams and muxes can be named:\n"
        "\n"
        "        streammux -s serial:///dev/ttyUSB1:38400,N=rx -s tcpsvr://:12345,N=svr -m rx=svr,N=rx2svr\n"
        "\n"
        "    To prevent any data flowing from the server back to the receiver use one of:\n"
        "\n"
        "        streammux -s serial:///dev/ttyUSB1:38400    -s tcpsvr://:12345    -m 1=2,ER=off\n"
        "        streammux -s serial:///dev/ttyUSB1:38400    -s tcpsvr://:12345,WO -m 1=2\n"
        "        streammux -s serial:///dev/ttyUSB1:38400,RO -s tcpsvr://:12345    -m 1=2\n"
        "\n"
        "    For USB connections you could use use hotplugging and and a retry timeout:\n"
        "\n"
        "        streammux -s serial:///dev/serial/by-id/somereceiver,H=on,R=5.0 -s tcpsvr://:12345 -m 1=2\n"
        "\n"
        "    Offer the receiver on port as above, and also get correction data from a NTRIP caster. Feed the\n"
        "    RTCM3 data to the receiver, forward receiver NMEA-GN-GGA messages to the NTRIP caster and also provide\n"
        "    the RTMC3 data on another port:\n"
        "\n"
        "        streammux -s serial:///dev/ttyUSB1:38400,N=rx -s tcpsvr://:12345,N=svr_rx \\\n"
        "            -s ntripcli://user:pass@example.com/VRS,N=corr -s tcpsvr://:12346,N=svr_corr \\\n"
        "            -m rx=svr_rx -m corr=rx,FF=RTCM3,FR=NMEA-GN-GGA -m corr=svr_corr,ER=off\\\n"
        "\n"
        "    To see what's going on, add '-a :12346' and then browse to http://localhost:12346.\n"
        "\n"
        "    Log receiver to one hour logfiles with timestamp in filename:\n"
        "\n"
        "        streammux -s serial:///dev/ttyUSB1:38400::N=rx -s fileout://log_%Y%m%d-%h%M.ubx::S=1.0::N=log \\\n"
        "            -m rx=log\n"
        "\n", stdout);
    // clang-format on
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamMuxOptions::HandleOption(const Option& option, const std::string& argument)
{
    bool ok = true;
    switch (option.flag) {
        case 's':
            streams_.push_back(argument);
            break;
        case 'm':
            muxes_.push_back(argument);
            break;
        case 'M':
            monitors_.push_back(argument);
            break;
        case 'a':
            if (api_.empty()) {
                api_ = argument;
            } else {
                WARNING("--api can only be given once");
            }
            break;
        case 'A':
            assets_path_ = argument;
            break;
        case 'r':
            if (report_path_.empty()) {
                report_path_ = argument;
            } else {
                WARNING("--report can only be given once");
            }
            break;
        case 'n':
            if (name_.empty()) {
                name_ = argument;
            } else {
                WARNING("--name can only be given once");
            }
            break;
        case 'S':
            systemd_ = true;
            break;
        case 'c':
            if (!LoadConfigYaml(argument)) {
                ok = false;
            }
            break;
        default:
            ok = false;
            break;
    }
    return ok;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamMuxOptions::GetKeyValList(const YAML::Node& node, KeyValList& kvList) const
{
    if (!node || !node.IsSequence()) {
        return false;
    }
    for (auto& entry : node) {
        if (!entry.IsMap() || (entry.size() != 1)) {
            return false;
        }
        std::string key;
        std::string val;
        try {
            key = entry.begin()->first.as<std::string>();
            val = entry.begin()->second.as<std::string>();
        } catch (...) {
            return false;
        }
        if (key.empty() || val.empty()) {
            return false;
        }
        kvList.push_back({ key, val });
    }
    return true;
}

bool StreamMuxOptions::LoadConfigYaml(const std::string& file)
{
    std::vector<uint8_t> yaml;
    YAML::Node config;
    if (!FileSlurp(file, yaml) || !StringToYaml(BufToStr(yaml), config)) {
        return false;
    }

    // Add to vars
    if (config["vars"] && !GetKeyValList(config["vars"], yaml_vars_)) {
        WARNING("%s yaml must have .vars: [ { var: \"value\" }, { var: \"value\" }, ... ]", file.c_str());
        return false;
    }

    // Load options
    KeyValList opts;
    if (!GetKeyValList(config["streammux"], opts)) {
        WARNING("%s yaml must have .streammux: [ { option: \"value\" }, { option: \"value\" }, ... ]", file.c_str());
        return false;
    }

    // Process options
    for (auto& opt : opts) {
        // Interpolate vars
        for (auto v = yaml_vars_.rbegin(); v != yaml_vars_.rend(); v++) {
            StrReplace(opt.val_, "{" + v->key_ + "}", v->val_);
        }

        bool ok = false;
        if (opt.key_ == "config") {
            // nope!
        } else if (opt.key_ == "verbose") {
            HandleOption({ 'v', false, nullptr }, "");
        } else if (opt.key_ == "quiet") {
            HandleOption({ 'q', false, nullptr }, "");
        } else {
            for (auto& o : OPTIONS) {
                if (opt.key_ == o.name) {
                    if (HandleOption(o, opt.val_)) {
                        ok = true;
                        break;
                    } else {
                        return false;
                    }
                }
            }
        }
        if (!ok) {
            WARNING("%s: unknown option %s", file.c_str(), opt.key_.c_str());
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamMuxOptions::CheckOptions(const std::vector<std::string>& args)
{
    bool ok = true;
    if (!args.empty()) {
        WARNING("Spurious arguments");
        ok = false;
    }
    if (!name_.empty() && !CheckName(name_)) {
        WARNING("Bad -n <name>");
        ok = false;
    }

    for (std::size_t ix = 0; ix < streams_.size(); ix++) {
        DEBUG("streams[%" PRIuMAX "]  = %s", ix, streams_[ix].c_str());
    }
    for (std::size_t ix = 0; ix < muxes_.size(); ix++) {
        DEBUG("muxes[%" PRIuMAX "] = %s", ix, muxes_[ix].c_str());
    }
    for (std::size_t ix = 0; ix < monitors_.size(); ix++) {
        DEBUG("monitors[%" PRIuMAX "] = %s", ix, monitors_[ix].c_str());
    }
    DEBUG("api         = %s (%s)", api_.c_str(), assets_path_.c_str());
    DEBUG("report_path = %s", report_path_.c_str());
    DEBUG("name        = %s", name_.c_str());
    DEBUG("systemd     = %s", ToStr(systemd_));

    return ok;
}

/* ****************************************************************************************************************** */

json SmStats::ToJson() const
{
    json j = dynamic_cast<const ParserStats&>(*this);
    j["n_err"] = n_err_;
    j["n_filt"] = n_filt_;
    j["s_filt"] = s_filt_;
    return j;
}

/* ****************************************************************************************************************** */

bool SmFilter::Set(const std::string& spec)
{
    bool ok = true;
    spec_ = spec;
    for (auto part : StrSplit(spec, "/")) {
        const bool excl = StrStartsWith(part, "!");
        if (excl) {
            part = part.substr(1);
        }
        if (part.empty()) {
            ok = false;
            break;
        }
        const Mode mode = (excl ? Mode::EXCLUDE : Mode::INCLUDE);
        if (part == "X-NMEATOS") {
            entries_.push_back({ Type::NMEA_TOS, mode, "" });
        } else if (part == "X-UBXNAVTOS") {
            entries_.push_back({ Type::UBXNAV_TOS, mode, "" });
        } else if (part == "X-RTCM3MSM") {
            entries_.push_back({ Type::RTCM3_MSM, mode, "" });
        } else if (part == "X-RTCM3STA") {
            entries_.push_back({ Type::RTCM3_STA, mode, "" });
        } else if (part == "X-RTCM3BIAS") {
            entries_.push_back({ Type::RTCM3_BIAS, mode, "" });
        } else if (part == "*") {
            entries_.push_back({ Type::MSG_NAME, mode, part });
        } else {
            bool part_ok = false;
            for (const auto protocol : ALL_PROTOCOLS) {
                const std::string pstr = ProtocolStr(protocol);
                if ((part == pstr) || StrStartsWith(part, pstr + "-")) {
                    entries_.push_back({ Type::MSG_NAME, mode, part });
                    part_ok = true;
                    break;
                }
            }
            if (!part_ok) {
                WARNING("Bad filter entry '%s'", part.c_str());
                ok = false;
            }
        }
    }

    if (!ok) {
        entries_.clear();
        spec_.clear();
    }
    return ok;
}

// ---------------------------------------------------------------------------------------------------------------------

std::size_t SmFilter::Size() const
{
    return entries_.size();
}

// ---------------------------------------------------------------------------------------------------------------------

const std::string& SmFilter::Spec() const
{
    return spec_;
}

// ---------------------------------------------------------------------------------------------------------------------

bool SmFilter::Pass(const ParserMsg& msg) const
{
    if (entries_.empty()) {
        return true;
    }

    for (const auto& e : entries_) {
        bool match = false;

        switch (e.type_) {
            case Type::MSG_NAME:
                match = ((e.str_ == "*") || StrStartsWith(msg.name_, e.str_));
                break;
            case Type::NMEA_TOS:
                match = IsNmeaTos(msg);
                break;
            case Type::UBXNAV_TOS:
                match = IsUbxTos(msg);
                break;
            case Type::RTCM3_MSM:
                if (msg.proto_ == Protocol::RTCM3) {
                    const uint16_t type = Rtcm3Type(msg.Data());
                    match = (((type >= RTCM3_TYPE1071_MSGID) && (type >= RTCM3_TYPE1077_MSGID)) ||  // GPS
                            ((type >= RTCM3_TYPE1081_MSGID) && (type >= RTCM3_TYPE1087_MSGID)) ||  // GLO
                            ((type >= RTCM3_TYPE1091_MSGID) && (type >= RTCM3_TYPE1097_MSGID)) ||  // GAL
                            ((type >= RTCM3_TYPE1101_MSGID) && (type >= RTCM3_TYPE1107_MSGID)) ||  // SBAS
                            ((type >= RTCM3_TYPE1111_MSGID) && (type >= RTCM3_TYPE1117_MSGID)) ||  // QZSS
                            ((type >= RTCM3_TYPE1121_MSGID) && (type >= RTCM3_TYPE1127_MSGID)) ||  // BDS
                            ((type >= RTCM3_TYPE1131_MSGID) && (type >= RTCM3_TYPE1137_MSGID)));   // NAVIC
                }
                break;
            case Type::RTCM3_STA:
                if (msg.proto_ == Protocol::RTCM3) {
                    const uint16_t type = Rtcm3Type(msg.Data());
                    match = ((type == RTCM3_TYPE1005_MSGID) || (type == RTCM3_TYPE1006_MSGID) ||
                             (type == RTCM3_TYPE1032_MSGID));
                }
                break;
            case Type::RTCM3_BIAS:
                if (msg.proto_ == Protocol::RTCM3) {
                    const uint16_t type = Rtcm3Type(msg.Data());
                    match = (type == RTCM3_TYPE1230_MSGID);
                }
                break;
        }
        if (match) {
            switch (e.mode_) {
                case Mode::INCLUDE: return true;
                case Mode::EXCLUDE: return false;
            }
        }
    }

    return false;
}

/* ****************************************************************************************************************** */

json SmStr::ToJson()
{
    const auto opts = stream_->GetOpts();
    const Duration stateage = (state_ts_.IsZero() ? Duration() : Time::FromClockRealtime() - state_ts_);

    return json::object({
        { "name", name_ },
        { "type", StreamTypeStr(opts.type_) },
        { "mode", StreamModeStr(opts.mode_) },
        { "state", StreamStateStr(stream_->GetState()) },
        { "stateage", (int)stateage.GetSec(0) },
        { "stateagestr", stateage.Stringify(0) },
        { "statestrs", statestrs_ },
        { "error", StreamErrorStr(stream_->GetError()) },
        { "info", stream_->GetInfo() },
        { "disp", opts.disp_ },
        { "opts", opts.opts_ },
        { "filter", json::array({ filter_read_.Spec(), filter_write_.Spec() }) },
        { "stats", json::array({ stats_read_.ToJson(), stats_write_.ToJson() }) },
        { "can", json::array({ can_read_, can_write_ }) },
        { "ena", json::array({ ena_read_.load(), ena_write_.load() }) },
    });
}

/* ****************************************************************************************************************** */

json SmMux::ToJson()
{
    return json::object({
        { "name", name_ },
        { "can", json::array({ can_fwd_, can_rev_ }) },
        { "ena", json::array({ ena_fwd_.load(), ena_rev_.load() }) },
        { "src", src_->name_ },
        { "dst", dst_->name_ },
        { "filter", json::array({ filter_fwd_.Spec(), filter_rev_.Spec() }) },
        { "stats", json::array({ stats_fwd_.ToJson(), stats_rev_.ToJson() }) },
    });
}

/* ****************************************************************************************************************** */

void SmMon::Add(const ParserMsg& msg)
{
    // TRACE("Mon(%s) add %s", name_.c_str(), msg.name_.c_str());
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.size() >= QUEUE_MAX) {
        WARNING_THR(5000, "Mon(%s) queue ovfl", name_.c_str());
    } else {
        queue_.emplace_back(msg, Time::FromClockRealtime());
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void SmMon::Process()
{
    std::vector<QueueEntry> queue;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        std::swap(queue, queue_);
    }

    TRACE("Mon(%s) process %" PRIuMAX " messages", name_.c_str(), queue.size());
    for (const auto& entry : queue) {
        const auto epoch = coll_->Collect(entry.first);
        if (!epoch) {
            continue;
        }

        Meas meas;
        if (epoch->time.IsZero()) {
            // Assume reception time
            meas.time_ = entry.second;

            // But we can maybe reconstruct it...
            if ((coll_->GetMode() == EpochCollector::Mode::CORRECTIONS) && epoch->haveGpsTow) {
                meas.time_.SetWnoTow({ entry.second.GetWnoTow(WnoTow::Sys::GPS).wno_, epoch->gpsTow, WnoTow::Sys::GPS });
                meas.latency_ = (entry.second - meas.time_).GetSec();
                // ...and maybe not
                if ((meas.latency_ < 0.0f) || (meas.latency_ > 10.0f)) {
                    meas.latency_ = -1.0f;
                    meas.time_ = entry.second;
                }
            }
        } else {
            meas.time_ = epoch->time;
            meas.latency_ = (entry.second - epoch->time).GetSec();
        }

        // TRACE("Mon(%s) epoch %s, latency %.3f", name_.c_str(), epoch->str, meas.latency);

        // No jump back in time, keeps meas_ ordered by time
        if (!meas_.empty() && (meas.time_ <= meas_.back().time_)) {
            return;
        }

        // Collect values of interest
        if (epoch->haveFixType) {
            meas.fixType_ = EnumToVal(epoch->fixType);
        }
        if (epoch->havePdop) {
            meas.pDOP_ = epoch->pDOP;
        }
        if (epoch->haveTdop) {
            meas.tDOP_ = epoch->tDOP;
        }
        if (epoch->haveGdop) {
            meas.gDOP_ = epoch->gDOP;
        }
        if (epoch->haveNumSigUsed) {
            meas.numSigUsed_      = epoch->numSigUsed.numTotal;
            meas.numSigUsedGps_   = epoch->numSigUsed.numGps;
            meas.numSigUsedSbas_  = epoch->numSigUsed.numSbas;
            meas.numSigUsedGal_   = epoch->numSigUsed.numGal;
            meas.numSigUsedBds_   = epoch->numSigUsed.numBds;
            meas.numSigUsedQzss_  = epoch->numSigUsed.numQzss;
            meas.numSigUsedGlo_   = epoch->numSigUsed.numGlo;
            meas.numSigUsedNavic_ = epoch->numSigUsed.numNavic;
            meas.numSigUsedL1_    = epoch->numSigUsedL1.numTotal;
            meas.numSigUsedE6_    = epoch->numSigUsedE6.numTotal;
            meas.numSigUsedL2_    = epoch->numSigUsedL2.numTotal;
            meas.numSigUsedL5_    = epoch->numSigUsedL5.numTotal;
        }
        meas.rateMsgs_ = epoch->msgsRate;
        meas.rateBytes_ = epoch->bytesRate;
        std::vector<float> cnoL1;
        std::vector<float> cnoE6;
        std::vector<float> cnoL2;
        std::vector<float> cnoL5;
        for (const auto &sig: epoch->sigs) {
            if ((sig.cno > 0.0f) && sig.anyUsed) {
                switch (sig.band) {
                    case Band::L1: cnoL1.push_back(sig.cno); break;
                    case Band::E6: cnoE6.push_back(sig.cno); break;
                    case Band::L2: cnoL2.push_back(sig.cno); break;
                    case Band::L5: cnoL5.push_back(sig.cno); break;
                    case Band::UNKNOWN: break;
                }
            }
        }
        std::sort(cnoL1.rbegin(), cnoL1.rend());
        std::sort(cnoE6.rbegin(), cnoE6.rend());
        std::sort(cnoL2.rbegin(), cnoL2.rend());
        std::sort(cnoL5.rbegin(), cnoL5.rend());
        if (cnoL1.size() >=  5) { meas.cnoAvgTop5L1_ = std::accumulate(cnoL1.begin(), cnoL1.begin() + 5, 0.0f) / 5.0f; }
        if (cnoE6.size() >=  5) { meas.cnoAvgTop5E6_ = std::accumulate(cnoE6.begin(), cnoE6.begin() + 5, 0.0f) / 5.0f; }
        if (cnoL2.size() >=  5) { meas.cnoAvgTop5L2_ = std::accumulate(cnoL2.begin(), cnoL2.begin() + 5, 0.0f) / 5.0f; }
        if (cnoL5.size() >=  5) { meas.cnoAvgTop5L5_ = std::accumulate(cnoL5.begin(), cnoL5.begin() + 5, 0.0f) / 5.0f; }

        // Add
        {
            std::unique_lock<std::mutex> lock(mutex_);
            meas_.push_back(meas);
        }
    }

    // Limit max size, expire old
    {
        std::unique_lock<std::mutex> lock(mutex_);
        const auto maxWinSize = std::max({winSizes_[0], winSizes_[1], winSizes_[2]});
        const auto maxTime = Time::FromClockRealtime() - (maxWinSize + 5.0);  // keep a little bit more...
        while ((meas_.size() > MAX_MEAS) || (!meas_.empty() && (meas_.front().time_ < maxTime))) {
            meas_.pop_front();
        }
        // TRACE("Mon(%s) %" PRIuMAX " meas %s .. %s dt=%.1f/%.1f n=%" PRIuMAX "/%" PRIuMAX, name_.c_str(), meas_.size(),
        //     meas_.front().time_.StrUtcTime().c_str(), meas_.back().time_.StrUtcTime().c_str(),
        //     (meas_.back().time_ - meas_.front().time_).GetSec(), maxWinSize.GetSec(), meas_.size(), MAX_MEAS);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

struct SmMeasStats {

    void operator()(const float val) { acc_(val); }

    json ToJson() const {
        const std::size_t count = boost::accumulators::count(acc_);
        if (count >= MIN_MEAS) {
            return json::object({
                { "num",  count },
                { "mean", boost::accumulators::mean(acc_) },
                { "min",  boost::accumulators::min(acc_) },
                { "max",  boost::accumulators::max(acc_) },
                { "std",  std::sqrt(boost::accumulators::variance(acc_)) },
            });
        } else {
            return nullptr;
        }
    }

    static constexpr std::size_t MIN_MEAS = 5;

   private:
    boost::accumulators::accumulator_set<float, boost::accumulators::stats<
            boost::accumulators::tag::count,
            boost::accumulators::tag::mean,
            boost::accumulators::tag::min,
            boost::accumulators::tag::max,
            boost::accumulators::tag::variance>> acc_;

};

/* ****************************************************************************************************************** */

json SmMon::ToJson()
{
    Process();
    auto res = json::object({
        { "name", name_ },
        { "src", src_->name_ },
        { "stats", json::array() },
    });

    if (meas_.size() < SmMeasStats::MIN_MEAS) {
        return res;
    }

    // Compute statistics
    SmMeasStats latency;
    SmMeasStats fixType;
    SmMeasStats pDOP;
    SmMeasStats tDOP;
    SmMeasStats gDOP;
    SmMeasStats numSigUsed;
    SmMeasStats numSigUsedL1;
    SmMeasStats numSigUsedE6;
    SmMeasStats numSigUsedL2;
    SmMeasStats numSigUsedL5;
    SmMeasStats numSigUsedGps;
    SmMeasStats numSigUsedSbas;
    SmMeasStats numSigUsedGal;
    SmMeasStats numSigUsedBds;
    SmMeasStats numSigUsedQzss;
    SmMeasStats numSigUsedGlo;
    SmMeasStats numSigUsedNavic;
    SmMeasStats cnoAvgTop5;
    SmMeasStats cnoAvgTop5L1;
    SmMeasStats cnoAvgTop5E6;
    SmMeasStats cnoAvgTop5L2;
    SmMeasStats cnoAvgTop5L5;
    SmMeasStats rateMsgs;
    SmMeasStats rateBytes;
    auto stats = json::array();

    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto measIt = meas_.rbegin();
        const auto& tNewest = measIt->time_;
        // TRACE("Mon(%s) oldest %s", name_.c_str(), meas_.front().time_.StrUtcTime().c_str());
        // TRACE("Mon(%s) newest %s", name_.c_str(), tNewest.StrUtcTime().c_str());
        const auto dt_thrs = Duration::FromSec(-0.005);
        for (std::size_t ix = 0; ix < winSizes_.size(); ix++) {
            if (winSizes_[ix].IsZero()) {
                continue;
            }
            while (measIt != meas_.rend()) {
                const auto dt = tNewest - measIt->time_;
                // TRACE("Mon(%s) meas  %s %.3f > %.3f", name_.c_str(), measIt->time_.StrUtcTime().c_str(), dt.GetSec(),
                    // winSizes_[ix].GetSec());

                const bool winComplete = ((dt - winSizes_[ix]) > dt_thrs);

                if (winComplete)  {
                    stats.push_back(json::object({
                        { "winSize",        winSizes_[ix].GetSec(3) },
                        { "latency",         latency.ToJson() },
                        { "fixType",         fixType.ToJson() },
                        { "pDOP",            pDOP.ToJson() },
                        { "tDOP",            tDOP.ToJson() },
                        { "gDOP",            gDOP.ToJson() },
                        { "numSigUsed",      numSigUsed.ToJson() },
                        { "numSigUsedL1",    numSigUsedL1.ToJson() },
                        { "numSigUsedE6",    numSigUsedE6.ToJson() },
                        { "numSigUsedL2",    numSigUsedL2.ToJson() },
                        { "numSigUsedL5",    numSigUsedL5.ToJson() },
                        { "numSigUsedGps",   numSigUsedGps.ToJson() },
                        { "numSigUsedSbas",  numSigUsedSbas.ToJson() },
                        { "numSigUsedGal",   numSigUsedGal.ToJson() },
                        { "numSigUsedBds",   numSigUsedBds.ToJson() },
                        { "numSigUsedQzss",  numSigUsedQzss.ToJson() },
                        { "numSigUsedGlo",   numSigUsedGlo.ToJson() },
                        { "numSigUsedNavic", numSigUsedNavic.ToJson() },
                        { "cnoAvgTop5",      cnoAvgTop5.ToJson() },
                        { "cnoAvgTop5L1",    cnoAvgTop5L1.ToJson() },
                        { "cnoAvgTop5E6",    cnoAvgTop5E6.ToJson() },
                        { "cnoAvgTop5L2",    cnoAvgTop5L2.ToJson() },
                        { "cnoAvgTop5L5",    cnoAvgTop5L5.ToJson() },
                        { "rateMsgs",        rateMsgs.ToJson() },
                        { "rateBytes",       rateBytes.ToJson() },
                    }));
                }

                // clang-format off
                if (measIt->latency_         > 0.0f) { latency(measIt->latency_); }
                if (measIt->fixType_         > 0.0f) { fixType(measIt->fixType_); }
                if (measIt->pDOP_            > 0.0f) { pDOP(measIt->pDOP_); }
                if (measIt->tDOP_            > 0.0f) { tDOP(measIt->tDOP_); }
                if (measIt->gDOP_            > 0.0f) { gDOP(measIt->gDOP_); }
                if (measIt->numSigUsed_      > 0)    { numSigUsed(measIt->numSigUsed_); }
                if (measIt->numSigUsedL1_    > 0)    { numSigUsedL1(measIt->numSigUsedL1_); }
                if (measIt->numSigUsedE6_    > 0)    { numSigUsedE6(measIt->numSigUsedE6_); }
                if (measIt->numSigUsedL2_    > 0)    { numSigUsedL2(measIt->numSigUsedL2_); }
                if (measIt->numSigUsedL5_    > 0)    { numSigUsedL5(measIt->numSigUsedL5_); }
                if (measIt->numSigUsedGps_   > 0)    { numSigUsedGps(measIt->numSigUsedGps_); }
                if (measIt->numSigUsedSbas_  > 0)    { numSigUsedSbas(measIt->numSigUsedSbas_); }
                if (measIt->numSigUsedGal_   > 0)    { numSigUsedGal(measIt->numSigUsedGal_); }
                if (measIt->numSigUsedBds_   > 0)    { numSigUsedBds(measIt->numSigUsedBds_); }
                if (measIt->numSigUsedQzss_  > 0)    { numSigUsedQzss(measIt->numSigUsedQzss_); }
                if (measIt->numSigUsedGlo_   > 0)    { numSigUsedGlo(measIt->numSigUsedGlo_); }
                if (measIt->numSigUsedNavic_ > 0)    { numSigUsedNavic(measIt->numSigUsedNavic_); }
                if (measIt->cnoAvgTop5L1_    > 0.0f) { cnoAvgTop5L1(measIt->cnoAvgTop5L1_); cnoAvgTop5(measIt->cnoAvgTop5L1_); }
                if (measIt->cnoAvgTop5E6_    > 0.0f) { cnoAvgTop5E6(measIt->cnoAvgTop5E6_); cnoAvgTop5(measIt->cnoAvgTop5E6_); }
                if (measIt->cnoAvgTop5L2_    > 0.0f) { cnoAvgTop5L2(measIt->cnoAvgTop5L2_); cnoAvgTop5(measIt->cnoAvgTop5L2_); }
                if (measIt->cnoAvgTop5L5_    > 0.0f) { cnoAvgTop5L5(measIt->cnoAvgTop5L5_); cnoAvgTop5(measIt->cnoAvgTop5L5_); }
                if (measIt->rateMsgs_        > 0.0f) { rateMsgs(measIt->rateMsgs_); }
                if (measIt->rateBytes_       > 0.0f) { rateBytes(measIt->rateBytes_); }
                // clang-format on

                measIt++;

                if (winComplete) {
                    break;
                }
            }
        }
    }

    if (!stats.empty()) {
        res["stats"] = stats;
    }

    return res;
}

/* ****************************************************************************************************************** */

void SmProc::Update()
{
    perf_.Update();

    Meas meas;
    meas.cpu_ = perf_.cpu_curr_;
    meas.mem_ = perf_.mem_curr_;
    meas_.push_back(meas);

    while (meas_.size() > NUM_MEAS) {
        meas_.pop_front();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

json SmProc::ToJson()
{
    SmMeasStats cpu;
    SmMeasStats mem;
    for (const auto& meas : meas_) {
        cpu(meas.cpu_);
        mem(meas.mem_);
    }

    const auto now = Time::FromClockRealtime();

    return json::object({
        { "name",       name_ },
        { "time",       now.GetPosix() },
        { "time_str",   now.StrUtcTime(0) },
        { "cpu_curr",   perf_.cpu_curr_ },
        { "cpu_stats",  cpu.ToJson() },
        { "mem_curr",   perf_.mem_curr_ },
        { "mem_stats",  mem.ToJson() },
        { "uptime",     (uint64_t)perf_.uptime_.GetSec() },
        { "uptime_str", perf_.uptime_.Stringify(0) },
        { "pid",        perf_.pid_ },
    });
}

/* ****************************************************************************************************************** */

std::string ConsumeOption(std::string& spec, const std::string& option, const std::string& def)
{
    std::string res;
    std::vector<std::string> keep;
    const auto parts = StrSplit(spec, ",");
    for (auto& part : parts) {
        if (res.empty() && StrStartsWith(part, option + "=")) {
            res = part.substr(option.size() + 1);
        } else {
            keep.push_back(part);
        }
    }
    spec = StrJoin(keep, ",");
    return res.empty() ? def : res;
}

// ---------------------------------------------------------------------------------------------------------------------

bool CheckName(const std::string& name)
{
    const std::regex re("^[a-zA-Z][a-zA-Z0-9_]{1,9}$");
    std::smatch m;
    return std::regex_match(name, m, re);
}

/* ****************************************************************************************************************** */
}  // namespace ffapps::streammux
