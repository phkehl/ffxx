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
 * @brief parserdump main
 */

/* LIBC/STL */
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

/* EXTERNAL */
#include <unistd.h>
#include <nlohmann/json.hpp>

/* Fixposition SDK */
#include <ffxx/epoch.hpp>
#include <ffxx/utils.hpp>
#include <fpsdk_common/app.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/parser.hpp>
#include <fpsdk_common/string.hpp>
#include <fpsdk_common/time.hpp>
#include <fpsdk_common/types.hpp>
#include <fpsdk_common/to_json/parser.hpp>

/* PACKAGE */

namespace ffapps::parserdump {
/* ****************************************************************************************************************** */

using namespace ffxx;
using namespace fpsdk::common::app;
using namespace fpsdk::common::parser;
using namespace fpsdk::common::string;
using namespace fpsdk::common::time;
using namespace fpsdk::common::types;

// ---------------------------------------------------------------------------------------------------------------------

// Program options
class ParserDumpOptions : public ProgramOptions
{
   public:
    ParserDumpOptions()  // clang-format off
        : ProgramOptions("parsertool", {
            { 'x', false, "hexdump" }, { 's', false, "save" }, { 'f', true, "filter" }, { 'c', false, "stdout" },
            { 'e', false, "epoch" }, { 'j', false, "jsonl" }
        })  // clang-format on
    {
        version_str_ = ffxx::GetVersionString();
        copy_str_ = ffxx::GetCopyrightString();
        lic_str_ = ffxx::GetLicenseString();
    }

    // clang-format off
    bool                      hexdump_ = false;  //!< See help screen below
    bool                      save_    = false;  //!< See help screen below
    std::vector<std::string>  inputs_;           //!< See help screen below
    std::vector<std::string>  filters_;          //!< See help screen below
    bool                      stdout_  = false;  //!< See help screen below
    bool                      jsonl_ = false;    //!< See help screen below
    bool                      epochDetect_ = false;   //!< See help screen below
    EpochCollector::Mode      epochMode_   = EpochCollector::Mode::RECEIVER;  //!< See help screen below
    // clang-format on

    void PrintHelp() override final
    {
        // clang-format off
        std::fputs(
            "\n"
            "This parses input data from stdin or file(s) into individual messages (frames) and\n"
            "dumps some information about them to stdout.\n"
            "\n"
            "Usage:\n"
            "\n"
            "    parserdump [flags] [<input> ...]\n"
            "\n"
            "Where:\n"
            "\n", stdout);
        std::fputs(COMMON_FLAGS_HELP, stdout);
        std::fputs(
            "\n"
            "    -x, --hexdump  -- Print hexdump of each message, not with -c\n"
            "    -s, --save     -- Save each (!) message into a separate (!) file in the current directory, not with -cj\n"
            "    -f <filter>, --filter <filter>\n"
            "                   -- Filter input, where <filter> is a comma-separated list of full or partial message.\n"
            "                      names. Multiple -f can be given.\n"
            "    -c, --stdout   -- Write messages to stdout instead of printing info, useful with -f, not with -j\n"
            "    -j, --jsonl    -- Write messages as JSONL to stdout instead of printing info, not with -c\n"
            "    -e, --epoch    -- Detect and dump epochs, multiple -e to change detect/collect method, not with -cj\n"
            "    <input>        -- File or device to read data from (instead of stdin)\n"
            "\n"
            "\n", stdout);
        // clang-format on
    }

    bool HandleOption(const Option& option, const std::string& argument) final
    {
        bool ok = true;
        switch (option.flag) {
            case 'x':
                hexdump_ = true;
                break;
            case 's':
                save_ = true;
                break;
            case 'f':
                for (auto& f : StrSplit(argument, ",")) {
                    filters_.push_back(f);
                }
                break;
            case 'c':
                stdout_ = true;
                break;
            case 'j':
                jsonl_ = true;
                break;
            case 'e':
                if (!epochDetect_) {
                    epochDetect_ = true;
                } else {
                    switch (epochMode_) {  // clang-format off
                        case EpochCollector::Mode::RECEIVER:    epochMode_ = EpochCollector::Mode::CORRECTIONS; break;
                        case EpochCollector::Mode::CORRECTIONS: epochMode_ = EpochCollector::Mode::RECEIVER;    break;
                    }  // clang-format on
                }
                break;
            default:
                ok = false;
                break;
        }
        return ok;
    }

    bool CheckOptions(const std::vector<std::string>& args) final
    {
        bool ok = true;

        // Any further positional arguments are inputs
        inputs_ = args;

        for (std::size_t ix = 0; ix < inputs_.size(); ix++) {
            DEBUG("inputs_[%" PRIuMAX "] = '%s'", ix, inputs_[ix].c_str());
        }
        DEBUG("hexdump    = %s", ToStr(hexdump_));
        DEBUG("save       = %s", ToStr(save_));
        for (std::size_t ix = 0; ix < filters_.size(); ix++) {
            DEBUG("filters[%" PRIuMAX "] = %s", ix, filters_[ix].c_str());
        }
        DEBUG("stdout     = %s", ToStr(stdout_));
        DEBUG("jsonl      = %s", ToStr(jsonl_));
        DEBUG("epochDetect/Mode = %s %d", ToStr(epochDetect_), EnumToVal(epochMode_));

        if (stdout_ && jsonl_) {
            WARNING("Cannot do --stdout and --jsonl at the same time");
            ok = false;
        }

        if (epochDetect_ && jsonl_) {
            WARNING("Cannot do --epoch and --jsonl at the same time");
            ok = false;
        }

        if (hexdump_ && jsonl_) {
            WARNING("Cannot do --hexdump and --jsonl at the same time");
            ok = false;
        }

        if (save_ && (stdout_ || jsonl_)) {
            WARNING("Cannot do --save and --stdout or --jsonlat the same time");
            ok = false;
        }

        return ok;
    }
};

/* ****************************************************************************************************************** */

class ParserDump
{
   public:
    ParserDump(const ParserDumpOptions& opts) : opts_{ opts }, offs_{ 0 }
    {
    }

    bool Run()
    {
        bool ok = true;

        // Print header
        if (!opts_.stdout_) {
            PrintMessageHeader();
        }

        // Process data from stdin
        if (opts_.inputs_.empty()) {
            INFO("Reading from stdin");
            std::ios_base::sync_with_stdio(false);
            ProcessInput(std::cin);
        }

        // Process all input files
        for (const auto& input_file : opts_.inputs_) {
            INFO("Reading from %s", input_file.c_str());
            std::ifstream input(input_file, std::ios::binary);
            if (input.good()) {
                ProcessInput(input);
            } else {
                WARNING("Failed reading from %s: %s", input_file.c_str(), StrError(errno).c_str());
                ok = false;
            }
            if (sigint_.ShouldAbort() || !ok) {
                break;
            }
        }

        //! Print statistics
        if (!opts_.stdout_) {
            PrintParserStats();
        }

        return ok;
    }

   private:
    // -----------------------------------------------------------------------------------------------------------------

    ParserDumpOptions opts_;  //!< Program options
    SigIntHelper sigint_;     //!< Handle SIGINT (C-c) to abort nicely
    Parser parser_;           //!< Parser
    std::size_t offs_;        //!< Offset of message in input data
    ParserStats stats_;       //!< Parser statistics

    // -----------------------------------------------------------------------------------------------------------------

    void ProcessInput(std::istream& input)
    {
        // Run all data through the parser and print what it finds...

        ParserMsg msg;
        const auto tenms = Duration::FromNSec(10000000);

        EpochCollector coll(opts_.epochMode_);

        while (!sigint_.ShouldAbort() && input.good() && !input.eof()) {
            // Read a chunk of data from the logfile
            uint8_t data[MAX_ADD_SIZE];
            const int size = input.readsome((char*)data, sizeof(data));

            // We may be at end of file
            if (size <= 0) {
                // End of file
                if (input.peek() == EOF) {
                    break;
                }
                // More data may be available later (e.g. when reading from stdin, pipe, device)
                // @todo Use select()
                else {
                    tenms.Sleep();  // @todo necessary?
                    continue;
                }
            }

            // Add the chunk of data to the parser
            if (!parser_.Add(data, size)) {
                WARNING("Parser overflow");
                parser_.Reset();
                continue;
            }

            // Run parser and print messages to the screen
            while (parser_.Process(msg)) {
                OnMessage(msg, coll);
            }
        }

        // There may be some remaining data in the parser
        while (parser_.Flush(msg)) {
            OnMessage(msg, coll);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------

    void OnMessage(const ParserMsg& msg, EpochCollector& coll)
    {
        EpochPtr epoch;
        bool msgPartOfEpoch = false;
        if (opts_.epochDetect_) {
            epoch = coll.Collect(msg, &msgPartOfEpoch);
        }
        if (msgPartOfEpoch) {
            PrintMessageData(msg, offs_, opts_.hexdump_);
        }
        if (epoch) {
            PrintEpochData(*epoch);
        }
        if (!msgPartOfEpoch) {
            PrintMessageData(msg, offs_, opts_.hexdump_);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------

    void SaveMessage(const ParserMsg& msg)
    {
        const auto output_file = Sprintf("%06" PRIuMAX "_%s.bin", msg.seq_, msg.name_.c_str());
        std::ofstream output(output_file, std::ios::binary);
        output.write((const char*)msg.Data(), msg.Size());
        output.close();
    }

    // -----------------------------------------------------------------------------------------------------------------

    void PrintMessageHeader()
    {
        // Keep in sync with PrintMessageData()
        std::fprintf(opts_.jsonl_ ? stderr : stdout,
            "------- Seq#     Offset  Size Protocol Message                        Info\n");
    }

    // -----------------------------------------------------------------------------------------------------------------

    void PrintMessageData(const ParserMsg& msg, const std::size_t offs, const bool hexdump)
    {
        if (!CheckFilter(msg)) {
            return;
        }

        if (opts_.stdout_) {
            std::fwrite(msg.Data(), msg.Size(), 1, stdout);
        } else if (opts_.jsonl_) {
            msg.MakeInfo();
            std::printf("%s\n", ParserMsgToJson(msg).dump().c_str());
        } else {
            msg.MakeInfo();
            std::printf("message %08" PRIuMAX " %8" PRIuMAX " %5" PRIuMAX " %-8s %-30s %s\n", msg.seq_, offs,
                msg.Size(), ProtocolStr(msg.proto_), msg.name_.c_str(), msg.info_.empty() ? "-" : msg.info_.c_str());
            if (hexdump) {
                for (auto& line : HexDump(msg.data_)) {
                    std::printf("%s\n", line.c_str());
                }
            }
            if (opts_.save_) {
                SaveMessage(msg);
            }
        }

        stats_.Update(msg);
        offs_ += msg.Size();
    }

    // -----------------------------------------------------------------------------------------------------------------

    void PrintEpochData(const Epoch& epoch)
    {
        std::printf("epoch   %08" PRIuMAX "        -     - -        EPOCH                          %s\n",
            epoch.seq, epoch.str);
    }

    // -----------------------------------------------------------------------------------------------------------------

    void PrintParserStats()
    {
        const auto& s = stats_;
        const double p_n = (s.n_msgs_ > 0 ? 100.0 / (double)s.n_msgs_ : 0.0);
        const double p_s = (s.s_msgs_ > 0 ? 100.0 / (double)s.s_msgs_ : 0.0);
        FILE* f = (opts_.jsonl_ ? stderr : stdout);
        std::fprintf(f, "Stats:     Messages               Bytes\n");
        const char* fmt = "%-8s %10" PRIu64 " (%5.1f%%) %10" PRIu64 " (%5.1f%%)\n";
        // clang-format off
        std::fprintf(f, fmt, "Total",                              s.n_msgs_,   (double)s.n_msgs_   * p_n, s.s_msgs_,   (double)s.s_msgs_   * p_s);
        std::fprintf(f, fmt, ProtocolStr(Protocol::FP_A),   s.n_fpa_,    (double)s.n_fpa_    * p_n, s.s_fpa_,    (double)s.s_fpa_    * p_s);
        std::fprintf(f, fmt, ProtocolStr(Protocol::FP_B),   s.n_fpb_,    (double)s.n_fpb_    * p_n, s.s_fpb_,    (double)s.s_fpb_    * p_s);
        std::fprintf(f, fmt, ProtocolStr(Protocol::NMEA),   s.n_nmea_,   (double)s.n_nmea_   * p_n, s.s_nmea_,   (double)s.s_nmea_   * p_s);
        std::fprintf(f, fmt, ProtocolStr(Protocol::UBX),    s.n_ubx_,    (double)s.n_ubx_    * p_n, s.s_ubx_,    (double)s.s_ubx_    * p_s);
        std::fprintf(f, fmt, ProtocolStr(Protocol::RTCM3),  s.n_rtcm3_,  (double)s.n_rtcm3_  * p_n, s.s_rtcm3_,  (double)s.s_rtcm3_  * p_s);
        std::fprintf(f, fmt, ProtocolStr(Protocol::NOV_B),  s.n_novb_,   (double)s.n_novb_   * p_n, s.s_novb_,   (double)s.s_novb_   * p_s);
        std::fprintf(f, fmt, ProtocolStr(Protocol::UNI_B),  s.n_unib_,   (double)s.n_unib_   * p_n, s.s_unib_,   (double)s.s_unib_   * p_s);
        std::fprintf(f, fmt, ProtocolStr(Protocol::SBF),    s.n_sbf_,    (double)s.n_sbf_ *    p_n, s.s_sbf_,    (double)s.s_sbf_    * p_s);
        std::fprintf(f, fmt, ProtocolStr(Protocol::QGC),    s.n_qgc_,    (double)s.n_qgc_ *    p_n, s.s_qgc_,    (double)s.s_qgc_    * p_s);
        std::fprintf(f, fmt, ProtocolStr(Protocol::SPARTN), s.n_spartn_, (double)s.n_spartn_ * p_n, s.s_spartn_, (double)s.s_spartn_ * p_s);
        std::fprintf(f, fmt, ProtocolStr(Protocol::OTHER),  s.n_other_,  (double)s.n_other_  * p_n, s.s_other_,  (double)s.s_other_  * p_s);
        // clang-format on
    }

    // -----------------------------------------------------------------------------------------------------------------

    bool CheckFilter(const ParserMsg& msg)
    {
        if (opts_.filters_.empty()) {
            return true;
        }
        for (const auto& f : opts_.filters_) {
            if (StrStartsWith(msg.name_, f)) {
                return true;
            }
        }
        return false;
    }
};

/* ****************************************************************************************************************** */
}  // namespace ffapps::parserdump


/* ****************************************************************************************************************** */

int main(int argc, char** argv)
{
    using namespace ffapps::parserdump;
#ifndef NDEBUG
    fpsdk::common::app::StacktraceHelper stacktrace;
#endif
    bool ok = true;

    // Parse command line arguments
    ParserDumpOptions opts;
    if (!opts.LoadFromArgv(argc, argv)) {
        ok = false;
    }

    if (ok) {
        ParserDump app(opts);
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
