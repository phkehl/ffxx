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
 * @brief Stream serial port
 */

/* LIBC/STL */
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <termios.h>
#include <unistd.h>

/* EXTERNAL */
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/string.hpp>

/* PACKAGE */
#include "serial.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

static bool IsAcmDevice(const std::string& path)
{
    // https://www.kernel.org/doc/html/latest/admin-guide/devices.html, kernel/drivers/usb/class/cdc-acm.h
    struct statx stx;
    if ((statx(AT_FDCWD, path.c_str(), 0, STATX_MODE, &stx) == 0) &&
        ((stx.stx_rdev_major == 166) || (stx.stx_rdev_major == 167))) {
        return true;
    };

    // Maybe the device isn't available right now. We can maybe still tell that it is an ACM device
    // E.g. /dev/serial/by-id/usb-u-blox_AG_-_www.u-blox.com_u-blox_GNSS_receiver-if00 -> ../../ttyACM1
    if (string::StrContains(path, "ttyACM") || string::StrContains(path, "usb-u-blox_AG")) {
        return true;
    }

    return false;
}

/*static*/ std::unique_ptr<StreamOptsSerial> StreamOptsSerial::FromPath(
    const std::string& path, std::vector<std::string>& errors)
{
    StreamOptsSerial opts;
    bool ok = true;

    // "<device>[:<baudrate>[:<autobaud>[:<mode>[:<flow>]]]]"
    const auto parts = string::StrSplit(path, ":");
    if ((parts.size() >= 1) && (parts.size() <= 5) && !parts[0].empty()) {
        opts.device_ = parts[0];
        if (parts.size() > 1) {
            uint32_t baudrate = 0;
            if (string::StrToValue(parts[1], baudrate) &&
                (std::find(BAUDRATES.cbegin(), BAUDRATES.cend(), baudrate) != BAUDRATES.end())) {
                opts.baudrate_ = baudrate;
            } else {
                errors.push_back("bad <baudrate>");
                ok = false;
            }
        } else {
            // Use maximum baudrate for ACM devices (they don't really have a baudrate)
            opts.baudrate_ = (IsAcmDevice(opts.device_) ? StreamOpts::BAUDRATES.back() : 115200);
        }
        if (parts.size() > 2) {
            if (!StrToAutobaudMode(string::StrToUpper(parts[2]), opts.autobaud_)) {
                ok = false;
                errors.push_back("bad <autobaud>");
            }
        }
        if (parts.size() > 3) {
            // clang-format off
            if      (parts[3] == "8N1") { opts.serial_mode_ = SerialMode::_8N1; }
            // @todo all options: [7|8][N|E|O][1|1.5|2]
            else {
                ok = false;
                errors.push_back("bad <mode>");
            }
            // clang-format on
        } else {
            opts.serial_mode_ = SerialMode::_8N1;
        }
        if (parts.size() > 4) {
            // clang-format off
            if      (parts[4] == "off") { opts.serial_flow_ = SerialFlow::OFF; }
            else if (parts[4] == "sw")  { opts.serial_flow_ = SerialFlow::SW; }
            else if (parts[4] == "hw")  { opts.serial_flow_ = SerialFlow::HW; }
            else {
                ok = false;
                errors.push_back("bad <flow>");
            }
            // clang-format on
        } else {
            opts.serial_flow_ = SerialFlow::OFF;
        }
    } else {
        ok = false;
        errors.push_back("bad <device> or <baudrate>");
    }

    opts.UpdatePath();

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsSerial>(opts);
}

void StreamOptsSerial::UpdatePath()
{
    path_ = device_ + ":" + std::to_string(baudrate_) + ":" + string::StrToLower(AutobaudModeStr(autobaud_));
    switch (serial_mode_) {  // clang-format off
        case SerialMode::UNSPECIFIED: path_ += ":?";   break;
        case SerialMode::_8N1:        path_ += ":8N1"; break;
    }  // clang-format on
    switch (serial_flow_) {  // clang-format off
        case SerialFlow::UNSPECIFIED: path_ += ":?";   break;
        case SerialFlow::OFF:         path_ += ":off"; break;
        case SerialFlow::SW:          path_ += ":sw";  break;
        case SerialFlow::HW:          path_ += ":hw";  break;
    }  // clang-format on
    disp_.clear();
    opts_.clear();
    UpdateSpec();
}

/* ****************************************************************************************************************** */

StreamSerial::StreamSerial(const StreamOptsSerial& opts) /* clang-format off */ :
    StreamBase(opts, opts_),
    opts_          { opts },
    port_          { ctx_ },
    port_open_     { false },
    timer_         { ctx_ },
    rx_buf_        { boost::asio::buffer(rx_buf_data_) },
    tx_buf_size_   { 0 },
    tx_buf_sent_   { 0 },
    thread_        { opts_.name_, std::bind(&StreamSerial::Worker, this) },
    autobauder_    { *this }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();
}

StreamSerial::~StreamSerial()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamSerial::Start()
{
    STREAM_TRACE("Start");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }
    if (!thread_.Start()) {
        return false;
    }
    if (opts_.hotplug_) {
        DoOpen();
    } else {
        SetStateConnecting("open");
        if (OpenPort()) {
            if (opts_.autobaud_ != AutobaudMode::NONE) {
                Autobaud(opts_.autobaud_);
            } else {
                SetStateConnected(std::to_string(opts_.baudrate_));
                DoRead();
                DoInactTimeout();
            }
        } else {
            Stop();
            return false;
        }
    }

    return true;
}

void StreamSerial::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    if (port_open_) {
        StopWaitTxDone(timeout);
    }
    ctx_.stop();
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
    ClosePort();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamSerial::Worker()
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

bool StreamSerial::OpenPort()
{
    STREAM_TRACE("OpenPort (%s)", port_open_ ? "open" : "closed");
    if (port_open_) {
        return false;
    }

    // Open port
    boost::system::error_code ec;
    port_.open(opts_.device_, ec);
    if (ec) {
        SetStateError(StreamError::DEVICE_FAIL, "open: " + ec.message());
        return false;
    }

    port_open_ = true;

    // Get lock on file
    int res = flock(port_.native_handle(), LOCK_EX | LOCK_NB);
    if (res != 0) {
        SetStateError(StreamError::DEVICE_FAIL, "lock: " + string::StrError(errno));
        ClosePort();
        return false;
    }

    using spb = boost::asio::serial_port_base;

    std::vector<std::string> errors;
    spb::parity parity;
    spb::character_size character_size;
    spb::stop_bits stop_bits;
    spb::flow_control flow_control;
    switch (opts_.serial_mode_) {
        case StreamOpts::SerialMode::UNSPECIFIED:
            break;
        case StreamOpts::SerialMode::_8N1:
            parity = spb::parity(spb::parity::none);
            character_size = spb::character_size(8);
            stop_bits = spb::stop_bits(spb::stop_bits::one);
            break;
    }
    switch (opts_.serial_flow_) {
        case StreamOpts::SerialFlow::UNSPECIFIED:
            break;
        case StreamOpts::SerialFlow::OFF:
            flow_control = spb::flow_control(spb::flow_control::none);
            break;
        case StreamOpts::SerialFlow::SW:
            flow_control = spb::flow_control(spb::flow_control::software);
            break;
        case StreamOpts::SerialFlow::HW:
            flow_control = spb::flow_control(spb::flow_control::hardware);
            break;
    }

    port_.set_option(spb::baud_rate(opts_.baudrate_), ec);
    if (ec) {
        errors.push_back("baud_rate: " + ec.message());
    }
    port_.set_option(parity, ec);
    if (ec) {
        errors.push_back("parity: " + ec.message());
    }
    port_.set_option(character_size, ec);
    if (ec) {
        errors.push_back("character_size: " + ec.message());
    }
    port_.set_option(stop_bits, ec);
    if (ec) {
        errors.push_back("stop_bits: " + ec.message());
    }
    port_.set_option(flow_control, ec);
    if (ec) {
        errors.push_back("flow_control: " + ec.message());
    }

    if (!errors.empty()) {
        SetStateError(StreamError::DEVICE_FAIL, "config: " + string::StrJoin(errors, ", "));
        ClosePort();
        return false;
    }

    // Set RTS/CTS. Some devices (Hello ST! Hello Quectel!) need this... :-/ Man page: tty_ioctl(2)
    // @todo Make this stream options?
    const bool RTS = false;  // false=clear, true=set
    const bool DTR = false;  // false=clear, true=set
    const int RTSbits = TIOCM_RTS;
    const int DTRbits = TIOCM_DTR;
    const int fd = port_.native_handle();
    if (ioctl(fd, RTS ? TIOCMBIS : TIOCMBIC, &RTSbits) < 0) {
        STREAM_WARNING("Failed setting RTS line for %s: %s", opts_.device_.c_str(), string::StrError(errno).c_str());
        // Continue anyway...
    }
    if (ioctl(fd, DTR ? TIOCMBIS : TIOCMBIC, &DTRbits) < 0) {
        STREAM_WARNING("Failed setting DTR line for %s: %s", opts_.device_.c_str(), string::StrError(errno).c_str());
        // Continue anyway...
    }

    // It appears that some (?) ACM devices sometimes (?) have some old data buffered that we would read.
    // @todo How to discard all that data? None of the below seems to work..
    //   // always gives 0, even if there is data
    //   int val = 0; ioctl(port_.lowest_layer().native_handle(), FIONREAD, &val), val);
    //   // doesn't seem to do anything either
    //   tcflush(port_.lowest_layer().native_handle(), TCIOFLUSH);
    //   // returns immediately, reads nothing
    //   boost::asio::read(port_, boost::asio::buffer(buf, sizeof(buf)), boost::asio::transfer_at_least(0), ec);

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamSerial::ClosePort()
{
    STREAM_TRACE("ClosePort (%s)", port_open_ ? "open" : "closed");
    if (!port_open_) {
        return;
    }

    boost::system::error_code ec;
    port_.cancel(ec);
    if (ec) {
        STREAM_WARNING("port cancel: %s", ec.message().c_str());
    }

    int res = flock(port_.native_handle(), LOCK_UN);
    if (res != 0) {
        STREAM_WARNING("port unlock: %s", string::StrError(errno).c_str());
    }

    port_.close(ec);
    if (ec) {
        STREAM_WARNING("port close: %s", ec.message().c_str());
    }
    port_open_ = false;
}

// ---------------------------------------------------------------------------------------------------------------------

uint32_t StreamSerial::GetBaudrate()
{
    return opts_.baudrate_;
}

bool StreamSerial::SetBaudrate(const uint32_t baudrate)
{
    STREAM_DEBUG("SetBaudrate %" PRIu32, baudrate);
    // Physical baudrate change only if connected (port is open and we can change its baudrate)
    const auto state = GetState();
    if (port_.is_open()) {  // CONNECTING or CONNECTED
        boost::system::error_code ec;
        port_.set_option(boost::asio::serial_port_base::baud_rate(baudrate), ec);
        if (ec) {
            STREAM_WARNING("Failed setting baudrate %" PRIu32 ": %s", baudrate, ec.message().c_str());
            return false;
        }
    }

    // Always update options
    opts_.baudrate_ = baudrate;
    opts_.UpdatePath();

    // Notify, and also reset the read timeout
    if (state == StreamState::CONNECTED) {
        SetStateConnected(std::to_string(opts_.baudrate_));
        DoInactTimeout();
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamSerial::DoRetry()
{
    STREAM_TRACE("DoRetry");
    timer_.cancel();
    if (opts_.retry_to_.count() > 0) {
        timer_.expires_after(opts_.retry_to_);
        timer_.async_wait(std::bind(&StreamSerial::OnRetry, this, std::placeholders::_1));
    } else {
        SetStateClosed();
    }
}

void StreamSerial::OnRetry(const boost::system::error_code& ec)
{
    STREAM_TRACE("OnRetry %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Initiate a new cycle (open)
    else {
        DoOpen();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamSerial::DoOpen()
{
    STREAM_TRACE("DoOpen");

    SetStateConnecting("open");
    boost::asio::post(ctx_, std::bind(&StreamSerial::OnOpen, this, boost::system::error_code()));
}

void StreamSerial::OnOpen(const boost::system::error_code& ec)
{
    STREAM_TRACE("OnOpen %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
        return;
    }
    // Open
    else {
        if (OpenPort()) {
            if (opts_.autobaud_ != AutobaudMode::NONE) {
                Autobaud(opts_.autobaud_);
            } else {
                SetStateConnected(std::to_string(opts_.baudrate_));
                DoRead();
                DoInactTimeout();
            }
        } else {
            DoRetry();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamSerial::Autobaud(const AutobaudMode mode)
{
    return autobauder_.Start(mode, [this](const bool success) {
        if (success) {
            SetStateConnected(std::to_string(opts_.baudrate_));
            DoRead();
            DoInactTimeout();
        } else {
            SetStateError(StreamError::CONNECT_FAIL, "autobaud fail");
            ClosePort();
            DoRetry();
        }
    });
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamSerial::DoInactTimeout()
{
    STREAM_TRACE("DoInactTimeout");
    if (opts_.inact_to_.count() > 0) {
        timer_.cancel();
        timer_.expires_after(opts_.inact_to_);
        timer_.async_wait(std::bind(&StreamSerial::OnInactTimeout, this, std::placeholders::_1));
    }
}

void StreamSerial::OnInactTimeout(const boost::system::error_code& ec)
{
    STREAM_TRACE("OnInactTimeout %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Inactivity timeout expired
    else {
        SetStateError(StreamError::NO_DATA_RECV);
        ClosePort();
        DoRetry();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamSerial::DoRead()
{
    STREAM_TRACE("DoRead");
    port_.async_read_some(
        rx_buf_, std::bind(&StreamSerial::OnRead, this, std::placeholders::_1, std::placeholders::_2));
}

void StreamSerial::OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    STREAM_TRACE("OnRead %" PRIuMAX " %s", bytes_transferred, ec.message().c_str());
    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // E.g. boost::asio::error::eof (= remote closed connection)
    else if (ec) {
        SetStateError(StreamError::DEVICE_FAIL, "read: " + ec.message());
        ClosePort();
        DoRetry();
    }
    // Process incoming data, and read again
    else {
        ProcessRead((const uint8_t*)rx_buf_.data(), bytes_transferred);
        DoRead();
        DoInactTimeout();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamSerial::DoWrite()
{
    // Get data from the write queue into the tx buffer and fire the asio write
    std::unique_lock<std::mutex> lock(mutex_);
    tx_buf_size_ = std::min(write_queue_.Used(), sizeof(tx_buf_data_));
    tx_buf_sent_ = 0;
    if (tx_buf_size_ > 0) {
        STREAM_TRACE("DoWrite %" PRIuMAX, tx_buf_size_);
        write_queue_.Read(tx_buf_data_, tx_buf_size_);
        port_.async_write_some(boost::asio::buffer(tx_buf_data_, tx_buf_size_),
            std::bind(&StreamSerial::OnWrite, this, std::placeholders::_1, std::placeholders::_2));
    } else {
        STREAM_TRACE("DoWrite done");
        tx_ongoing_ = false;
        NotifyTxDone();
    }
}

void StreamSerial::OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    STREAM_TRACE("OnWrite %" PRIuMAX "/%" PRIuMAX " %s", bytes_transferred, tx_buf_size_, ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Error writing
    else if (ec) {
        SetStateError(StreamError::DEVICE_FAIL, "write: " + ec.message());
        ClosePort();
        DoRetry();
    }
    // Write successfull, check if there's more to write
    else {
        // Data was not fully sent, some is left in the tx buffer
        tx_buf_sent_ += bytes_transferred;
        if (tx_buf_sent_ < tx_buf_size_) {
            port_.async_write_some(boost::asio::buffer(&tx_buf_data_[tx_buf_sent_], tx_buf_size_ - tx_buf_sent_),
                std::bind(&StreamSerial::OnWrite, this, std::placeholders::_1, std::placeholders::_2));
        }
        // Everything sent, but check if there's new data to write
        else {
            tx_buf_size_ = 0;
            tx_buf_sent_ = 0;
            DoWrite();
        }
    }
}

bool StreamSerial::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_ongoing_));
    UNUSED(size);

    // Initiate the write in the io context (thread), unless a write is already ongoing
    if (!tx_ongoing_) {
        tx_ongoing_ = true;
        boost::asio::post(ctx_, std::bind(&StreamSerial::DoWrite, this));
    }

    return true;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
