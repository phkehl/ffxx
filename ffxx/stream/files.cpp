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
 * @brief Stream file read and write
 */

/* LIBC/STL */
#include <cmath>

/* EXTERNAL */
#include <fcntl.h>
#include <sys/stat.h>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/path.hpp>
#include <fpsdk_common/string.hpp>

/* PACKAGE */
#include "files.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

/*static*/ std::unique_ptr<StreamOptsFileout> StreamOptsFileout::FromPath(
    const std::string& path, std::vector<std::string>& errors)
{
    StreamOptsFileout opts;
    bool ok = true;

    // "<file>[:<swap>[:<ts>]]"
    const auto parts = string::StrSplit(path, ":");
    if ((parts.size() >= 1) && (parts.size() <= 3)) {
        if (!parts[0].empty()) {
            opts.templ_ = parts[0];
        } else {
            ok = false;
            errors.push_back("bad <file>");
        }
        if (parts.size() >= 2) {
            int swap = 0;
            if (!parts[1].empty() && (!string::StrToValue(parts[1], swap) || (std::abs(swap) < SWAP_MIN) ||
                                         (std::abs(swap) > SWAP_MAX) || !opts.swap_.SetSec(swap))) {
                ok = false;
                errors.push_back("bad <swap>");
            } else {
                opts.align_ = (swap >= 0);
            }
        }
        if (parts.size() >= 3) {
            if (parts[2] == "ts") {
                opts.ts_ = true;
            } else if (parts[2].empty()) {
                opts.ts_ = false;
            } else {
                ok = false;
                errors.push_back("bad <ts>");
            }
        }
    } else {
        ok = false;
    }

    const int swap = std::floor(opts.swap_.GetSec(0)) * (opts.align_ ? 1 : -1);
    opts.path_ = /* clang-format off */ swap == 0 ?
        string::Sprintf("%s::%s", opts.templ_.c_str(), opts.ts_ ? "ts" : "") :
        string::Sprintf("%s:%d:%s", opts.templ_.c_str(), swap, opts.ts_ ? "ts" : "");  // clang-format on

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsFileout>(opts);
}

/* ****************************************************************************************************************** */

StreamFileout::StreamFileout(const StreamOptsFileout& opts) /* clang-format off */ :
    StreamBase(opts, opts_),
    opts_      { opts },
    data_fd_   { -1 },
    ts_fd_     { -1 },
    thread_    { opts_.name_, std::bind(&StreamFileout::Worker, this) }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();
}

StreamFileout::~StreamFileout()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamFileout::Start()
{
    STREAM_TRACE("Start");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }

    SetStateConnecting();

    if (!OpenFile()) {
        Stop();
        return false;
    }

    if (!thread_.Start()) {
        return false;
    }

    return true;
}

void StreamFileout::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    if (!data_fn_.empty()) {
        StopWaitTxDone(timeout);
        CloseFile();
    }
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamFileout::OpenFile()
{
    const std::string data_fn = MakeFilePath();
    STREAM_TRACE("OpenFile %s", data_fn.c_str());
    opts_.disp_ = data_fn;
    int data_fd = open(data_fn.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (data_fd < 0) {
        SetStateError(StreamError::DEVICE_FAIL, "open: " + string::StrError(errno));
        return false;
    }
    data_fd_ = data_fd;
    data_fn_ = data_fn;
    data_t0_.SetClockTai();

    if (opts_.ts_) {
        const std::string ts_fn = data_fn + ".ts";
        int ts_fd = open(ts_fn.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (ts_fd < 0) {
            SetStateError(StreamError::DEVICE_FAIL, "open: " + string::StrError(errno));
            return false;
        }
        ts_fd_ = ts_fd;
        ts_fn_ = ts_fn;

        ts_rec_.offs_ = 0;
    }

    SetStateConnected();

    return true;
}

void StreamFileout::CloseFile()
{
    STREAM_TRACE("CloseFile %d %d", data_fd_, ts_fd_);
    opts_.disp_ = opts_.path_;

    if ((data_fd_ >= 0) && (close(data_fd_) != 0)) {
        STREAM_WARNING("close fail: %s", string::StrError(errno).c_str());
    }
    data_fd_ = -1;
    data_fn_.clear();

    if ((ts_fd_ >= 0) && (close(ts_fd_) != 0)) {
        STREAM_WARNING("close fail: %s", string::StrError(errno).c_str());
    }
    ts_fd_ = -1;
    ts_fn_.clear();

    return;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamFileout::Worker()
{
    STREAM_TRACE("Worker");
    bool ok = true;
    time::Time t_rec;
    const time::Duration dt_min = time::Duration::FromNSec(10000000);  // 10ms
    bool swap_file = false;
    bool write_ts = opts_.ts_;
    ts_rec_.time_.SetClockTai();
    while ((!thread_.ShouldAbort() || tx_ongoing_) && ok) {
        // Check if there is new data
        std::size_t chunk_size = 0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            chunk_size = std::min(write_queue_.Used(), sizeof(chunk_buf));
            write_queue_.Read(chunk_buf, chunk_size);
        }

        // Write data. We'll run data through a parser so that we can write, timetag and split at message boundaries
        if (chunk_size > 0) {
            tx_ongoing_ = true;
            t_rec.SetClockTai();
            swap_file = (!opts_.swap_.IsZero() && ((t_rec - data_t0_) > opts_.swap_));

            if (!parser_.Add(chunk_buf, chunk_size)) {
                STREAM_WARNING_THR(1000, "parser ovfl");  // we don't expect this ever
                parser_.Reset();
                parser_.Add(chunk_buf, chunk_size);
            }

            parser::ParserMsg msg;
            std::size_t size = 0;
            while (parser_.Process(msg)) {
                STREAM_TRACE("Worker write %" PRIuMAX " %s", msg.data_.size(), msg.name_.c_str());
                if (write(data_fd_, msg.Data(), msg.Size()) != (int)msg.Size()) {
                    SetStateError(StreamError::DEVICE_FAIL, "write: " + string::StrError(errno));
                    ok = false;
                    break;
                }
                size += msg.data_.size();
            }
            ts_rec_.offs_ += size;

            if (opts_.ts_ && (swap_file || ((size > 0) && ((t_rec - ts_rec_.time_) >= dt_min)))) {
                ts_rec_.time_ = t_rec;
                write_ts = true;
            }
        }

        if (write_ts) {
            STREAM_TRACE("Worker ts %s %" PRIuMAX, ts_rec_.time_.StrUtcTime().c_str(), ts_rec_.offs_);
            if (write(ts_fd_, &ts_rec_, sizeof(ts_rec_)) != (int)sizeof(ts_rec_)) {
                SetStateError(StreamError::DEVICE_FAIL, "write: " + string::StrError(errno));
                ok = false;
                break;
            }
            write_ts = false;
        }

        if (swap_file) {
            STREAM_TRACE("swap file");
            CloseFile();
            if (!OpenFile()) {
                ok = false;
                break;
            }
            write_ts = true;
            ts_rec_.time_.SetClockTai();
            swap_file = false;
        }

        // No data to write
        if (chunk_size == 0) {
            tx_ongoing_ = false;
            thread_.Sleep(337);
        }
    }
    CloseFile();
    SetStateClosed();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

std::string StreamFileout::MakeFilePath()
{
    auto now = time::Time::FromClockTai();
    if (opts_.align_ && !opts_.swap_.IsZero()) {
        const double period = opts_.swap_.GetSec();
        const double sec = now.GetRosTime().sec_;  // Use POSIX sec (~ UTC)
        now.SetRosTime({ static_cast<uint32_t>(std::floor(sec / period) * period), 0 });
    }
    // DEBUG("now=%s gps=%s", now.StrUtcTime().c_str(), now.StrWnoTow().c_str());

    const auto utc = now.GetUtcTime(0);
    const auto gps = now.GetWnoTow(time::WnoTow::Sys::GPS, 0);
    std::string file_path = opts_.templ_;
    string::StrReplace(file_path, "%Y", string::Sprintf("%04d", utc.year_));
    string::StrReplace(file_path, "%m", string::Sprintf("%02d", utc.month_));
    string::StrReplace(file_path, "%d", string::Sprintf("%02d", utc.day_));
    string::StrReplace(file_path, "%h", string::Sprintf("%02d", utc.hour_));
    string::StrReplace(file_path, "%M", string::Sprintf("%02d", utc.min_));
    string::StrReplace(file_path, "%S", string::Sprintf("%02.0f", utc.sec_));
    string::StrReplace(file_path, "%j", string::Sprintf("%03.0f", std::floor(now.GetDayOfYear())));
    string::StrReplace(file_path, "%W", string::Sprintf("%04d", gps.wno_));
    string::StrReplace(file_path, "%w", string::Sprintf("%d", static_cast<int>(gps.tow_) / time::SEC_IN_WEEK_I));
    string::StrReplace(file_path, "%s", string::Sprintf("%06.0f", gps.tow_));
    return file_path;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamFileout::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_ongoing_));
    UNUSED(size);
    tx_ongoing_ = true;
    thread_.Wakeup();
    return true;
}

/* ****************************************************************************************************************** */

/*static*/ std::unique_ptr<StreamOptsFilein> StreamOptsFilein::FromPath(
    const std::string& path, std::vector<std::string>& errors)
{
    StreamOptsFilein opts;

    bool ok = true;

    // "<file>[:<speed>[:<offset>]]"
    const auto parts = string::StrSplit(path, ":");
    if ((parts.size() >= 1) && (parts.size() <= 3)) {
        if (!parts[0].empty()) {
            opts.file_ = parts[0];
        } else {
            ok = false;
            errors.push_back("bad <file>");
        }
        if (parts.size() >= 2) {
            if (parts[1].empty()) {
                opts.speed_ = 0.0;
            } else if (!string::StrToValue(parts[1], opts.speed_) ||
                       !((opts.speed_ == 0.0) || !((opts.speed_ < opts.SPEED_MIN) || (opts.speed_ > opts.SPEED_MAX)))) {
                ok = false;
                errors.push_back("bad <speed>");
            }
        }
        if (parts.size() >= 3) {
            if (!parts[2].empty() && (!string::StrToValue(parts[2], opts.offs_) || (opts.offs_ < 0.0))) {
                ok = false;
                errors.push_back("bad <offset>");
            }
        }
    } else {
        ok = false;
    }

    opts.path_ = opts.file_;
    if (opts.speed_ > 0.0) {
        opts.path_ += string::Sprintf(":%.*f:%.*f",  // clang-format off
        (opts.speed_ > 0.0) && (opts.speed_ != 1.0) ? 1 : 0, opts.speed_,
        (opts.offs_ > 0.0) && (opts.offs_ < 0.01) ? 3 : (
        (opts.offs_ > 0.0) && (opts.offs_ < 0.1 ) ? 2 : (
        (opts.offs_ > 0.0) && (opts.offs_ < 1.0 ) ? 1 : 0)), opts.offs_);  // clang-format on
    }

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsFilein>(opts);
}

/* ****************************************************************************************************************** */

StreamFilein::StreamFilein(const StreamOptsFilein& opts) /* clang-format off */ :
    StreamBase(opts, opts_),
    opts_          { opts },
    thread_        { opts_.name_, std::bind(&StreamFilein::Worker, this) }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();
}

StreamFilein::~StreamFilein()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamFilein::Start()
{
    STREAM_TRACE("Start");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }

    SetStateConnecting();

    if (!OpenFile()) {
        Stop();
        return false;
    }

    if (!thread_.Start()) {
        return false;
    }

    return true;
}

void StreamFilein::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    UNUSED(timeout);
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
    if (!data_fn_.empty()) {
        CloseFile();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamFilein::OpenFile()
{
    STREAM_TRACE("OpenFile %s", opts_.file_.c_str());
    int data_fd = open(opts_.file_.c_str(), O_RDONLY, S_IRUSR);
    if (data_fd < 0) {
        SetStateError(StreamError::DEVICE_FAIL, "open: " + string::StrError(errno));
        return false;
    }
    data_fd_ = data_fd;
    data_fn_ = opts_.file_;

    const std::string file_ts = opts_.file_ + ".ts";
    if (opts_.speed_ > 0.0) {
        int ts_fd = open(file_ts.c_str(), O_RDONLY, S_IRUSR);
        if (ts_fd < 0) {
            SetStateError(StreamError::DEVICE_FAIL, "open: " + string::StrError(errno));
            return false;
        }
        ts_fd_ = ts_fd;
        ts_fn_ = file_ts;
    }

    SetStateConnected();

    return true;
}

void StreamFilein::CloseFile()
{
    STREAM_TRACE("CloseFile %d %d", data_fd_, ts_fd_);
    opts_.disp_ = opts_.path_;

    if ((data_fd_ >= 0) && (close(data_fd_) != 0)) {
        STREAM_WARNING("close fail: %s", string::StrError(errno).c_str());
    }
    data_fd_ = -1;
    data_fn_.clear();

    if ((ts_fd_ >= 0) && (close(ts_fd_) != 0)) {
        STREAM_WARNING("close fail: %s", string::StrError(errno).c_str());
    }
    ts_fd_ = -1;
    ts_fn_.clear();

    return;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamFilein::Worker()
{
    uint8_t chunk[parser::MAX_ADD_SIZE];
    FileIdxTsRec prev_ts_rec;
    const bool have_ts = !ts_fn_.empty();
    uint64_t subms = 0;
    time::Duration dur;
    time::Duration skip;
    if (opts_.offs_ > 0.0) {
        skip.SetSec(opts_.offs_);
    }
    const double dt_scale = (opts_.speed_ > 0.0 ? 1.0 / opts_.speed_ : 1.0);
    time::Duration dt;

    STREAM_TRACE(
        "Worker fn=%s ts=%s dt_scale=%.3f skip=%.3f", data_fn_.c_str(), ts_fn_.c_str(), dt_scale, skip.GetSec(3));

    while (!thread_.ShouldAbort() && (data_fd_ >= 0)) {
        std::size_t chunk_size = sizeof(chunk);

        // If we have a .ts file, throttle the read accordingly
        if (have_ts) {
            // Read next timestamp record
            FileIdxTsRec ts_rec;
            const auto res = read(ts_fd_, &ts_rec, sizeof(ts_rec));
            if (res <= 0) {
                if (res < 0) {
                    SetStateError(StreamError::DEVICE_FAIL, "read .ts: " + string::StrError(errno));
                } else {
                    STREAM_TRACE("ts eof");
                }
                break;
            }

            if (!prev_ts_rec.time_.IsZero() && ts_rec.time_.Diff(prev_ts_rec.time_, dt)) {
                dur += dt;
            }

            STREAM_TRACE("Worker ts %s offs %" PRIuMAX " dur %.3f", ts_rec.time_.StrUtcTime().c_str(), ts_rec.offs_,
                dur.GetSec(3));

            // Sleep (but not on first ts) milliseconds, carry over the remainder sub-ms part to the next record
            if (dur > skip) {
                dt *= dt_scale;
                const uint64_t dt_ns = dt.GetNSec() + subms;
                const uint32_t sleep_ms = dt_ns / (uint64_t)1000000;
                subms = dt_ns % (uint64_t)1000000;
                STREAM_TRACE(
                    "Worker sleep %.9f = %" PRIuMAX " --> %" PRIu32 " %" PRIuMAX, dt.GetSec(), dt_ns, sleep_ms, subms);
                thread_.Sleep(sleep_ms);
            }

            chunk_size = ts_rec.offs_ - prev_ts_rec.offs_;
            prev_ts_rec = ts_rec;
        }

        if (chunk_size > 0) {
            const auto size = read(data_fd_, chunk, chunk_size);
            if (size <= 0) {
                if (size < 0) {
                    SetStateError(StreamError::DEVICE_FAIL, "read: " + string::StrError(errno));
                }
                break;
            }
            STREAM_TRACE("Worker read %" PRIiMAX, size);
            if (dur >= skip) {
                ProcessRead(chunk, size);
            }
        }
    }

    SetStateClosed();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamFilein::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_ongoing_));
    UNUSED(size);
    return false;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
