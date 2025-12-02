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
#include <atomic>
#include <cstdio>
#include <list>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

/* EXTERNAL */
#include <ffxx/http.hpp>
#include <ffxx/stream.hpp>
#include <ffxx/utils.hpp>
#include <fpsdk_common/app.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/parser.hpp>
#include <fpsdk_common/path.hpp>
#include <fpsdk_common/string.hpp>
#include <fpsdk_common/thread.hpp>
#include <fpsdk_common/time.hpp>
#include <fpsdk_common/types.hpp>
#include <nlohmann/json.hpp>

/* PACKAGE */

namespace ffapps::streammux {
/* ****************************************************************************************************************** */

using namespace ffxx;
using namespace fpsdk::common::app;
using namespace fpsdk::common::logging;
using namespace fpsdk::common::parser;
using namespace fpsdk::common::path;
using namespace fpsdk::common::string;
using namespace fpsdk::common::time;
using namespace fpsdk::common::thread;
using namespace nlohmann;

// ---------------------------------------------------------------------------------------------------------------------

struct SmStats : public ParserStats
{
    uint64_t n_err_ = 0;
    uint64_t n_filt_ = 0;
    uint64_t s_filt_ = 0;
};

using SmFilter = std::vector<std::pair<std::string, bool>>;

struct SmStr
{
    std::string name_;
    std::shared_ptr<Stream> stream_;
    SmFilter filter_read_;
    SmFilter filter_write_;
    std::string filter_read_str_;
    std::string filter_write_str_;
    SmStats stats_read_;
    SmStats stats_write_;
    bool can_read_ = true;
    bool can_write_ = true;
    std::atomic<bool> ena_read_ = true;
    std::atomic<bool> ena_write_ = true;
    bool used_ = false;
    std::atomic<bool> connected_ = false;
    std::list<std::string> statestrs_;
};

using SmStrPtr = std::shared_ptr<SmStr>;

struct SmMux
{
    std::string name_;
    SmFilter filter_fwd_;
    SmFilter filter_rev_;
    std::string filter_fwd_str_;
    std::string filter_rev_str_;
    SmStats stats_fwd_;
    SmStats stats_rev_;
    bool can_fwd_ = true;
    bool can_rev_ = true;
    std::atomic<bool> ena_fwd_ = true;
    std::atomic<bool> ena_rev_ = true;
    SmStrPtr src_;
    SmStrPtr dst_;
};

using SmMuxPtr = std::shared_ptr<SmMux>;

/* ****************************************************************************************************************** */

// Program options
class StreamMuxOptions : public ProgramOptions
{
   public:
    StreamMuxOptions() /* clang-format off */ :
        ProgramOptions("streammux", {
            { 's', true, "stream" }, { 'm', true, "mux" }, { 'a', true, "api" }, { 'A', true, "assets" },
            { 'r', true, "report" } })  // clang-format on
    {
    }

    // clang-format off
    std::vector<std::string> streams_;
    std::vector<std::string> muxes_;
    std::string api_;
    std::string assets_path_;
    std::string report_path_;
    // clang-format on

    void PrintVersion() override final
    {
        std::fprintf(stdout, "%s (%s, %s)\n%s\n%s\n", app_name_.c_str(),
        #ifdef NDEBUG
                "release",
        #else
                "debug",
        #endif
                GetVersionString(), GetCopyrightString(), GetLicenseString());
    }

    void PrintHelp() override final
    {
        // clang-format off
        std::fputs(
            "\n"
            "Tool to connect many streams to each other with filtering capabilities."
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
            "    -r <path>, --report <path>     -- Report stats to JSON file given by <path>, updated once every second.\n"
            "                                      Use a tmpfs RAM disk, such as /run/user/$UID/streammux.json\n"
            "    -a <api>, --api <api>          -- Provide HTTP API (and web UI), see below\n"
            // "    -A <path>, --assets            -- Use web UI assets from path instead of built-in assets (for development)\n"
            "\n"
            "The <mux>es connect the <stream>s to each other. Data is processed on message (frame) level. Therefore,\n"
            "data from multiples inputs muxed to the same output does not interfere with each other, as long as the data\n"
            "consists of messages of supported protocols (UBX, NMEA, RTCM3, etc.).\n"
            "\n", stdout);
            std::fputs(StreamHelpScreen(), stdout);
        std::fputs(
            "\n"
            "Additionally to the general stream options described above streammux supports the following:\n"
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
            "- N=<name>     -- A short and concise name for the mux ([a-zA-Z0-9_)]\n"
            "- EF=on|off    -- Enable forward transmission from <source> to <dest>\n"
            "- ER=on|off    -- Enable reverse transmission from <dest> to <source>\n"
            "- FF=<filter>  -- Filter forward messages\n"
            "- FR=<filter>  -- Filter reverse messages\n"
            "\n"
            "The <filter>s for streams and muxes are in the form <name>[/<name>][...]. If a filter is set, each message\n"
            "is filtered by checking each <name> in the order given. If the message name begins with <name>, it passes\n"
            "through the filter. The special <name> '*' matches all messages names. If <name> is prefixed by a '!'\n"
            "then a message with a matching name does not pass the filter. Some examples:\n"
            "\n"
            "- 'UBX/NMEA'   -- matches all UBX and NMEA messages, i.e. filters out anything but UBX or NMEA messages\n"
            "- '!UBX-NAV/*' -- filters out all UBX-NAV messages, i.e. all but UBX-NAV message pass the filter\n"
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

    bool HandleOption(const Option& option, const std::string& argument) final
    {
        bool ok = true;
        switch (option.flag) {  // clang-format off
            case 's': streams_.push_back(argument); break;
            case 'm': muxes_.push_back(argument);   break;
            case 'a': api_         = argument;      break;
            case 'A': assets_path_ = argument;      break;
            case 'r': report_path_ = argument;      break;
            default: ok = false; break;
        }  // clang-format on
        return ok;
    }

    bool CheckOptions(const std::vector<std::string>& args) final
    {
        bool ok = true;
        if (streams_.empty() || muxes_.empty() || !args.empty()) {
            ok = false;
        }

        for (std::size_t ix = 0; ix < streams_.size(); ix++) {
            DEBUG("streams[%" PRIuMAX "] = %s", ix, streams_[ix].c_str());
        }
        for (std::size_t ix = 0; ix < muxes_.size(); ix++) {
            DEBUG("muxes[%" PRIuMAX "] = %s", ix, muxes_[ix].c_str());
        }
        DEBUG("api         = %s (%s)", api_.c_str(), assets_path_.c_str());
        DEBUG("report_path = %s", report_path_.c_str());

        return ok;
    }
};

/* ****************************************************************************************************************** */

static std::string ConsumeOption(std::string& spec, const std::string& option, const std::string& def = "")
{
    std::string res = def;
    std::vector<std::string> keep;
    const auto parts = StrSplit(spec, ",");
    for (auto& part : parts) {
        if (StrStartsWith(part, option + "=")) {
            res = part.substr(option.size() + 1);
        } else {
            keep.push_back(part);
        }
    }
    spec = StrJoin(keep, ",");
    return res;
}

// ---------------------------------------------------------------------------------------------------------------------

static bool SetFilter(SmFilter& filter, const std::string& spec)
{
    bool ok = true;
    for (auto part : StrSplit(spec, "/")) {
        const bool excl = StrStartsWith(part, "!");
        if (excl) {
            part = part.substr(1);
        }
        if (!part.empty()) {
            filter.emplace_back(part, !excl);  // push_back({part, excl});
        } else {
            ok = false;
        }
    }
    return ok;
}

// ---------------------------------------------------------------------------------------------------------------------

static bool PassFilter(const SmFilter& filter, const ParserMsg& msg)
{
    for (auto& f : filter) {
        if ((f.first == "*") || StrStartsWith(msg.name_, f.first)) {
            return f.second;
        }
    }
    return filter.empty();
}

// ---------------------------------------------------------------------------------------------------------------------

static bool CheckName(const std::string& name)
{
    const std::regex re("^[a-zA-Z][a-zA-Z0-9_]{0,9}$");
    std::smatch m;
    return std::regex_match(name, m, re);
}

/* ****************************************************************************************************************** */

class StreamMux
{
   public:
    StreamMux(const StreamMuxOptions& opts) /* clang-format off */ :
    opts_            { opts },
    status_thread_   { "status", std::bind(&StreamMux::StatusWorker, this) }  // clang-format on
    {
    }

    ~StreamMux()
    {
    }

    bool Run();

   private:
    StreamMuxOptions opts_;
    std::vector<SmStrPtr> strs_;
    std::vector<SmMuxPtr> muxs_;
    Thread status_thread_;
    PerfStats perf_;
    std::mutex status_mutex_;
    HttpApiServer::Response status_res_;
    std::unique_ptr<HttpApiServer> api_;

    SmStrPtr FindStr(const std::string& name_or_nr);
    SmMuxPtr FindMux(const std::string& name_or_nr);
    bool InitApi();
    bool ApiHandler(const HttpApiServer::Request& req, HttpApiServer::Response& res);
    bool ApiHandlerCtrl(const json& data, HttpApiServer::Response& res);
    bool StatusWorker();
};

// ---------------------------------------------------------------------------------------------------------------------

bool StreamMux::Run()
{
    bool ok = true;

    NOTICE("flipflip's StreamMux, version %s, PID %" PRIu64, GetVersionString(), perf_.pid_);
    INFO("%s", GetCopyrightString());
    INFO("%s", GetLicenseString());

    // Setup streams
    for (std::string spec : opts_.streams_) {
        auto str = std::make_shared<SmStr>();
        bool sok = true;

        // ER=0|1, EW=0|1
        bool ena_read = true;
        if (!StrToValue(ConsumeOption(spec, "ER", "on"), ena_read)) {
            WARNING("Bad ER option value in stream %" PRIuMAX, strs_.size() + 1);
            sok = false;
        }
        str->ena_read_ = ena_read;
        bool ena_write = true;
        if (!StrToValue(ConsumeOption(spec, "EW", "on"), ena_write)) {
            WARNING("Bad EW option value in stream %" PRIuMAX, strs_.size() + 1);
            sok = false;
        }
        str->ena_write_ = ena_write;
        // FR=.... FW=...
        str->filter_read_str_ = ConsumeOption(spec, "FR");
        if (!SetFilter(str->filter_read_, str->filter_read_str_)) {
            WARNING("Bad FR option value in stream %" PRIuMAX, strs_.size() + 1);
            sok = false;
        }
        str->filter_write_str_ = ConsumeOption(spec, "FW");
        if (!SetFilter(str->filter_write_, str->filter_write_str_)) {
            WARNING("Bad FW option value in stream %" PRIuMAX, strs_.size() + 1);
            sok = false;
        }

        // The rest is the stream spec
        DEBUG("from spec %s", spec.c_str());
        str->stream_ = Stream::FromSpec(spec);
        if (!str->stream_) {
            sok = false;
            continue;
        }

        auto str_opts = str->stream_->GetOpts();
        str->name_ = str_opts.name_;
        str->can_read_ = (str_opts.mode_ != StreamMode::WO);
        str->can_write_ = (str_opts.mode_ != StreamMode::RO);

        if (!CheckName(str->name_)) {
            WARNING("Bad stream name '%s'", str->name_.c_str());
            sok = false;
        } else if (FindStr(str->name_)) {
            WARNING("Duplicate stream name '%s'", str->name_.c_str());
            sok = false;
        }

        if (!sok) {
            ok = false;
            continue;
        }

        INFO("Stream(%s) ER=%s EW=%s FR=<%" PRIuMAX "> FW=<%" PRIuMAX ">", str->name_.c_str(),
            str->ena_read_ ? "on" : "off", str->ena_write_ ? "on" : "off", str->filter_read_.size(),
            str->filter_write_.size());

        strs_.push_back(str);
    }

    if (!ok) {
        return false;
    }

    // Setup muxes
    for (std::string spec : opts_.muxes_) {
        auto mux = std::make_shared<SmMux>();
        bool mok = true;

        // N=...
        mux->name_ = ConsumeOption(spec, "N");
        if (mux->name_.empty()) {
            mux->name_ = "mux" + std::to_string(muxs_.size() + 1);
        }

        if (FindStr(mux->name_) || FindMux(mux->name_)) {
            WARNING("Duplicate mux or stream name '%s'", mux->name_.c_str());
            mok = false;
        }
        if (!CheckName(mux->name_)) {
            WARNING("Bad mux name '%s'", mux->name_.c_str());
            mok = false;
        }

        // EF=..., ER=...
        bool ena_fwd = true;
        if (!StrToValue(ConsumeOption(spec, "EF", "on"), ena_fwd)) {
            WARNING("Bad EF option value in mux %" PRIuMAX, muxs_.size() + 1);
            mok = false;
        }
        mux->ena_fwd_ = ena_fwd;
        bool ena_rev = true;
        if (!StrToValue(ConsumeOption(spec, "ER", "on"), ena_rev)) {
            WARNING("Bad ER option value in mux %" PRIuMAX, muxs_.size() + 1);
            mok = false;
        }
        mux->ena_rev_ = ena_rev;
        // FW=..., FR=...
        mux->filter_fwd_str_ = ConsumeOption(spec, "FF");
        if (!SetFilter(mux->filter_fwd_, mux->filter_fwd_str_)) {
            WARNING("Bad FW option value in mux %" PRIuMAX, muxs_.size() + 1);
            mok = false;
        }
        mux->filter_rev_str_ = ConsumeOption(spec, "FR");
        if (!SetFilter(mux->filter_rev_, mux->filter_rev_str_)) {
            WARNING("Bad FR option value in mux %" PRIuMAX, muxs_.size() + 1);
            mok = false;
        }

        // in-out, in=out
        const auto parts = StrSplit(spec, "=");
        if ((parts.size() != 2) || parts[0].empty() || parts[1].empty()) {
            WARNING("%s: bad spec: %s", mux->name_.c_str(), spec.c_str());
            mok = false;
        }

        if (mok) {
            mux->src_ = FindStr(parts[0]);
            mux->dst_ = FindStr(parts[1]);
            if (!mux->src_) {
                WARNING("%s could not find src stream %s", mux->name_.c_str(), parts[0].c_str());
                mok = false;
            }
            if (!mux->dst_) {
                WARNING("%s could not find dst stream %s", mux->name_.c_str(), parts[1].c_str());
                mok = false;
            }
            if (ok && (mux->src_ == mux->dst_)) {
                WARNING("%s src and dst are the same", mux->name_.c_str());
                mok = false;
            }
        }

        if (!mok) {
            ok = false;
            continue;
        }

        INFO("Mux(%s) %s=%s EF=%s ER=%s FF=<%" PRIuMAX "> FR=<%" PRIuMAX ">", mux->name_.c_str(),
            mux->src_->name_.c_str(), mux->dst_->name_.c_str(), mux->ena_fwd_ ? "on" : "off",
            mux->ena_rev_ ? "on" : "off", mux->filter_fwd_.size(), mux->filter_rev_.size());

        muxs_.push_back(mux);
    }

    if (!ok) {
        return false;
    }

    // Check that all streams are used
    for (const auto& str : strs_) {
        bool used = false;
        for (const auto& mux : muxs_) {
            if ((mux->src_ == str) || (mux->dst_ == str)) {
                used = true;
                break;
            }
        }
        if (!used) {
            WARNING("Unused stream %s", str->name_.c_str());
            ok = false;
        }
    }

    if (!ok) {
        return false;
    }

    // Start API, status thread
    if (!opts_.api_.empty() && !InitApi()) {
        ok = false;
    }
    if ((api_ || !opts_.report_path_.empty()) && !status_thread_.Start()) {
        ok = false;
    }
    if (!ok) {
        return false;
    }

    SigIntHelper sigint;

    // Observe streams for data available to read and state changes
    BinarySemaphore sem;
    for (auto& str : strs_) {
        str->stream_->AddReadObserver([&str, &sem]() { sem.Notify(); });
        str->stream_->AddStateObserver([&str, &ok, &sigint](const StreamState old_state, const StreamState new_state,
                                           const StreamError error, const std::string& info) {
            // To know if we can Read() on the stream
            str->connected_ = (new_state == StreamState::CONNECTED);

            // Streams should never close unless they have a problem, in which case we'll abort
            if (ok && !sigint.ShouldAbort() && (old_state != new_state) && (new_state == StreamState::CLOSED)) {
                // Ignore FILEIN streams, they are supposed to close at some point. Their use here is questionable...
                if (str->stream_->GetType() != StreamType::FILEIN) {
                    WARNING("Stream %s has closed unexpectedly", str->name_.c_str());
                    ok = false;
                }
            }

            // Store away the last few state info messages
            std::string statestr = Time::FromClockRealtime().StrUtcTime(1) + " " + StreamStateStr(new_state);
            if (error != StreamError::NONE) {
                statestr += " ";
                statestr += StreamErrorStr(error);
            }
            str->statestrs_.push_front(statestr + " (" + info + ")");
            while (str->statestrs_.size() > 5) {
                str->statestrs_.pop_back();
            }
        });
    }

    // Start all streams
    INFO("Starting streams");
    for (auto& str : strs_) {
        if (!str->stream_->Start()) {
            ok = false;
        }
    }
    if (!ok) {
        for (auto& str : strs_) {
            str->stream_->Stop();
        }
        return false;
    }

    NOTICE("Running...");
    // This is the main loop where all the receiving, forwarding and writing of the messages happens...
    bool again = false;
    ParserMsg msg;
    while (ok && !sigint.ShouldAbort()) {
        // Limit processing up to some number messages from each stream at a time
        static constexpr int max_msgs_per_str = 10;
        // Whether we should process more immediately or we should wait (sleep) for more data
        again = false;

        // Process all streams
        for (auto& str : strs_) {
            int n_msgs = 0;

            // Read messages from stream (input)
            while (str->can_read_ && str->connected_ && str->stream_->Read(msg) && (n_msgs++ < max_msgs_per_str)) {
                // Discard input if read isn't enabled
                if (!str->ena_read_) {
                    continue;
                }

                // Skip message if it doesn't pass the read (input) filter
                if (!PassFilter(str->filter_read_, msg)) {
                    str->stats_read_.n_filt_++;
                    str->stats_read_.s_filt_ += msg.data_.size();
                    continue;
                }

                // TRACE("%s < %s", str->name_.c_str(), msg.name_.c_str());

                // Update read (input) statistics
                str->stats_read_.Update(msg);

                // Forward messages through the connected muxes
                for (auto& mux : muxs_) {
                    auto& src = mux->src_;
                    auto& dst = mux->dst_;

                    // Forward (src->dst)
                    if (mux->can_fwd_ && mux->ena_fwd_ && (src == str)) {
                        if (PassFilter(mux->filter_fwd_, msg)) {
                            if (dst->can_write_ && dst->ena_write_ && dst->connected_) {
                                if (PassFilter(dst->filter_write_, msg)) {
                                    if (dst->stream_->Write(msg.data_)) {
                                        dst->stats_write_.Update(msg);
                                    } else {
                                        dst->stats_write_.n_err_++;
                                    }
                                } else {
                                    dst->stats_write_.n_filt_++;
                                    dst->stats_write_.s_filt_ += msg.data_.size();
                                }
                            }
                            mux->stats_fwd_.Update(msg);
                        } else {
                            mux->stats_fwd_.n_filt_++;
                            mux->stats_fwd_.s_filt_ += msg.data_.size();
                        }
                    }

                    // Reverse (dst->src)
                    else if (mux->can_rev_ && mux->ena_rev_ && (dst == str)) {
                        if (PassFilter(mux->filter_rev_, msg)) {
                            if (dst->can_write_ && src->ena_write_ && src->connected_) {
                                if (PassFilter(src->filter_write_, msg)) {
                                    if (src->stream_->Write(msg.data_)) {
                                        src->stats_write_.Update(msg);
                                    } else {
                                        src->stats_write_.n_err_++;
                                    }
                                } else {
                                    src->stats_write_.n_filt_++;
                                    src->stats_write_.s_filt_ += msg.data_.size();
                                }
                            }
                            mux->stats_rev_.Update(msg);
                        } else {
                            mux->stats_rev_.n_filt_++;
                            mux->stats_rev_.s_filt_ += msg.data_.size();
                        }
                    }
                }

                again = true;
            }
        }

        // All done, wait for more data
        if (!again) {
            sem.WaitFor(1000);
        }
    }

    if (api_) {
        api_->Stop();
    }
    status_thread_.Stop();

    for (auto& str : strs_) {
        str->stream_->Stop();
    }

    return ok;
}

// ---------------------------------------------------------------------------------------------------------------------

SmStrPtr StreamMux::FindStr(const std::string& name_or_nr)
{
    const auto stream =
        std::find_if(strs_.begin(), strs_.end(), [name_or_nr](const auto& cand) { return cand->name_ == name_or_nr; });
    std::size_t nr = 0;
    if (stream != strs_.end()) {
        return *stream;
    } else if (StrToValue(name_or_nr, nr) && (nr > 0) && (nr <= strs_.size())) {
        return strs_[nr - 1];
    } else {
        return nullptr;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

SmMuxPtr StreamMux::FindMux(const std::string& name_or_nr)
{
    const auto stream =
        std::find_if(muxs_.begin(), muxs_.end(), [name_or_nr](const auto& cand) { return cand->name_ == name_or_nr; });
    std::size_t nr = 0;
    if (stream != muxs_.end()) {
        return *stream;
    } else if (StrToValue(name_or_nr, nr) && (nr > 0) && (nr <= muxs_.size())) {
        return muxs_[nr - 1];
    } else {
        return nullptr;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

static json StatsToJson(const SmStats& stats)
{
    return {
        { "n_msgs", stats.n_msgs_ },
        { "s_msgs", stats.s_msgs_ },
        { "n_fpa", stats.n_fpa_ },
        { "s_fpa", stats.s_fpa_ },
        { "n_fpb", stats.n_fpb_ },
        { "s_fpb", stats.s_fpb_ },
        { "n_nmea", stats.n_nmea_ },
        { "s_nmea", stats.s_nmea_ },
        { "n_ubx", stats.n_ubx_ },
        { "s_ubx", stats.s_ubx_ },
        { "n_rtcm3", stats.n_rtcm3_ },
        { "s_rtcm3", stats.s_rtcm3_ },
        { "n_unib", stats.n_unib_ },
        { "s_unib", stats.s_unib_ },
        { "n_novb", stats.n_novb_ },
        { "s_novb", stats.s_novb_ },
        { "n_spartn", stats.n_spartn_ },
        { "s_spartn", stats.s_spartn_ },
        { "n_other", stats.n_other_ },
        { "s_other", stats.s_other_ },
        { "n_err", stats.n_err_ },
        { "n_filt", stats.n_filt_ },
        { "s_filt", stats.s_filt_ },
    };
}

bool StreamMux::StatusWorker()
{
    const std::string report_path_tmp = opts_.report_path_ + ".tmp";
    bool report_ok = !opts_.report_path_.empty();
    while (!status_thread_.ShouldAbort()) {
        // Update status data
        // - Process info
        perf_.Update();
        json proc = json::object({
            { "time", Time::FromClockRealtime().StrIsoTime() },
            { "mem_curr", perf_.mem_curr_ },
            { "mem_peak", perf_.mem_peak_ },
            { "cpu_curr", perf_.cpu_curr_ },
            { "cpu_avg", perf_.cpu_avg_ },
            { "cpu_peak", perf_.cpu_peak_ },
            { "uptime", perf_.uptime_.Stringify(0) },
            { "pid", perf_.pid_ },
        });
        // - Streams
        json strs = json::array();
        for (const auto& str : strs_) {
            json infos = json::array();
            const auto opts = str->stream_->GetOpts();
            strs.push_back(json::object({
                { "name", str->name_ },
                { "type", StreamTypeStr(opts.type_) },
                { "mode", StreamModeStr(opts.mode_) },
                { "state", StreamStateStr(str->stream_->GetState()) },
                { "statestrs", str->statestrs_ },
                { "error", StreamErrorStr(str->stream_->GetError()) },
                { "info", str->stream_->GetInfo() },
                { "disp", opts.disp_ },
                { "opts", opts.opts_ },
                { "filter", json::array({ str->filter_read_str_, str->filter_write_str_ }) },
                { "stats", json::array({ StatsToJson(str->stats_read_), StatsToJson(str->stats_write_) }) },
                { "can", json::array({ str->can_read_, str->can_write_ }) },
                { "ena", json::array({ str->ena_read_.load(), str->ena_write_.load() }) },
            }));
        }
        // - Muxes
        json muxs = json::array();
        for (const auto& mux : muxs_) {
            muxs.push_back(json::object({
                { "name", mux->name_ },
                { "can", json::array({ mux->can_fwd_, mux->can_rev_ }) },
                { "ena", json::array({ mux->ena_fwd_.load(), mux->ena_rev_.load() }) },
                { "src", mux->src_->name_ },
                { "dst", mux->dst_->name_ },
                { "filter", json::array({ mux->filter_fwd_str_, mux->filter_rev_str_ }) },
                { "stats", json::array({ StatsToJson(mux->stats_fwd_), StatsToJson(mux->stats_rev_) }) },
            }));
        }
        // - And finally the entire status data is:
        json status = json::object({
            { "api", "status" },
            { "proc", proc },
            { "strs", strs },
            { "muxs", muxs },
        });
        const auto status_json =
            StrToBuf(status.dump(LoggingGetParams().level_ >= LoggingLevel::DEBUG ? 4 : -1) + "\n");

        // Update /status API HTTP response, and also broadcast it to the websockets
        if (api_) {
            std::unique_lock<std::mutex> lock(status_mutex_);
            status_res_.type_ = HttpApiServer::CONTENT_TYPE_JSON;
            status_res_.body_ = status_json;
            api_->SendWs("/ws", status_res_);
        }

        // Update status file
        if (report_ok) {
            if (FileSpew(report_path_tmp, status_json)) {
                std::error_code ec;
                std::filesystem::rename(report_path_tmp, opts_.report_path_);
                if (ec) {
                    WARNING("Failed to rename %s: %s", report_path_tmp.c_str(), ec.message().c_str());
                    report_ok = false;
                }
            } else {
                report_ok = false;
            }
            if (!report_ok) {
                WARNING("Disabling writing status file %s", opts_.report_path_.c_str());
                std::error_code ec;
                std::filesystem::remove(opts_.report_path_, ec);
            }
        }

        status_thread_.SleepUntil(1000);
    }

    // Cleanup
    if (!opts_.report_path_.empty()) {
        std::error_code ec;
        std::filesystem::remove(report_path_tmp, ec);
        std::filesystem::remove(opts_.report_path_, ec);
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamMux::InitApi()
{
    api_ = HttpApiServer::Create(opts_.api_);
    if (!api_) {
        return false;
    }

    auto handler = std::bind(&StreamMux::ApiHandler, this, std::placeholders::_1, std::placeholders::_2);
    api_->SetHandler(HttpApiServer::Method::GET, "", handler);
    api_->SetHandler(HttpApiServer::Method::GET, "/", handler);
    api_->SetHandler(HttpApiServer::Method::GET, "/streammux.html", handler);
    api_->SetHandler(HttpApiServer::Method::GET, "/streammux.css", handler);
    api_->SetHandler(HttpApiServer::Method::GET, "/streammux.js", handler);
    api_->SetHandler(HttpApiServer::Method::GET, "/status", handler);
    api_->SetHandler(HttpApiServer::Method::GET, "/version", handler);
    api_->SetHandler(HttpApiServer::Method::POST, "/ctrl", handler);
    api_->SetHandler(HttpApiServer::Method::WS, "/ws", handler);

    return api_->Start();
}

#include "streammux_css.hpp"
#include "streammux_html.hpp"
#include "streammux_js.hpp"

bool StreamMux::ApiHandler(const HttpApiServer::Request& req, HttpApiServer::Response& res)
{
    bool ok = true;

    // App
    if (req.path_.empty() || (req.path_ == "/") || (req.path_ == "/streammux.html")) {
        res.type_ = HttpApiServer::CONTENT_TYPE_HTML;
        if (opts_.assets_path_.empty()) {
            res.body_ = { (const uint8_t*)STREAMMUX_HTML, (const uint8_t*)STREAMMUX_HTML + STREAMMUX_HTML_LEN };
        } else {
            ok = FileSlurp(opts_.assets_path_ + "/streammux.html", res.body_);
        }
    } else if (req.path_ == "/streammux.css") {
        res.type_ = HttpApiServer::CONTENT_TYPE_CSS;
        if (opts_.assets_path_.empty()) {
            res.body_ = { (const uint8_t*)STREAMMUX_CSS, (const uint8_t*)STREAMMUX_CSS + STREAMMUX_CSS_LEN };
        } else {
            ok = FileSlurp(opts_.assets_path_ + "/streammux.css", res.body_);
        }
    } else if (req.path_ == "/streammux.js") {
        res.type_ = HttpApiServer::CONTENT_TYPE_JS;
        if (opts_.assets_path_.empty()) {
            res.body_ = { (const uint8_t*)STREAMMUX_JS, (const uint8_t*)STREAMMUX_JS + STREAMMUX_JS_LEN };
        } else {
            ok = FileSlurp(opts_.assets_path_ + +"/streammux.js", res.body_);
        }
    }
    // API
    else if (req.path_ == "/status") {
        std::unique_lock<std::mutex> lock(status_mutex_);
        res = status_res_;
    } else if (req.path_ == "/version") {
        res.type_ = HttpApiServer::CONTENT_TYPE_JSON;
        res.body_ = /* clang-format off */ StrToBuf(json::object( {
            { "api", "version" },
            { "version",   GetVersionString()   },
            { "copyright", GetCopyrightString() },
            { "license",   GetLicenseString()   } }).dump());  // clang-format on
    } else if (req.path_ == "/ctrl") {
        ok = ApiHandlerCtrl(req.data_, res);
    }
    // Websocket
    else if (req.path_ == "/ws") {
        if (req.data_.is_object() && (req.data_.find("api") != req.data_.end()) && req.data_["api"].is_string()) {
            const std::string api = req.data_["api"].get<std::string>();
            if ((api == "ctrl") && (req.data_.find("data") != req.data_.end()) && req.data_["data"].is_array()) {
                ok = ApiHandlerCtrl(req.data_["data"], res);
            }
        } else {
            res.error_ = "bad request";
        }
    }
    // Path we registered but didn't handle above
    else {
        ok = false;
        res.error_ = "path not handled";
    }

    return ok;
}

bool StreamMux::ApiHandlerCtrl(const json& data, HttpApiServer::Response& res)
{
    DEBUG("API ctrl: %s", data.dump().c_str());
    res.error_ = "bad request data";
    // Request: [ "str_or_mux", true|false|null, true|false|null ]
    if (!data.is_array() || (data.size() != 3) || !data[0].is_string()) {
        return false;
    }
    const std::string str_or_mux = data[0].get<std::string>();
    auto str = FindStr(str_or_mux);
    auto mux = FindMux(str_or_mux);
    if (!str && !mux) {
        return false;
    }
    auto& curr_ena_read = (str ? str->ena_read_ : mux->ena_fwd_);
    auto& curr_ena_write = (str ? str->ena_write_ : mux->ena_rev_);
    if ((!data[1].is_null() && !data[1].is_boolean()) || (!data[2].is_null() && !data[2].is_boolean())) {
        return false;
    }

    if (!data[1].is_null()) {
        curr_ena_read = data[1].get<bool>();
    }
    if (!data[2].is_null()) {
        curr_ena_write = data[2].get<bool>();
    }

    res.error_.clear();
    res.type_ = HttpApiServer::CONTENT_TYPE_JSON;
    res.body_ = StrToBuf(json::object(
        { // clang-format off
        { "api", "ctrl" },
        { "data", json::array({ str_or_mux, curr_ena_read.load(), curr_ena_write.load() }) }
    }).dump());  // clang-format on
    return true;
}

/* ****************************************************************************************************************** */
}  // namespace ffapps::streammux

/* ****************************************************************************************************************** */

int main(int argc, char** argv)
{
    using namespace ffapps::streammux;
#ifndef NDEBUG
    fpsdk::common::app::StacktraceHelper stacktrace;
#endif
    bool ok = true;

    // Parse command line arguments
    StreamMuxOptions opts;
    if (!opts.LoadFromArgv(argc, argv)) {
        ok = false;
    }

    if (ok) {
        StreamMux app(opts);
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
