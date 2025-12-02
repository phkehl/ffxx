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
 * @brief CAN stream
 */

/* LIBC/STL */
#include <cerrno>
#include <cstring>

/* EXTERNAL */
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/string.hpp>

/* PACKAGE */
#include "canstr.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

/*static*/ std::unique_ptr<StreamOptsCanstr> StreamOptsCanstr::FromPath(
    const std::string& path, std::vector<std::string>& errors)
{
    StreamOptsCanstr opts;
    bool ok = true;

    // <dev>:<canid_in>:<canid_out>[:<ff>[:<fd>[:<brs>]]]
    const auto parts = string::StrSplit(path, ":");
    if ((parts.size() >= 3) && (parts.size() <= 6)) {
        if (!parts[0].empty()) {
            opts.device_ = parts[0];
        } else {
            errors.push_back("bad <device>" + parts[0] + "/" + path);
            ok = false;
        }
        if (parts.size() >= 4) {
            if (parts[3] == "eff") {
                opts.eff_ = true;
            } else if (parts[3] == "sff") {
                opts.eff_ = false;
            } else {
                errors.push_back("bad <ff>");
                ok = false;
            }
        }
        uint32_t canid = 0;
        if (!string::StrToValue(parts[1], canid) || (!opts.eff_ && ((canid < 0x001) || canid > 0x7ff)) ||
            (opts.eff_ && ((canid < 0x00000001) || canid > 0x1fffffff))) {
            errors.push_back("bad <canid_in>");
            ok = false;
        } else {
            opts.canid_in_ = canid;
        }
        if (!string::StrToValue(parts[2], canid) || (!opts.eff_ && ((canid < 0x001) || canid > 0x7ff)) ||
            (opts.eff_ && ((canid < 0x00000001) || canid > 0x1fffffff))) {
            errors.push_back("bad <canid_out>");
            ok = false;
        } else {
            opts.canid_out_ = canid;
        }
        if (parts.size() >= 5) {
            if (parts[4] == "fd") {
                opts.fd_ = true;
            } else if (parts[4].empty()) {
                opts.fd_ = false;
            } else {
                errors.push_back("bad <fd>");
                ok = false;
            }
        }
        if (parts.size() >= 6) {
            if (opts.fd_ && (parts[5] == "brs")) {
                opts.brs_ = true;
            } else if (parts[5].empty()) {
                opts.brs_ = false;
            } else {
                errors.push_back("bad <brs>");
                ok = false;
            }
        }
    } else {
        ok = false;
    }

    opts.path_ = string::Sprintf("%s:0x%0*" PRIx32 ":0x%0*" PRIx32 ":%s:%s:%s", opts.device_.c_str(), opts.eff_ ? 8 : 3,
        opts.canid_in_, opts.eff_ ? 8 : 3, opts.canid_out_, opts.eff_ ? "eff" : "sff", opts.fd_ ? "fd" : "",
        opts.brs_ ? "brs" : "");

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsCanstr>(opts);
}

/* ****************************************************************************************************************** */

StreamCanstr::StreamCanstr(const StreamOptsCanstr& opts) /* clang-format off */ :
    StreamBase(opts, opts_),
    opts_         { opts },
    can_          { opts_.device_ },
    stream_       { ctx_ },
    rx_buf_       { boost::asio::buffer(rx_buf_data_) },
    thread_       { opts_.name_, std::bind(&StreamCanstr::Worker, this) }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();

    tx_frame_.can_id_ = opts_.canid_out_;
    tx_frame_.is_eff_ = opts_.eff_;
    tx_frame_.is_fd_ = opts_.fd_;
}

StreamCanstr::~StreamCanstr()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamCanstr::Start()
{
    STREAM_TRACE("Start");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }

    if (!thread_.Start()) {
        return false;
    }

    SetStateConnecting();

    if (!OpenCan()) {
        Stop();
        return false;
    }

    DoRead();

    return true;
}
void StreamCanstr::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    if (can_.IsOpen()) {
        StopWaitTxDone(timeout);
    }
    CloseCan();
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamCanstr::OpenCan()
{
    STREAM_TRACE("OpenCan");
    std::vector<struct can_filter> filter = { { opts_.canid_in_, opts_.eff_ ? CAN_EFF_MASK : CAN_SFF_MASK } };
    if (!can_.Open() || !can_.SetFilters(filter)) {
        SetStateError(StreamError::DEVICE_FAIL, "open: " + can_.GetStrerror());
        return false;
    }

    stream_.assign(can_.GetSocket());

    SetStateConnected();

    return true;
}

void StreamCanstr::CloseCan()
{
    STREAM_TRACE("CloseCan");
    can_.Close();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamCanstr::Worker()
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
    SetStateClosed();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamCanstr::DoRead()
{
    STREAM_TRACE("DoRead");
    stream_.async_read_some(
        rx_buf_, std::bind(&StreamCanstr::OnRead, this, std::placeholders::_1, std::placeholders::_2));
}

void StreamCanstr::OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    STREAM_TRACE("OnRead %" PRIuMAX " %s", bytes_transferred, ec.message().c_str());
    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // E.g. boost::asio::error::eof (= remote closed connection)
    else if (ec) {
        SetStateError(StreamError::DEVICE_FAIL, "read: " + ec.message());
        CloseCan();
    }
    // Process incoming data, and read again
    else {
        can::CanFrame frame;
        if (frame.RawToFrame((const uint8_t*)rx_buf_.data(), bytes_transferred)) {
            ProcessRead(frame.data_, frame.data_len_);
        } else {
            STREAM_WARNING_THR(1000, "bad frame size %" PRIuMAX, bytes_transferred);
        }
        DoRead();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamCanstr::DoWrite()
{
    // Get data from the write queue_ into the tx buffer and fire the asio write
    std::unique_lock<std::mutex> lock(mutex_);
    const std::size_t data_size = (opts_.fd_ ? sizeof(tx_frame_.fd_frame_.data) : sizeof(tx_frame_.can_frame_.data));
    const std::size_t tx_size = std::min(write_queue_.Used(), data_size);
    if (tx_size > 0) {
        STREAM_TRACE("DoWrite %" PRIuMAX "/%" PRIuMAX, tx_size, data_size);
        write_queue_.Read(tx_frame_.fd_frame_.data /* = tx_frame_.data_ */, tx_size);
        tx_frame_.data_len_ = tx_size;
        tx_frame_.FrameToRaw();
        const std::size_t frame_size = (opts_.fd_ ? sizeof(tx_frame_.fd_frame_) : sizeof(tx_frame_.can_frame_));
        stream_.async_write_some(boost::asio::buffer(&tx_frame_.fd_frame_, frame_size),
            std::bind(&StreamCanstr::OnWrite, this, std::placeholders::_1, std::placeholders::_2));
    } else {
        STREAM_TRACE("DoWrite done");
        tx_ongoing_ = false;
        NotifyTxDone();
    }
}

void StreamCanstr::OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    STREAM_TRACE("OnWrite %" PRIuMAX " %s", bytes_transferred, ec.message().c_str());
    UNUSED(bytes_transferred);

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Error writing
    else if (ec) {
        SetStateError(StreamError::DEVICE_FAIL, "write: " + ec.message());
        CloseCan();
    }
    // Write successful, check if there's more to write
    else {
        // Hopefully everything was sent, but check if there's more data to write
        DoWrite();
    }
}

bool StreamCanstr::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_ongoing_));
    UNUSED(size);

    // Initiate the write in the io context (thread), unless a write is already ongoing
    if (!tx_ongoing_) {
        tx_ongoing_ = true;
        boost::asio::post(ctx_, std::bind(&StreamCanstr::DoWrite, this));
    }

    return true;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
