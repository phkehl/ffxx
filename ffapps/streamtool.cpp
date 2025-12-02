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
 * @brief streamtool main
 */

/* LIBC/STL */
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>

/* EXTERNAL */
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
//
#include <boost/histogram.hpp>
#include <ffxx/stream.hpp>
#include <ffxx/utils.hpp>
#include <ffxx/epoch.hpp>
#include <fpsdk_common/app.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/math.hpp>
#include <fpsdk_common/parser/fpa.hpp>
#include <fpsdk_common/parser/types.hpp>
#include <fpsdk_common/path.hpp>
#include <fpsdk_common/string.hpp>
#include <fpsdk_common/time.hpp>
#include <nlohmann/json.hpp>

/* PACKAGE */
#include "streamtool_stats.hpp"

namespace ffapps::streamtool {

/* ****************************************************************************************************************** */

using namespace ffxx;
using namespace fpsdk::common::app;
using namespace fpsdk::common::logging;
using namespace fpsdk::common::math;
using namespace fpsdk::common::parser;
using namespace fpsdk::common::parser::fpa;
using namespace fpsdk::common::path;
using namespace fpsdk::common::string;
using namespace fpsdk::common::time;

// ---------------------------------------------------------------------------------------------------------------------

// Program options
class StreamToolOptions : public ProgramOptions
{
   public:
    StreamToolOptions()  // clang-format off
        : ProgramOptions("streamtool", {
            { 'f', false, "filter" }, { 'x', false, "hexdump" }, { 'j', true, "json" },
            { 'n', false, "nomsg"  }, { 'N', false, "nostats" } })
        {};  // clang-format on

    // clang-format off
    bool         filter_   = false;
    bool         hexdump_  = false;
    std::string  json_;
    bool         nomsgs_   = false;
    bool         nostats_  = false;
    std::string  spec_;
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
            "This dumps information about messages received and summary statistics to stdout. With '-f'\n"
            "it instead connects a stream to stdin and stdout. For most streams both are connected to\n"
            "the stream. For write-only streams only stdin is connected and for read-only streams only\n"
            "stdout is connected.\n"
            "\n"
            "Usage:\n"
            "\n"
            "    streamtool [flags] <stream>\n"
            "\n"
            "Where:\n"
            "\n", stdout);
        std::fputs(COMMON_FLAGS_HELP, stdout);
        std::fputs(
            "    -f, --filter             -- Act as stdin/stdout filter instead of dumping\n"
            "    -x, --hexdump            -- Hexdump messages (not with -f or -j)\n"
            "    -j <file>, --json <file> -- Output statistics as json to <file> (not with -f)\n"
            "    -n, --nomsgs             -- Don't print messages data, but print statistics repeatedly (not with -f)\n"
            "    -N, --nostats            -- Don't print statistics (not with -f)\n"
            "    <stream>                 -- Stream spec\n"
            "\n", stdout);
        std::fputs(ffxx::StreamHelpScreen(), stdout);
        std::fputs(
            "\n"
            "Examples:\n"
            "\n"
            "    Connect to TCP server on localhost port 12345 and print info about received messages:\n"
            "\n"
            "        streamtool tcpcli://localhost:12345\n"
            "\n"
            "    Connect to Vision-RTK 2 sensor TCP0 port and silently collect statistics data for a minute and then\n"
            "    generate a report and view it:\n"
            "\n"
            "        timeout -s INT 60 streamtool -n -N tcpcli://10.0.2.1:21000 -j stats.json\n"
            "        streamtool-plot stats.json\n"
            "        xdg-open stats.html\n"
            "\n"
            "    Send a file to a serial port. Note how pv is used to rate-limit the transmit and chunk size so that\n"
            "    the serial stream buffers do not overflow as well as a sleep to ensure the port has enough time to\n"
            "    finish the transmission on the wire.\n"
            "\n"
            "        (cat send.txt | pv -q -L 80000 -B 100; sleep 1;) | streamtool -f serial:///dev/ttyUSB0@921600\n"
            "\n"
            "\n", stdout);
        // clang-format on
    }

    bool HandleOption(const Option& option, const std::string& argument) final
    {
        bool ok = true;
        switch (option.flag) {  // clang-format off
            case 'f': filter_   = true;     break;
            case 'x': hexdump_  = true;     break;
            case 'j': json_     = argument; break;
            case 'n': nomsgs_   = true;     break;
            case 'N': nostats_  = true;     break;
            default: ok = false; break;
        }  // clang-format on
        return ok;
    }

    bool CheckOptions(const std::vector<std::string>& args) final
    {
        bool ok = true;

        // Exactly one positional argument
        if (args.size() == 1) {
            spec_ = args[0];
        }

        DEBUG("spec      = '%s'", spec_.c_str());
        DEBUG("filter    = %s", ToStr(filter_));
        DEBUG("hexdump   = %s", ToStr(hexdump_));
        DEBUG("json      = %s", json_.c_str());
        DEBUG("nomsg     = %s", ToStr(nomsgs_));
        DEBUG("nostats   = %s", ToStr(nostats_));

        if (spec_.empty()) {
            WARNING("Missing stream spec");
            ok = false;
        }
        if (filter_ && (hexdump_ || !json_.empty() || nomsgs_ || nostats_)) {
            WARNING("Cannot do '-xjnN' with '-f'");
            ok = false;
        }

        return ok;
    }
};

/* ****************************************************************************************************************** */

class StreamTool
{
   public:
    StreamTool(const StreamToolOptions& args) : opts_{ args }
    {
    }

    bool Run()
    {
        bool ok = true;
        bool closed = false;

        auto stream = Stream::FromSpec(opts_.spec_);
        if (!stream) {
            return false;
        }

        const bool can_read = (stream->GetMode() != StreamMode::WO);
        const bool can_write = (stream->GetMode() != StreamMode::RO);
        DEBUG("can_read=%s can_write=%s", can_read ? "true" : "false", can_write ? "true" : "false");

        if (!opts_.filter_ && !can_read) {
            WARNING("Cannot dump from a %s stream", StreamModeStr(stream->GetMode()));
            return false;
        }

        INFO("Starting stream");
        if (!stream->Start()) {
            return false;
        }

        stream->AddStateObserver(
            [&closed, &ok, &stream](const StreamState, const StreamState state, const StreamError, const std::string&) {
                switch (state) {
                    case StreamState::CLOSED:
                        closed = true;
                        break;
                    case StreamState::CONNECTING:
                    case StreamState::CONNECTED:
                    case StreamState::ERROR:
                        break;
                }
            });

        const Duration sleep = Duration::FromSec(0.01);

        // Run as a filter and connect stdin/stdout to the stream
        if (opts_.filter_) {
            SigIntHelper sigint;
            SigPipeHelper sigpipe;
            INFO("Using%s%s", can_write ? " stdin->stream" : "", can_read ? " stream->stdout" : "");

            // We'll have to watch stdin and/or stdout
            struct pollfd p_fds[3];
            std::memset(&p_fds, 0, sizeof(p_fds));
            // - stdin
            p_fds[0].fd = (can_write ? fileno(stdin) : -1);
            p_fds[0].events = POLLIN | POLLERR | POLLHUP;
            // - stdout
            p_fds[1].fd = (can_read ? fileno(stdout) : -1);
            p_fds[1].events = /*POLLOUT |*/ POLLERR | POLLHUP;
            // - helper pipe
            p_fds[2].fd = -1;  // see below
            p_fds[2].events = POLLIN;
            bool stdin_done = false;

            // Create a pipe to signal that the stream has data for us to process
            int s_fds[2] = { -1, -1 };
            if (can_read) {
                if (pipe(s_fds) != 0) {
                    WARNING("pipe fail: %s", StrError(errno).c_str());
                    ok = false;
                }
                p_fds[2].fd = s_fds[0];
            }
            stream->AddReadObserver([&s_fds]() { (void)(write(s_fds[1], "", 1) + 1); });

            while (!sigint.ShouldAbort() && !sigpipe.Raised() && ok && !closed && !stdin_done) {
                // Wait until stream is connected
                if (stream->GetState() != StreamState::CONNECTED) {
                    sleep.Sleep();
                    continue;
                }

                // Wait for i/o event. We have setup p_fds[] to watch for stdin, stdout and the helper pipe.
                const int res = poll(p_fds, 3, 337);
                if ((res < 0) && (errno != EINTR)) {
                    WARNING("poll fail: %s", StrError(errno).c_str());
                    ok = false;
                    break;
                }
                // DEBUG("poll %d %d %d", p_fds[0].revents, p_fds[1].revents, p_fds[2].revents);

                // We can read from stdin
                if ((p_fds[0].revents & POLLIN) != 0) {
                    uint8_t inbuf[10000];
                    const auto nin = read(fileno(stdin), inbuf, sizeof(inbuf));
                    if (nin <= 0) {
                        if (nin < 0) {
                            WARNING("stdin fail: %s", StrError(errno).c_str());
                        }
                        stdin_done = true;
                    } else if (!stream->Write(inbuf, nin)) {
                        // Not much we can do. Probably we're sending too much data in too short time (e.g. on a slow
                        // serial port)
                    }
                }

                // We can read from the stream
                if ((p_fds[2].revents & POLLIN) != 0) {
                    uint8_t dummy[1000];
                    (void)(read(s_fds[0], dummy, sizeof(dummy)) + 1);
                    ParserMsg msg;
                    while (stream->Read(msg)) {
                        const std::size_t n = std::fwrite(msg.data_.data(), 1, msg.data_.size(), stdout);
                        if (n != msg.data_.size()) {
                            WARNING("Short write to stdout %" PRIuMAX " %" PRIuMAX ": %s", n, msg.data_.size(),
                                StrError(errno).c_str());
                            ok = false;
                        }
                        std::fflush(stdout);
                    }
                }

                // Abort if stdin or stdout closed or error
                if (((p_fds[0].revents | p_fds[1].revents) & (POLLHUP | POLLERR)) != 0) {
                    break;
                }
            }
        }
        // Dump mode
        else {
            t_start_ = Time::FromClockRealtime();
            INFO("Press C-c or send SIGINT to stop%s", opts_.nostats_ ? "" : " and print statistics");
            auto last_stat = Time::FromClockRealtime() - 4.0;
            SigIntHelper sigint;
            EpochCollector coll;
            EpochPtr epoch;

            ParserMsg msg;
            while (!sigint.ShouldAbort() && ok && !closed) {
                // Wait until stream is connected
                if (stream->GetState() != StreamState::CONNECTED) {
                    sleep.Sleep();
                    continue;
                }

                // Dump received messages
                if (stream->Read(msg, 50)) {
                    const auto now = Time::FromClockRealtime();
                    epoch = coll.Collect(msg);
                    if (epoch) {
                        const auto epoch_info = stats_.Update(*epoch, now);
                        if (!opts_.nomsgs_) {
                            PrintEpochData(*epoch, epoch_info);
                        }
                    }

                    const auto msg_info = stats_.Update(msg, now);
                    if (!opts_.nomsgs_) {
                        PrintMessageData(msg, msg_info);
                    } else if (!opts_.nostats_ && ((now - last_stat).GetSec() >= 5.0)) {
                        PrintStats(stream->GetParserStats());
                        last_stat = now;
                    }
                }
            }

            t_stop_ = Time::FromClockRealtime();

            if (!opts_.nostats_) {
                PrintStats(stream->GetParserStats());
            }
            if (!opts_.json_.empty()) {
                if (!SaveStats(stream)) {
                    ok = false;
                }
            }
        }

        INFO("Stopping stream");
        stream->Stop(opts_.filter_ ? 1000 : 0);

        stream.reset();

        return ok;
    }

   private:
    // -----------------------------------------------------------------------------------------------------------------

    StreamToolOptions opts_;  //!< Program options
    Stats stats_;
    Time t_start_;
    Time t_stop_;

    // -----------------------------------------------------------------------------------------------------------------

    void PrintMessageData(const ParserMsg& msg, const Stats::MsgInfo& info)
    {
        msg.MakeInfo();

        if ((msg.seq_ % 100) == 1) {
            // clang-format off
            std::printf(
                "+----------+----------+-------+----------+----------------------+--------------------------------+--------+--------+------------------------------------------------------------------------------------------------------+\n"
                "| Sequence |   Offset |  Size | Protocol | Message              | UniqueName                     |Interval|Latency | Info                                                                                                 |\n"
                "|----------+----------+-------+----------+----------------------+--------------------------------+--------+--------+------------------------------------------------------------------------------------------------------|\n");
            // clang-format on
        }

        // clang-format off
        std::printf("| %8" PRIuMAX " | %8" PRIuMAX " | %5" PRIuMAX " | %-8s | %-20s | %-30s | %6.3f | %6.3f | %-100s |\n",
            msg.seq_, info.offs_, msg.data_.size(),
            ProtocolStr(msg.proto_), msg.name_.c_str(), info.unique_name_.c_str(),
            !info.interval_.IsZero() ? info.interval_.GetSec() : NAN,
            !info.latency_.IsZero()  ? info.latency_.GetSec() : NAN,
            msg.info_.c_str());
        // clang-format on
        if (opts_.hexdump_) {
            for (auto& line : HexDump(msg.data_)) {
                //              "+----------+----------+-------+----------+----------------------+--------------------------------+--------+--------+------------------------------------------------------------------------------------------------------+\n"
                // clang-format off
                std::printf("|                                                                                                                    %-100s |\n", line.c_str());
                // clang-format on
            }
        }
        std::fflush(stdout);
    }

    void PrintEpochData(const Epoch& epoch, const Stats::MsgInfo& info)
    {
        // clang-format off
        std::printf("| %8" PRIuMAX " | -        | -      | -       | -                    | EPOCH                          | %6.3f | %6.3f | %-100s |\n",
            epoch.seq,
            !info.interval_.IsZero() ? info.interval_.GetSec() : NAN,
            !info.latency_.IsZero()  ? info.latency_.GetSec() : NAN,
            epoch.str); // clang-format on
    }

    // -----------------------------------------------------------------------------------------------------------------

    void PrintStats(const ParserStats& parser_stats)
    {
        if (parser_stats.n_msgs_ == 0) {
            return;
        }

        // Parser stats
        auto& ps = parser_stats;
        const double p_n = (ps.n_msgs_ > 0 ? 100.0 / (double)ps.n_msgs_ : 0.0);
        const double p_s = (ps.s_msgs_ > 0 ? 100.0 / (double)ps.s_msgs_ : 0.0);
        // clang-format off
        std::printf("\n"
                    "+------------+---------------+-----------------+\n"
                    "| Protocol   | Messages      | Bytes           |\n"
                    "|------------+---------------+-----------------|\n");
        const char* fmt = "| %-10s | %6"  PRIuMAX " %5.1f%% | %8"  PRIuMAX " %5.1f%% |\n";
        // clang-format off
        std::printf(fmt, "Total",                       ps.n_msgs_,   (double)ps.n_msgs_   * p_n, ps.s_msgs_,   (double)ps.s_msgs_   * p_s);
        std::printf(fmt, ProtocolStr(Protocol::FP_A),   ps.n_fpa_,    (double)ps.n_fpa_    * p_n, ps.s_fpa_,    (double)ps.s_fpa_    * p_s);
        std::printf(fmt, ProtocolStr(Protocol::FP_B),   ps.n_fpb_,    (double)ps.n_fpb_    * p_n, ps.s_fpb_,    (double)ps.s_fpb_    * p_s);
        std::printf(fmt, ProtocolStr(Protocol::NMEA),   ps.n_nmea_,   (double)ps.n_nmea_   * p_n, ps.s_nmea_,   (double)ps.s_nmea_   * p_s);
        std::printf(fmt, ProtocolStr(Protocol::UBX),    ps.n_ubx_,    (double)ps.n_ubx_    * p_n, ps.s_ubx_,    (double)ps.s_ubx_    * p_s);
        std::printf(fmt, ProtocolStr(Protocol::RTCM3),  ps.n_rtcm3_,  (double)ps.n_rtcm3_  * p_n, ps.s_rtcm3_,  (double)ps.s_rtcm3_  * p_s);
        std::printf(fmt, ProtocolStr(Protocol::NOV_B),  ps.n_novb_,   (double)ps.n_novb_   * p_n, ps.s_novb_,   (double)ps.s_novb_   * p_s);
        std::printf(fmt, ProtocolStr(Protocol::UNI_B),  ps.n_unib_,   (double)ps.n_unib_   * p_n, ps.s_novb_,   (double)ps.s_unib_   * p_s);
        std::printf(fmt, ProtocolStr(Protocol::SPARTN), ps.n_spartn_, (double)ps.n_spartn_ * p_n, ps.s_spartn_, (double)ps.s_spartn_ * p_s);
        std::printf(fmt, ProtocolStr(Protocol::OTHER),  ps.n_other_,  (double)ps.n_other_  * p_n, ps.s_other_,  (double)ps.s_other_  * p_s);
        // clang-format on
        std::printf("+------------+---------------+-----------------+\n");

        // Message stats
        // clang-format off
        std::printf("\n"
            "+-------------------------------------+---------------+-----------------+----------------------------------------------------------------+----------------------------------------------------------------+----------------------------------------------------------------+\n"
            "|                                     | Messages      | Bytes           | Latency [s]                                                    | Interval [s]                                                   | Frequency [Hz]                                                 |\n"
            "| Message (UniqueName)                |  Count Percnt |    Count Percnt |      N   Mean    Std    Min %5.1f%% %5.1f%% %5.1f%% %5.1f%%    Max |      N   Mean    Std    Min %5.1f%% %5.1f%% %5.1f%% %5.1f%%    Max |      N   Mean    Std    Min %5.1f%% %5.1f%% %5.1f%% %5.1f%%    Max |\n"
            "|-------------------------------------+---------------+-----------------+----------------------------------------------------------------+----------------------------------------------------------------+----------------------------------------------------------------|\n",
            Stats::MsgStats::PROB[0] * 100.0, Stats::MsgStats::PROB[1] * 100.0, Stats::MsgStats::PROB[2] * 100.0, Stats::MsgStats::PROB[3] * 100.0,
            Stats::MsgStats::PROB[0] * 100.0, Stats::MsgStats::PROB[1] * 100.0, Stats::MsgStats::PROB[2] * 100.0, Stats::MsgStats::PROB[3] * 100.0,
            Stats::MsgStats::PROB[0] * 100.0, Stats::MsgStats::PROB[1] * 100.0, Stats::MsgStats::PROB[2] * 100.0, Stats::MsgStats::PROB[3] * 100.0);  // clang-format on
        for (const auto& entry : stats_.msg_stats_) {
            const auto& ms = entry.second;
            const std::size_t n_lat = boost::accumulators::count(ms.acc_latency_);
            const std::size_t n_int = boost::accumulators::count(ms.acc_interval_);
            const std::size_t n_fre = boost::accumulators::count(ms.acc_frequency_);
            // clang-format off
            std::printf("| %-35s | %6"  PRIuMAX " %5.1f%% | %8"  PRIuMAX " %5.1f%% |"
                " %6" PRIuMAX " %6.3f %6.3f %6.3f %6.3f %6.3f %6.3f %6.3f %6.3f |"
                " %6" PRIuMAX " %6.3f %6.3f %6.3f %6.3f %6.3f %6.3f %6.3f %6.3f |"
                " %6" PRIuMAX " %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f |\n",
                ms.unique_name_.c_str(), ms.count_, (double)ms.count_ * p_n, ms.bytes_, (double)ms.bytes_ * p_s,
                n_lat,
                n_lat > 0 ? boost::accumulators::mean(ms.acc_latency_)                   : NAN,
                n_lat > 9 ? std::sqrt(boost::accumulators::variance(ms.acc_latency_))    : NAN,
                n_lat > 1 ? boost::accumulators::min(ms.acc_latency_)                    : NAN,
                n_lat > 9 ? boost::accumulators::extended_p_square(ms.acc_latency_)[0]   : NAN,
                n_lat > 9 ? boost::accumulators::extended_p_square(ms.acc_latency_)[1]   : NAN,
                n_lat > 9 ? boost::accumulators::extended_p_square(ms.acc_latency_)[2]   : NAN,
                n_lat > 9 ? boost::accumulators::extended_p_square(ms.acc_latency_)[3]   : NAN,
                n_lat > 1 ? boost::accumulators::max(ms.acc_latency_)                    : NAN,
                n_int,
                n_int > 0 ? boost::accumulators::mean(ms.acc_interval_)                  : NAN,
                n_int > 9 ? std::sqrt(boost::accumulators::variance(ms.acc_interval_))   : NAN,
                n_int > 1 ? boost::accumulators::min(ms.acc_interval_)                   : NAN,
                n_int > 9 ? boost::accumulators::extended_p_square(ms.acc_interval_)[0]  : NAN,
                n_int > 9 ? boost::accumulators::extended_p_square(ms.acc_interval_)[1]  : NAN,
                n_int > 9 ? boost::accumulators::extended_p_square(ms.acc_interval_)[2]  : NAN,
                n_int > 9 ? boost::accumulators::extended_p_square(ms.acc_interval_)[3]  : NAN,
                n_int > 1 ? boost::accumulators::max(ms.acc_interval_)                   : NAN,
                n_fre,
                n_fre > 0 ? boost::accumulators::mean(ms.acc_frequency_)                 : NAN,
                n_fre > 9 ? std::sqrt(boost::accumulators::variance(ms.acc_frequency_))  : NAN,
                n_fre > 1 ? boost::accumulators::min(ms.acc_frequency_)                  : NAN,
                n_fre > 9 ? boost::accumulators::extended_p_square(ms.acc_frequency_)[0] : NAN,
                n_fre > 9 ? boost::accumulators::extended_p_square(ms.acc_frequency_)[1] : NAN,
                n_fre > 9 ? boost::accumulators::extended_p_square(ms.acc_frequency_)[2] : NAN,
                n_fre > 9 ? boost::accumulators::extended_p_square(ms.acc_frequency_)[3] : NAN,
                n_fre > 1 ? boost::accumulators::max(ms.acc_frequency_)                  : NAN
            );
            // clang-format on
        }
        // clang-format off
        std::printf("+-------------------------------------+---------------+-----------------+----------------------------------------------------------------+----------------------------------------------------------------+----------------------------------------------------------------+\n");
        // clang-format on
    }

    // -----------------------------------------------------------------------------------------------------------------

    template <typename T>
    nlohmann::json HistData(const boost::histogram::histogram<T>& hist)
    {
        using namespace nlohmann;
        const int size = hist.size();                      // Number of bins (incl. underflow and overflow bins)
        const double start = hist.axis(0).bin(0).lower();  // Lower end of the histogram (regular bins)
        const double stop = hist.axis(0).bin(hist.size() - 2).lower();   // Lower end of the histogram (regular bins)
        const double width = hist.axis(0).bin(0).width();                // Bin width
        const int total = std::accumulate(hist.begin(), hist.end(), 0);  // Total count (incl. underfl/overfl)
        const double scale = (total > 0 ? 1.0 / (double)total : 0.0);    // Factor for normalisation
        json hist_values = json::array();
        // Underflow bin (with fake "lower" and "center" for plotting)
        const int under = hist.at(-1);
        double cumul = (double)under * scale;
        hist_values.push_back({ { "lower", start - width }, { "center", start - (width / 2.0) }, { "upper", start },
            { "count", under }, { "prob", (double)under * scale }, { "cumul", cumul } });
        // Regular bins
        for (auto&& x : boost::histogram::indexed(hist, boost::histogram::coverage::inner)) {
            const double prob = (double)*x * scale;
            cumul += prob;
            hist_values.push_back({ { "lower", x.bin().lower() }, { "center", x.bin().center() },
                { "upper", x.bin().upper() }, { "count", (int)*x }, { "prob", prob }, { "cumul", cumul } });
        }
        // Overflow bin (with fake "center" and "uppter" for plotting)
        const int over = hist.at(size - 2);
        cumul += (double)over * scale;
        hist_values.push_back({ { "lower", stop }, { "center", stop + (width / 2.0) }, { "upper", stop + width },
            { "count", over }, { "prob", (double)over * scale }, { "cumul", cumul } });

        return {
            { "size", size },
            { "start", start },
            { "stop", stop },
            { "width", width },
            { "data", hist_values },
        };
    }

    bool SaveStats(const StreamPtr& stream)
    {
        using json = nlohmann::json;

        const auto ps = stream->GetParserStats();
        const auto so = stream->GetOpts();
        const double p_n = (ps.n_msgs_ > 0 ? 100.0 / (double)ps.n_msgs_ : 0.0);
        const double p_s = (ps.s_msgs_ > 0 ? 100.0 / (double)ps.s_msgs_ : 0.0);

        // clang-format off
        json stats ={
            { "parser", {
                { "total",                       { { "n_msgs", ps.n_msgs_   }, { "p_msgs", 100.0                      }, { "n_bytes", ps.s_msgs_   }, { "p_bytes", 100.0                      }, } },
                { ProtocolStr(Protocol::FP_A),   { { "n_msgs", ps.n_fpa_    }, { "p_msgs", (double)ps.n_fpa_    * p_n }, { "n_bytes", ps.s_fpa_    }, { "p_bytes", (double)ps.s_fpa_    * p_s }, } },
                { ProtocolStr(Protocol::FP_B),   { { "n_msgs", ps.n_fpb_    }, { "p_msgs", (double)ps.n_fpb_    * p_n }, { "n_bytes", ps.s_fpb_    }, { "p_bytes", (double)ps.s_fpb_    * p_s }, } },
                { ProtocolStr(Protocol::NMEA),   { { "n_msgs", ps.n_nmea_   }, { "p_msgs", (double)ps.n_nmea_   * p_n }, { "n_bytes", ps.s_nmea_   }, { "p_bytes", (double)ps.s_nmea_   * p_s }, } },
                { ProtocolStr(Protocol::UBX),    { { "n_msgs", ps.n_ubx_    }, { "p_msgs", (double)ps.n_ubx_    * p_n }, { "n_bytes", ps.s_ubx_    }, { "p_bytes", (double)ps.s_ubx_    * p_s }, } },
                { ProtocolStr(Protocol::RTCM3),  { { "n_msgs", ps.n_rtcm3_  }, { "p_msgs", (double)ps.n_rtcm3_  * p_n }, { "n_bytes", ps.s_rtcm3_  }, { "p_bytes", (double)ps.s_rtcm3_  * p_s }, } },
                { ProtocolStr(Protocol::NOV_B),  { { "n_msgs", ps.n_novb_   }, { "p_msgs", (double)ps.n_novb_   * p_n }, { "n_bytes", ps.s_novb_   }, { "p_bytes", (double)ps.s_novb_   * p_s }, } },
                { ProtocolStr(Protocol::UNI_B),  { { "n_msgs", ps.n_unib_   }, { "p_msgs", (double)ps.n_unib_   * p_n }, { "n_bytes", ps.s_unib_   }, { "p_bytes", (double)ps.s_unib_   * p_s }, } },
                { ProtocolStr(Protocol::SPARTN), { { "n_msgs", ps.n_spartn_ }, { "p_msgs", (double)ps.n_spartn_ * p_n }, { "n_bytes", ps.s_spartn_ }, { "p_bytes", (double)ps.s_spartn_ * p_s }, } },
                { ProtocolStr(Protocol::OTHER),  { { "n_msgs", ps.n_other_  }, { "p_msgs", (double)ps.n_other_  * p_n }, { "n_bytes", ps.s_other_  }, { "p_bytes", (double)ps.s_other_  * p_s }, } },
            } },
            { "msgstats", json::array() },
            { "meta", {
                { "t_start", t_start_.StrIsoTime(3) },
                { "t_stop",  t_stop_.StrIsoTime(3) },
                { "t_dur",   (t_stop_ - t_start_).Stringify(3) },
                { "stream",  so.disp_ },
                { "ffxx",  json::object({
                    { "version" , GetVersionString() },
                    { "copyright" , GetCopyrightString() },
                    { "license" , GetLicenseString() }, }) }
            } },
        };
        // clang-format on

        // Message stats
        const std::string pstr0 = Sprintf("p%.0f", Stats::MsgStats::PROB[0] * 1e3);
        const std::string pstr1 = Sprintf("p%.0f", Stats::MsgStats::PROB[1] * 1e3);
        const std::string pstr2 = Sprintf("p%.0f", Stats::MsgStats::PROB[2] * 1e3);
        const std::string pstr3 = Sprintf("p%.0f", Stats::MsgStats::PROB[3] * 1e3);

        // clang-format off
        for (const auto& entry : stats_.msg_stats_) {
            const auto& ms = entry.second;
            const std::size_t n_lat = boost::accumulators::count(ms.acc_latency_);
            const std::size_t n_int = boost::accumulators::count(ms.acc_interval_);
            const std::size_t n_fre = boost::accumulators::count(ms.acc_frequency_);

            // clang-format off
            stats["msgstats"].push_back({
                { "protocol",     ms.protocol_name_        },
                { "message",      ms.message_name_         },
                { "unique_name",  ms.unique_name_          },
                { "desc",         ms.msg_desc_             },
                { "n_msgs",       ms.count_                },
                { "p_msgs",       (double)ms.count_ * p_n  },
                { "n_bytes",      ms.bytes_                },
                { "p_bytes",      (double)ms.bytes_ * p_s  },
                { "latency", {
                    { "N",     n_lat  },
                    { "mean",  (n_lat > 0 ? boost::accumulators::mean(ms.acc_latency_)                   : NAN) },
                    { "std",   (n_lat > 9 ? std::sqrt(boost::accumulators::variance(ms.acc_latency_))    : NAN) },
                    { "min",   (n_lat > 1 ? boost::accumulators::min(ms.acc_latency_)                    : NAN) },
                    { pstr0,   (n_lat > 9 ? boost::accumulators::extended_p_square(ms.acc_latency_)[0]   : NAN) },
                    { pstr1,   (n_lat > 9 ? boost::accumulators::extended_p_square(ms.acc_latency_)[1]   : NAN) },
                    { pstr2,   (n_lat > 9 ? boost::accumulators::extended_p_square(ms.acc_latency_)[2]   : NAN) },
                    { pstr3,   (n_lat > 9 ? boost::accumulators::extended_p_square(ms.acc_latency_)[3]   : NAN) },
                    { "max",   (n_lat > 1 ? boost::accumulators::max(ms.acc_latency_)                    : NAN) },
                    { "hist",  HistData(ms.hist_latency_) },
                }},
                { "interval", {
                    { "N",     n_int  },
                    { "mean",  (n_int > 0 ? boost::accumulators::mean(ms.acc_interval_)                  : NAN) },
                    { "std",   (n_int > 9 ? std::sqrt(boost::accumulators::variance(ms.acc_interval_))   : NAN) },
                    { "min",   (n_int > 1 ? boost::accumulators::min(ms.acc_interval_)                   : NAN) },
                    { pstr0,   (n_int > 9 ? boost::accumulators::extended_p_square(ms.acc_interval_)[0]  : NAN) },
                    { pstr1,   (n_int > 9 ? boost::accumulators::extended_p_square(ms.acc_interval_)[1]  : NAN) },
                    { pstr2,   (n_int > 9 ? boost::accumulators::extended_p_square(ms.acc_interval_)[2]  : NAN) },
                    { pstr3,   (n_int > 9 ? boost::accumulators::extended_p_square(ms.acc_interval_)[3]  : NAN) },
                    { "max",   (n_int > 1 ? boost::accumulators::max(ms.acc_interval_)                   : NAN) },
                    { "hist",  HistData(ms.hist_interval_) },
                }},
                { "frequency", {
                    { "N",     n_fre  },
                    { "mean",  (n_fre > 0 ? boost::accumulators::mean(ms.acc_frequency_)                 : NAN) },
                    { "std",   (n_fre > 9 ? std::sqrt(boost::accumulators::variance(ms.acc_frequency_))  : NAN) },
                    { "min",   (n_fre > 1 ? boost::accumulators::min(ms.acc_frequency_)                  : NAN) },
                    { pstr0,   (n_fre > 9 ? boost::accumulators::extended_p_square(ms.acc_frequency_)[0] : NAN) },
                    { pstr1,   (n_fre > 9 ? boost::accumulators::extended_p_square(ms.acc_frequency_)[1] : NAN) },
                    { pstr2,   (n_fre > 9 ? boost::accumulators::extended_p_square(ms.acc_frequency_)[2] : NAN) },
                    { pstr3,   (n_fre > 9 ? boost::accumulators::extended_p_square(ms.acc_frequency_)[3] : NAN) },
                    { "max",   (n_fre > 1 ? boost::accumulators::max(ms.acc_frequency_)                  : NAN) },
                    { "hist",  HistData(ms.hist_frequency_) },
                }},
            });
            // clang-format on
        }

        INFO("Writing stats to %s", opts_.json_.c_str());
        const auto json_str = stats.dump(LoggingGetParams().level_ >= LoggingLevel::DEBUG ? 4 : -1) + "\n";
        return FileSpew(opts_.json_, { json_str.data(), json_str.data() + json_str.size() });
    }
};

/* ****************************************************************************************************************** */
}  // namespace ffapps::streamtool

/* ****************************************************************************************************************** */

int main(int argc, char** argv)
{
    using namespace ffapps::streamtool;
#ifndef NDEBUG
    fpsdk::common::app::StacktraceHelper stacktrace;
#endif
    bool ok = true;

    // Parse command line arguments
    StreamToolOptions opts;
    if (!opts.LoadFromArgv(argc, argv)) {
        ok = false;
    }

    if (ok) {
        StreamTool app(opts);
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
