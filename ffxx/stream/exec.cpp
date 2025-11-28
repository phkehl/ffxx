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
 * @brief Stream EXEC
 */

/* LIBC/STL */
#include <algorithm>
#include <cctype>
#include <cerrno>

/* EXTERNAL */
#include <sys/stat.h>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/string.hpp>

/* PACKAGE */
#include "exec.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

/*static*/ std::unique_ptr<StreamOptsExec> StreamOptsExec::FromPath(
    const std::string& path, std::vector<std::string>& errors)
{
    StreamOptsExec opts;
    bool ok = true;

    // "<path>[[:<arg>]...]"
    const auto parts = string::StrSplit(path, ":");
    if (parts.size() >= 1) {
        struct stat st;
        if (stat(parts[0].c_str(), &st) == 0) {
            if ((st.st_mode & S_IXUSR) == S_IXUSR) {
                opts.argv_ = parts;
            } else {
                ok = false;
                errors.push_back("bad <path>, not executable");
            }
        } else {
            ok = false;
            errors.push_back("bad <path>: " + string::StrError(errno));
        };

    } else {
        ok = false;
    }

    if (!opts.argv_.empty()) {
        opts.path_ = string::StrJoin(opts.argv_, ":");
        opts.disp_ = opts.argv_[0];
    }

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsExec>(opts);
}

/* ****************************************************************************************************************** */

StreamExec::StreamExec(const StreamOptsExec& opts) /* clang-format off */ :
    StreamBase(opts, opts_),
    opts_          { opts },
    timer_         { ctx_ },
    rx_buf_        { boost::asio::buffer(rx_buf_data_) },
    stderr_buf_    { boost::asio::buffer(stderr_buf_data_) },
    thread_        { opts_.name_, std::bind(&StreamExec::Worker, this) }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();
}

StreamExec::~StreamExec()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamExec::Start()
{
    STREAM_TRACE("Start");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }

    if (!thread_.Start()) {
        return false;
    }

    if (!StartChild()) {
        Stop();
        return false;
    }

    return true;
}

void StreamExec::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    if (child_) {
        StopWaitTxDone(timeout);
    }
    StopChild();
    ctx_.stop();
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamExec::Worker()
{
    STREAM_TRACE("Worker");
    // Prevent run_for() below from returning early if there's no pending thing to handle. Otherwise we would busy-loop
    // below in case some stream implementation has states where nothing happens.
    auto work = boost::asio::make_work_guard(ctx_);
    while (!thread_.ShouldAbort()) {
        // Run i/o for a bit. The exact value of the time doesn't matter. It just needs to be small enough to make
        // ShouldAbort() "responsive" (and large enough to not busy wait)
        ctx_.run_for(std::chrono::milliseconds(337));
    }
    STREAM_TRACE("worker stop");
    SetStateClosed();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamExec::DoStart()
{
    TRACE("DoStart");
    if (!StartChild()) {
        DoRetry();
    }
}

bool StreamExec::StartChild()
{
    STREAM_TRACE("StartChild");

    SetStateConnecting();
    try {
        // clang-format off
        stderr_pipe_ = std::make_unique<boost::process::async_pipe>(ctx_);
        if (opts_.mode_ == StreamMode::WO) {
            stdin_pipe_ = std::make_unique<boost::process::async_pipe>(ctx_);
            child_ = std::make_unique<boost::process::child>(opts_.argv_,
                boost::process::std_in  = *stdin_pipe_,
                boost::process::std_out.close(),
                boost::process::std_err = *stderr_pipe_,
                boost::process::on_exit(std::bind(&StreamExec::OnExit, this, std::placeholders::_1, std::placeholders::_2)),
                ctx_);
        } else if (opts_.mode_ == StreamMode::RO) {
            stdout_pipe_ = std::make_unique<boost::process::async_pipe>(ctx_);
            child_ = std::make_unique<boost::process::child>(opts_.argv_,
                boost::process::std_in.close(),
                boost::process::std_out = *stdout_pipe_,
                boost::process::std_err = *stderr_pipe_,
                boost::process::on_exit(std::bind(&StreamExec::OnExit, this, std::placeholders::_1, std::placeholders::_2)),
                ctx_);
        } else {
            stdin_pipe_ = std::make_unique<boost::process::async_pipe>(ctx_);
            stdout_pipe_ = std::make_unique<boost::process::async_pipe>(ctx_);
            child_ = std::make_unique<boost::process::child>(opts_.argv_,
                boost::process::std_in  = *stdin_pipe_,
                boost::process::std_out = *stdout_pipe_,
                boost::process::std_err = *stderr_pipe_,
                boost::process::on_exit(std::bind(&StreamExec::OnExit, this, std::placeholders::_1, std::placeholders::_2)),
                ctx_);
        }
        // clang-format on
    } catch (std::system_error& ex) {
        SetStateError(StreamError::DEVICE_FAIL, ex.what());
        return false;
    }

    opts_.disp_ = opts_.argv_[0] + "[" + std::to_string(child_->id()) + "]";

    SetStateConnected();
    if (opts_.mode_ != StreamMode::WO) {
        DoRead();
    }
    DoStderr();
    DoInactTimeout();

    return true;
}

void StreamExec::StopChild()
{
    STREAM_TRACE("StopChild");
    if (child_ && child_->running()) {
        child_->terminate();
        child_.reset();
    }
}

void StreamExec::OnExit(const int status, const std::error_code& ec)
{
    STREAM_TRACE("OnExit %d %s", status, ec.message().c_str());
    UNUSED(ec);

    child_.reset();
    stdin_pipe_.reset();
    stdout_pipe_.reset();
    stderr_pipe_.reset();

    // Do not restart child on exit(0)
    if (status == 0) {
        timer_.cancel();
        SetStateClosed();
    }
    // (Maybe) try again if script failed (by itself, got killed by inactivity timeout)
    else {
        SetStateError(StreamError::DEVICE_FAIL, "exit(" + std::to_string(status) + ")");
        DoRetry();
    }

    opts_.disp_ = opts_.argv_[0];
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamExec::DoRetry()
{
    STREAM_TRACE("DoRetry");
    // CloseSocket();
    timer_.cancel();
    if (opts_.retry_to_.count() > 0) {
        timer_.expires_after(opts_.retry_to_);
        timer_.async_wait(std::bind(&StreamExec::OnRetry, this, std::placeholders::_1));
    } else {
        SetStateClosed();
    }
}

void StreamExec::OnRetry(const boost::system::error_code& ec)
{
    STREAM_TRACE("OnRetry %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Start child again
    else {
        DoStart();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamExec::DoInactTimeout()
{
    STREAM_TRACE("DoInactTimeout %" PRIiMAX, opts_.inact_to_.count());
    if (opts_.inact_to_.count() > 0) {
        timer_.cancel();
        timer_.expires_after(opts_.inact_to_);
        timer_.async_wait(std::bind(&StreamExec::OnInactTimeout, this, std::placeholders::_1));
    }
}

void StreamExec::OnInactTimeout(const boost::system::error_code& ec)
{
    STREAM_TRACE("OnInactTimeout %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Inactivity timeout expired
    else {
        SetStateError(StreamError::NO_DATA_RECV);
        StopChild();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamExec::DoWrite()
{
    // Get data from the write queue_ into the tx buffer and fire the asio write
    std::unique_lock<std::mutex> lock(mutex_);
    tx_buf_size_ = std::min(write_queue_.Used(), sizeof(tx_buf_data_));
    tx_buf_sent_ = 0;
    if (tx_buf_size_ > 0) {
        STREAM_TRACE("DoWrite %" PRIuMAX, tx_buf_size_);
        write_queue_.Read(tx_buf_data_, tx_buf_size_);
        stdin_pipe_->async_write_some(boost::asio::buffer(tx_buf_data_, tx_buf_size_),
            std::bind(&StreamExec::OnWrite, this, std::placeholders::_1, std::placeholders::_2));
    } else {
        STREAM_TRACE("DoWrite done");
        tx_ongoing_ = false;
        NotifyTxDone();
    }
}

void StreamExec::OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    STREAM_TRACE("OnWrite %" PRIuMAX "/%" PRIuMAX " %s", bytes_transferred, tx_buf_size_, ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Error writing
    else if (ec) {
        // @todo
        // SetStateError(StreamError::DEVICE_FAIL, "write: " + ec.message());
    }
    // Write successful, check if there's more to write
    else {
        // Data was not fully sent, some is left in the tx buffer
        tx_buf_sent_ += bytes_transferred;
        if (tx_buf_sent_ < tx_buf_size_) {
            stdin_pipe_->async_write_some(boost::asio::buffer(&tx_buf_data_[tx_buf_sent_], tx_buf_size_ - tx_buf_sent_),
                std::bind(&StreamExec::OnWrite, this, std::placeholders::_1, std::placeholders::_2));
        }
        // Everything sent, but check if there's new data to write
        else {
            tx_buf_size_ = 0;
            tx_buf_sent_ = 0;
            DoWrite();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamExec::DoRead()
{
    STREAM_TRACE("DoRead");
    stdout_pipe_->async_read_some(
        rx_buf_, std::bind(&StreamExec::OnRead, this, std::placeholders::_1, std::placeholders::_2));
}

void StreamExec::OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    STREAM_TRACE("OnRead %" PRIuMAX " %s", bytes_transferred, ec.message().c_str());
    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // E.g. boost::asio::error::eof (= script terminated)
    else if (ec) {
        // SetStateError(StreamError::CONN_LOST, "read " + ec.message());
        // socket_conn_ = false;
        // CloseSocket();
        // DoRetry();
    }
    // Process incoming data, and read again
    else {
        ProcessRead((const uint8_t*)rx_buf_.data(), bytes_transferred);
        DoRead();
        // DoInactTimeout();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamExec::DoStderr()
{
    STREAM_TRACE("DoStderr");
    stderr_pipe_->async_read_some(
        stderr_buf_, std::bind(&StreamExec::OnStderr, this, std::placeholders::_1, std::placeholders::_2));
}

void StreamExec::OnStderr(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    STREAM_TRACE("OnStderr %" PRIuMAX " %s", bytes_transferred, ec.message().c_str());
    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // E.g. boost::asio::error::eof (= remote closed connection)
    else if (ec) {
        // SetStateError(StreamError::CONN_LOST, "read " + ec.message());
        // socket_conn_ = false;
        // CloseSocket();
        // DoRetry();
    }
    // Process incoming data, and read again
    else {
        const bool printable = std::all_of(&stderr_buf_data_[0], &stderr_buf_data_[bytes_transferred],
            [](const uint8_t c) { return std::isprint(c) || std::isspace(c); });
        if (printable) {
            std::string str{ &stderr_buf_data_[0], &stderr_buf_data_[bytes_transferred] };
            string::StrTrim(str);
            STREAM_WARNING("%s stderr: %s", opts_.disp_.c_str(), str.c_str());
        } else {
            STREAM_WARNING("%s stderr: <%" PRIuMAX " bytes>", opts_.disp_.c_str(), bytes_transferred);
            TRACE_HEXDUMP(stderr_buf_data_, bytes_transferred, NULL, NULL);
        }
        DoStderr();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamExec::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_ongoing_));
    UNUSED(size);

    if (opts_.mode_ == StreamMode::RO) {
        return false;
    }

    // Initiate the write in the io context (thread), unless a write is already ongoing
    if (!tx_ongoing_) {
        tx_ongoing_ = true;
        boost::asio::post(ctx_, std::bind(&StreamExec::DoWrite, this));
    }

    return true;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
