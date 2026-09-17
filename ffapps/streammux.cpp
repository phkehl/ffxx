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

/* EXTERNAL */

/* PACKAGE */
#include "streammux_utils.hpp"

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

// ---------------------------------------------------------------------------------------------------------------------

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
    std::vector<SmMonPtr> mons_;
    Thread status_thread_;
    SmProc proc_;
    std::mutex status_mutex_;
    HttpApiServer::Response status_res_;
    std::unique_ptr<HttpApiServer> api_;

    SmStrPtr FindStr(const std::string& name_or_nr);
    SmMuxPtr FindMux(const std::string& name_or_nr);
    SmMonPtr FindMon(const std::string& name_or_nr);
    bool InitApi();
    bool ApiHandler(const HttpApiServer::Request& req, HttpApiServer::Response& res);
    bool ApiHandlerCtrl(const json& data, HttpApiServer::Response& res);
    bool StatusWorker();
};

// ---------------------------------------------------------------------------------------------------------------------

bool StreamMux::Run()
{
    bool ok = true;

    NOTICE("flipflip's StreamMux (version %s)", GetVersionString());
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
        if (!str->filter_read_.Set(ConsumeOption(spec, "FR"))) {
            WARNING("Bad FR option value in stream %" PRIuMAX, strs_.size() + 1);
            sok = false;
        }
        if (!str->filter_write_.Set(ConsumeOption(spec, "FW"))) {
            WARNING("Bad FW option value in stream %" PRIuMAX, strs_.size() + 1);
            sok = false;
        }

        // The rest is the stream spec
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
            str->ena_read_ ? "on" : "off", str->ena_write_ ? "on" : "off", str->filter_read_.Size(),
            str->filter_write_.Size());

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
        if (!CheckName(mux->name_)) {
            WARNING("Bad mux name '%s'", mux->name_.c_str());
            mok = false;
        }
        else if (FindStr(mux->name_) || FindMux(mux->name_)) {
            WARNING("Duplicate mux or stream name '%s'", mux->name_.c_str());
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
        if (!mux->filter_fwd_.Set(ConsumeOption(spec, "FF"))) {
            WARNING("Bad FW option value in mux %" PRIuMAX, muxs_.size() + 1);
            mok = false;
        }
        if (!mux->filter_rev_.Set(ConsumeOption(spec, "FR"))) {
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
            mux->ena_rev_ ? "on" : "off", mux->filter_fwd_.Size(), mux->filter_rev_.Size());

        muxs_.push_back(mux);
    }

    if (!ok) {
        return false;
    }

    // Setup monitors
    for (const std::string& spec : opts_.monitors_)
    {
        const auto parts = StrSplit(spec, ",", 2);
        if (parts.empty() || parts[0].empty()) {
            WARNING("Bad monitor spec '%s'", spec.c_str());
            ok = false;
            continue;
        }

        auto mon = std::make_shared<SmMon>();

        // Source stream
        mon->src_ = FindStr(parts[0]);
        if (!mon->src_) {
            WARNING("Bad monitor spec '%s': no stream '%s' found", spec.c_str(), parts[0].c_str());
            ok = false;
            continue;
        }

        bool mok = true;
        std::string opts = (parts.size() > 1 ? parts[1] : "");

        // N=...
        mon->name_ = ConsumeOption(opts, "N");
        if (mon->name_.empty()) {
            mon->name_ = "mon" + std::to_string(mons_.size() + 1);
        }
        if (!CheckName(mon->name_)) {
            WARNING("Bad monitor name '%s'", mon->name_.c_str());
            mok = false;
        } else if (FindStr(mon->name_) || FindMux(mon->name_) || FindMon(mon->name_)) {
            WARNING("Duplicate stream, mux or monitor name '%s'", mon->name_.c_str());
            mok = false;
        }

        // W=...
        const auto winSizes = StrSplit(ConsumeOption(opts, "W", "5.0/0.0/0.0"), "/", mon->winSizes_.size());
        for (std::size_t ix = 0; ix < std::min(mon->winSizes_.size(),winSizes.size()); ix++) {
            double win = 0.0;
            if (!StrToValue(winSizes[ix], win) || !mon->winSizes_[ix].SetSec(win) ||
                // First must be 5..300
                ((ix == 0) && ((win < 5.0) || (win > 300.0))) ||
                // Others can be 0, too, but must be larger than previous one
                ((ix > 0) && (win != 0) && ((win < 5.0) || (win > 300.0) || (win < mon->winSizes_[ix - 1].GetSec()))))
            {
                WARNING("Bad W option value '%s' in monitor %" PRIuMAX, winSizes[ix].c_str(), strs_.size() + 1);
                mok = false;
            }
        }

        // E=...
        const std::string em = ConsumeOption(opts, "E", "receiver");
        if ((em == "1") || (em == "receiver")) {
            mon->coll_ = std::make_unique<EpochCollector>(EpochCollector::Mode::RECEIVER);
        } else if ((em == "2") || (em == "corrections")) {
            mon->coll_ = std::make_unique<EpochCollector>(EpochCollector::Mode::CORRECTIONS);
        } else {
            WARNING("Bad E option value in monitor %" PRIuMAX, mons_.size() + 1);
            mok = false;
        }

        if (!opts.empty()) {
            WARNING("Spurious options '%s' in monitor %" PRIuMAX, opts.c_str(), strs_.size() + 1);
            mok = false;
        }

        if (!mok) {
            ok = false;
            continue;
        }

        INFO("Monitor(%s) %s W=%.1f/%.1f/%.1f E=%s", mon->name_.c_str(), mon->src_->name_.c_str(),
            mon->winSizes_[0].GetSec(), mon->winSizes_[1].GetSec(), mon->winSizes_[2].GetSec(),
            mon->coll_->GetMode() == EpochCollector::Mode::RECEIVER ? "receiver" : "corrections");

        mons_.push_back(mon);
        mon->src_->mons_.push_back(mon);
    }

    // Check that all streams are used by at least one mon or mux
    for (const auto& str : strs_) {
        bool used = false;
        // Used by mux?
        for (const auto& mux : muxs_) {
            if ((mux->src_ == str) || (mux->dst_ == str)) {
                used = true;
                break;
            }
        }
        // Used by mon?
        for (const auto& mon : mons_) {
            if (mon->src_ == str) {
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
    const bool use_status_thread = (api_ || !opts_.report_path_.empty());
    if (use_status_thread) {
        if ((!opts_.report_path_.empty() && !FileSpew(opts_.report_path_, {})) || !status_thread_.Start()) {
            ok = false;
        }
    }
    if (!ok) {
        return false;
    }

    // Generate a name if none provided
    if (opts_.name_.empty()) {
        // Hash all names and base26 encode that
        std::size_t hash = 0;
        for (const auto& str : strs_) { hash ^= std::hash<std::string>{}(str->name_); }
        for (const auto& mux : muxs_) { hash ^= std::hash<std::string>{}(mux->name_); }
        while (hash) { proc_.name_ += 'A' + (hash % 26); hash /= 26; }
    } else {
        proc_.name_ = opts_.name_;
    }


    SigIntHelper sigint;
    Time wd_time_ = Time::FromClockRealtime();
    const Duration wd_int_ = Duration::FromSec(5.0);

    // Observe streams for data available to read and state changes
    BinarySemaphore sem;
    for (auto& str : strs_) {
        str->stream_->AddReadObserver([&str, &sem]() { sem.Notify(); });
        str->stream_->AddStateObserver([&str, &ok, &sigint](const StreamState old_state, const StreamState new_state,
                                           const StreamError error, const std::string& info) {
            // To know if we can Read() on the stream
            str->connected_ = (new_state == StreamState::CONNECTED);

            // Save time of state change
            if (old_state != new_state) {
                str->state_ts_.SetClockRealtime();
            }

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


    if (opts_.systemd_) {
        SystemdNotify("READY=1");
    }
    NOTICE("Running... (%s)", proc_.name_.c_str());

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
            while (str->can_read_ && str->connected_ && (n_msgs++ < max_msgs_per_str) &&
                   /* and if that's all okay, only then read the next message */ str->stream_->Read(msg)) {
                //
                // Discard input if read isn't enabled
                if (!str->ena_read_) {
                    continue;
                }

                // Skip message if it doesn't pass the read (input) filter
                if (!str->filter_read_.Pass(msg)) {
                    str->stats_read_.n_filt_++;
                    str->stats_read_.s_filt_ += msg.data_.size();
                    continue;
                }

                // TRACE("%s < %s", str->name_.c_str(), msg.name_.c_str());

                // Update read (input) statistics
                str->stats_read_.Update(msg);
                for (auto& mon : str->mons_) {
                    mon->Add(msg);
                }

                // Forward messages through the connected muxes
                for (auto& mux : muxs_) {
                    auto& src = mux->src_;
                    auto& dst = mux->dst_;

                    // Forward (src->dst)
                    if (mux->can_fwd_ && mux->ena_fwd_ && (src == str)) {
                        if (mux->filter_fwd_.Pass(msg)) {
                            if (dst->can_write_ && dst->ena_write_ && dst->connected_) {
                                if (dst->filter_write_.Pass(msg)) {
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
                        if (mux->filter_rev_.Pass(msg)) {
                            if (dst->can_write_ && src->ena_write_ && src->connected_) {
                                if (src->filter_write_.Pass(msg)) {
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
            // Sleep
            sem.WaitFor(1000);

            // Check status thread, trigger watchdog
            const auto now = Time::FromClockRealtime();
            if ((now - wd_time_) > wd_int_) {
                if (opts_.systemd_) {
                    SystemdNotify("WATCHDOG=1");
                }
                if (use_status_thread && (status_thread_.GetStatus() != Thread::Status::RUNNING)) {
                    WARNING("status thread fail");
                    ok = false;
                }
                wd_time_ = now;
            }
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
    const auto stream = std::find_if(strs_.begin(), strs_.end(), [name_or_nr](const auto& cand) {
            return StrToLower(cand->name_) == StrToLower(name_or_nr); });
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
    const auto stream = std::find_if(muxs_.begin(), muxs_.end(), [name_or_nr](const auto& cand) {
        return StrToLower(cand->name_) == StrToLower(name_or_nr); });
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

SmMonPtr StreamMux::FindMon(const std::string& name_or_nr)
{
    const auto monitor = std::find_if(mons_.begin(), mons_.end(), [name_or_nr](const auto& cand) {
        return StrToLower(cand->name_) == StrToLower(name_or_nr); });
    std::size_t nr = 0;
    if (monitor != mons_.end()) {
        return *monitor;
    } else if (StrToValue(name_or_nr, nr) && (nr > 0) && (nr <= mons_.size())) {
        return mons_[nr - 1];
    } else {
        return nullptr;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamMux::StatusWorker()
{
    bool ok = true;
    const std::string report_path_tmp = opts_.report_path_ + ".tmp";
    bool report_ok = !opts_.report_path_.empty();
    while (ok && !status_thread_.ShouldAbort()) {
        // - Process info
        proc_.Update();
        // - Streams
        json strs = json::array();
        for (const auto& str : strs_) {
            strs.push_back(str->ToJson());
        }
        // - Muxes
        json muxs = json::array();
        for (const auto& mux : muxs_) {
            muxs.push_back(mux->ToJson());
        }
        // - Monitors
        json mons = json::array();
        for (const auto& mon : mons_) {
            mons.push_back(mon->ToJson());
        }
        // - And finally the entire status data is:
        json status = json::object({
            { "api", "status" },
            { "proc", proc_.ToJson() },
            { "strs", strs },
            { "muxs", muxs },
            { "mons", mons },
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
                    ok = false;
                }
            } else {
                ok = false;
            }
        }

        status_thread_.SleepUntil(STATUS_PERIOD);
    }

    // Cleanup
    if (!opts_.report_path_.empty()) {
        std::error_code ec;
        std::filesystem::remove(report_path_tmp, ec);
        std::filesystem::remove(opts_.report_path_, ec);
    }

    if (!ok) {
        WARNING("StatusWorker fail");
    }

    return ok;
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
