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
 * @brief Stream base/common
 */

/* LIBC/STL */
#include <array>
#include <cmath>
#include <regex>

/* EXTERNAL */
#include <boost/asio/io_context.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/path.hpp>
#include <fpsdk_common/string.hpp>
#include <fpsdk_common/time.hpp>

/* PACKAGE */
#include "base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

// clang-format off
//
// ===== Notes =====
//
// Boost documentation
// - ASIO
//   - https://www.boost.org/doc/libs/1_74_0/doc/html/boost_asio/reference.html
//   - https://www.boost.org/doc/libs/1_74_0/doc/html/boost_asio/reference/ip__basic_resolver/async_resolve.html
//   - https://www.boost.org/doc/libs/1_74_0/doc/html/boost_asio/reference/ip__basic_resolver/async_resolve/overload2.html
//   - https://www.boost.org/doc/libs/1_74_0/doc/html/boost_asio/reference/ResolveHandler.html
//   - https://www.boost.org/doc/libs/1_74_0/boost/asio/ip/resolver_base.hpp
//   - https://www.boost.org/doc/libs/1_74_0/doc/html/boost_asio/reference/steady_timer.html
//   - https://www.boost.org/doc/libs/1_74_0/doc/html/boost_asio/reference/strand.html
// - Beast
//   - https://www.boost.org/doc/libs/develop/libs/beast/doc/html/beast/quickref.html
// - Process
//   - https://www.boost.org/doc/libs/1_71_0/doc/html/process.html
// - Interprocess
//   - https://www.boost.org/doc/libs/1_77_0/doc/html/interprocess.html
// - Examples
// - /usr/share/doc/libboost1.74-doc/examples/libs/asio/example/cpp11/
//
// clear; make build BUILD_TESTING=OFF && date | ./build/Debug/ffapps/streamtool -v -v -v tcpcli://172.22.1.44:21004 | head -n5
// while true; do netcat -n4lp 12345; sleep 1; done
// clear; make build BUILD_TESTING=OFF && date | ./build/Debug/ffapps/streamtool -v -v -v tcpcli://localhost:12345
// Listen UDP: while true; do netcat -u6lp 12345; sleep 1; done
// Listen UDP: tcpdump -i any udp and dst port 12345 -v -X
// clang-format on

// ---------------------------------------------------------------------------------------------------------------------

StreamBase::StreamBase(const StreamOpts& opts_init, const StreamOpts& opts_final) /* clang-format off */ :
    // We have:
    // 1. base class: const StreamOpts&      StreamBase:opts_
    // 2. impl class:       StreamOptsTcpcli StreamTcpci::opts_
    // The 1. is a reference to 2., so that the common options, such as path_ or disp_ update for 1. when 2.
    // changes them at runtime. However, 1. is initialised before 2., but we need access to the options
    // before that happens. That is, the below initialisers execute before opts_& contains anything useful.
    // Only once the derived class ctor has completed, opts_& contains those values. Therefore, we have
    // two arguments to the base ctor: the options as given to the derived class ctor (casted to StreamOpts)
    // as well as the reference to the final location, which is only populated once all ctors have completed.
    // A bit tricky.. but I couldn't come up with a neater way.
    opts_          { opts_final },
    write_queue_   { std::max(opts_init.w_queue_size_, StreamOpts::W_QUEUE_SIZE_MIN) }  // clang-format on
{
    if (!opts_init.quiet_) {
        INFO("Stream(%s) %s", opts_init.name_.c_str(), opts_init.spec_.c_str());
    }
}

StreamBase::~StreamBase()
{
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamBase::Read(parser::ParserMsg& msg, const uint32_t timeout)
{
    // A message may be waiting to be picked from the queue...
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // STREAM_TRACE("Read timeout %" PRIu32 " queue size %" PRIuMAX, timeout, read_queue_.size());
        if (!read_queue_.empty()) {
            msg = std::move(read_queue_.front());
            read_queue_.pop();
            return true;
        }
    }

    if ((state_ != StreamState::CONNECTED) || (opts_.mode_ == StreamMode::WO)) {
        STREAM_WARNING_THR(1000, "cannot read");
        return false;
    }

    // We have a timeout, let's try once more. Maybe we'll receive more data within the timeout. It still may not be a
    // full message, though.
    // @todo Make this use the full timeout period wait for an entire message, similar to Write()
    if ((timeout > 0) && Wait(timeout)) {
        return Read(msg, 0);
    }

    return false;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamBase::Write(const uint8_t* data, const std::size_t size, const uint32_t timeout)
{
    STREAM_TRACE("Write %" PRIuMAX " %" PRIu32, size, timeout);

    if ((data == NULL) || (size == 0)) {
        return false;
    }

    if ((state_ != StreamState::CONNECTED) || (opts_.mode_ == StreamMode::RO)) {
        STREAM_WARNING_THR(1000, "cannot write");
        return false;
    }

    std::size_t data_offs = 0;
    std::size_t rem_size = size;
    uint32_t rem_timeout = timeout;
    while (true) {
        // Write chunk of data to the write_queue_ and signal the implementation (asio context / thread) to process it
        {
            std::unique_lock<std::mutex> lock(mutex_);
            // Write as much as we can
            const std::size_t chunk_size = std::min(rem_size, write_queue_.Avail());
            STREAM_TRACE("Write rem_size=%" PRIuMAX " data_offs=%" PRIuMAX " chunk_size=%" PRIuMAX
                         " (write_queue_ used=%" PRIuMAX " avail=%" PRIuMAX ")",
                rem_size, data_offs, chunk_size, write_queue_.Used(), write_queue_.Avail());
            if (chunk_size > 0) {
                write_queue_.Write(&data[data_offs], chunk_size);
                if (!ProcessWrite(chunk_size)) {
                    break;
                }
                data_offs += chunk_size;
                rem_size -= chunk_size;
            }
        }

        // Done, everything written (to the write_queue_, not necessarily to to actual output)
        if (rem_size == 0) {
            break;
        }

        // Wait for write_queue_ to become available again if there's more to write and we have time left
        if ((rem_size != 0) && (rem_timeout > 0)) {
            const uint64_t t0 = time::GetMillis();
            STREAM_TRACE("Write rem_size=%" PRIuMAX " rem_timeout=%" PRIu32, rem_size, rem_timeout);
            write_sem_.WaitFor(rem_timeout);
            const uint64_t dt = time::GetMillis() - t0;
            // Time expired, we give up
            if (dt >= rem_timeout) {
                break;
            }
            // Otherwise we should again be able to send more data, with what's left of the timeout
            rem_timeout -= std::max((uint64_t)1, dt);  // assume it took at least 1ms
        }
        // Just in case...
        else {
            break;
        }
    }
    STREAM_TRACE("Write rem_size=%" PRIuMAX, rem_size);

    if (rem_size != 0) {
        STREAM_WARNING_THR(1000, "tx buf ovfl");
        return false;
    } else {
        return true;
    }
}

bool StreamBase::Write(const std::vector<uint8_t> data, const uint32_t timeout)
{
    return Write(data.data(), data.size(), timeout);
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamBase::Wait(const uint32_t millis)
{
    return (opts_.mode_ != StreamMode::WO) && (read_sem_.WaitFor(millis) == thread::WaitRes::WOKEN);
}

// ---------------------------------------------------------------------------------------------------------------------

uint32_t StreamBase::GetBaudrate()
{
    return 0;
}

bool StreamBase::SetBaudrate(const uint32_t /*baudrate*/)
{
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamBase::Autobaud(const AutobaudMode /*mode*/)
{
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------

const StreamOpts StreamBase::GetOpts()
{
    std::unique_lock<std::mutex> lock(mutex_);
    return opts_;
}

// ---------------------------------------------------------------------------------------------------------------------

StreamState StreamBase::GetState() const
{
    return state_;
}

// ---------------------------------------------------------------------------------------------------------------------

StreamError StreamBase::GetError() const
{
    return error_;
}

// ---------------------------------------------------------------------------------------------------------------------

std::string StreamBase::GetInfo()
{
    std::unique_lock<std::mutex> lock(mutex_);
    return info_;
}

// ---------------------------------------------------------------------------------------------------------------------

StreamMode StreamBase::GetMode() const
{
    return opts_.mode_;
}

// ---------------------------------------------------------------------------------------------------------------------

StreamType StreamBase::GetType() const
{
    return opts_.type_;
}

// ---------------------------------------------------------------------------------------------------------------------

parser::ParserStats StreamBase::GetParserStats()
{
    std::unique_lock<std::mutex> lock(mutex_);
    return read_parser_.GetStats();
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamBase::AddStateObserver(StateObserver observer)
{
    state_observers_.push_back(observer);
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamBase::AddReadObserver(ReadObserver observer)
{
    rxready_observers_.push_back(observer);
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamBase::SetStateClosed()
{
    // For convenience we don't report a close again
    if (!state_set_ || (state_ != StreamState::CLOSED)) {
        SetStateEx(StreamState::CLOSED, error_, info_);
    }
}

void StreamBase::SetStateConnecting(const std::string& info)
{
    SetStateEx(StreamState::CONNECTING, StreamError::NONE, info);
}

void StreamBase::SetStateConnected(const std::string& info)
{
    SetStateEx(StreamState::CONNECTED, StreamError::NONE, info);
}

void StreamBase::SetStateError(const StreamError error, const std::string& info)
{
    SetStateEx(StreamState::ERROR, error, info);
}

void StreamBase::SetStateEx(const StreamState state, const StreamError error, const std::string& info)
{
    StreamState old_state;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        old_state = state_;
        state_ = state;
        error_ = error;
        info_ = info;
        if (!opts_.quiet_) {
            std::string str = StreamStateStr(state_);
            str += " " + opts_.disp_;
            if (error_ != StreamError::NONE) {
                str += " ";
                str += StreamErrorStr(error_);
            }
            if (!info_.empty()) {
                str += " - " + info_;
            }
            switch (state_) {
                case StreamState::CLOSED:
                case StreamState::CONNECTING:
                case StreamState::CONNECTED:
                    STREAM_INFO("%s", str.c_str());
                    break;
                case StreamState::ERROR:
                    STREAM_WARNING("%s", str.c_str());
                    break;
            }
        }
    }
    for (auto& obs : state_observers_) {
        obs(old_state, state_, error_, info_);
    }
    state_set_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamBase::ProcessRead(const uint8_t* data, const std::size_t size)
{
    STREAM_TRACE("ProcessRead %" PRIuMAX " bytes", size);

    // Process incoming data
    if (!read_parser_.Add(data, size)) {
        STREAM_WARNING_THR(1000, "rx parser ovfl");  // we don't expect this ever
        read_parser_.Reset();
        read_parser_.Add(data, size);
    }

    // Add messages to queue
    bool queue_ovfl = false;
    bool have_msg = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        parser::ParserMsg msg;
        while (read_parser_.Process(msg)) {
            STREAM_TRACE("ProcessRead got %s size %" PRIuMAX ", queue size %" PRIuMAX, msg.name_.c_str(),
                msg.data_.size(), read_queue_.size());
            if (read_queue_.size() < opts_.r_queue_size_) {
                read_queue_.push(msg);
                have_msg = true;
            } else {
                queue_ovfl = true;
            }
        }
    }

    // Warn, throttled
    if (queue_ovfl) {
        STREAM_WARNING_THR(2000, "rx queue ovfl");
    }

    // Notify of new message(s)
    if (have_msg) {
        read_sem_.Notify();
        for (auto& obs : rxready_observers_) {
            obs();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamBase::NotifyTxDone()
{
    write_sem_.Notify();
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamBase::StopWaitTxDone(const uint32_t timeout)
{
    if (timeout != 0) {
        const auto s = time::Duration::FromNSec(5000000);  // 5ms
        const int n_s = timeout / 5;
        while (tx_ongoing_) {
            s.Sleep();
            if (n_s <= 0) {
                break;
            }
        }
    }
    if (tx_ongoing_) {
        STREAM_WARNING("Cancelling pending writes");
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamBase::StreamLoggingPrint(
    const fpsdk::common::logging::LoggingLevel level, const std::size_t repeat, const char* fmt, ...) const
{
    if (!fpsdk::common::logging::LoggingIsLevel(level)) {
        return;
    }

    char line[1000];
#if STREAM_TRACE_ENABLED
    const int len = std::snprintf(line, sizeof(line), "Stream(%s, %s, %s, %s, %04" PRIx16 ") ", opts_.name_.c_str(),
        StreamTypeStr(GetType()), StreamModeStr(GetMode()), StreamStateStr(GetState()),
        static_cast<uint16_t>(fpsdk::common::thread::ThisThreadId() & 0xffff));
#else
    const int len = std::snprintf(line, sizeof(line), "Stream(%s) ", opts_.name_.c_str());
#endif

    va_list args;
    va_start(args, fmt);
    std::vsnprintf(&line[len], sizeof(line) - len, fmt, args);
    va_end(args);

    fpsdk::common::logging::LoggingPrint(level, repeat, "%s", line);
}

/* ****************************************************************************************************************** */

bool MatchHostPortPath(const std::string& path, std::string& host, uint16_t& port, bool& ipv6, bool require_host)
{
    // <hostname>|<IPv4>|[<IPv6>]:<port>
    const std::regex host_port_re("^(?:\\[(.+)\\]|(.*)):([0-9]+)$");
    std::smatch m;
    uint16_t portval = 0;
    if (std::regex_match(path, m, host_port_re) && (m.size() == 4) && string::StrToValue(m[3].str(), portval)) {
        const std::string h1 = m[1].str();
        const std::string h2 = m[2].str();
        host = h1.empty() ? h2 : h1;
        port = portval;
        ipv6 = !h1.empty();
        return (!require_host || !host.empty()) && (port >= StreamOpts::PORT_MIN) &&
               (StreamOpts::PORT_MAX == std::numeric_limits<uint16_t>::max() || (port <= StreamOpts::PORT_MAX));
    } else {
        return false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

template <typename T>
static std::vector<typename T::endpoint> ResolveEndpointsEx(
    const std::string& host, const uint16_t port, const bool want_ipv6, std::string& error)
{
    // DEBUG("ResolveEndpointsEx host=%s port=%" PRIu16 " want_ipv6=%s", host.c_str(), port, want_ipv6 ? "true" :
    // "false");
    std::vector<typename T::endpoint> endpoints;
    // - Resolve endpoints for given host and port
    if (!host.empty()) {
        boost::asio::io_context ctx;
        boost::system::error_code ec;
        typename T::resolver resolver(ctx);
        const typename T::resolver::results_type resolve_results =
            resolver.resolve(host, std::to_string(port), /*boost::asio::ip::resolver_query_base::flags::...,*/ ec);
        if (ec) {
            error = ec.message();
            return endpoints;
        }
        for (auto& cand : resolve_results) {
            const bool ipv4 = cand.endpoint().address().is_v4();
            const bool ipv6 = cand.endpoint().address().is_v6();
            // want_ipv6 = true ("[<host>]") --> use only IPv6 endpoint(s)
            // want_ipv6 = false ("<host>")  --> accept both IPv4 and IPv6 endpoint(s)
            bool use = ((want_ipv6 && ipv6) || (!want_ipv6 && (ipv4 || ipv6)));
            DEBUG("Resolve %s %" PRIu16 " --> %s %" PRIu16 " (%s): %s", host.c_str(), port,
                cand.endpoint().address().to_string().c_str(), cand.endpoint().port(),
                ipv4 ? "IPv4" : (ipv6 ? "IPv6" : "other"), use ? "use" : "skip");
            if (use) {
                endpoints.push_back(cand.endpoint());
            }
        }
    }
    // - Use all interfaces
    else {
        endpoints.push_back({ boost::asio::ip::address_v6::any(), port });
        if (!want_ipv6) {
            endpoints.push_back({ boost::asio::ip::address_v4::any(), port });
        }
    }
    return endpoints;
}

std::vector<boost::asio::ip::tcp::endpoint> ResolveTcpEndpoints(
    const std::string& host, const uint16_t port, const bool want_ipv6, std::string& error)
{
    return ResolveEndpointsEx<boost::asio::ip::tcp>(host, port, want_ipv6, error);
}

std::vector<boost::asio::ip::udp::endpoint> ResolveUdpEndpoints(
    const std::string& host, const uint16_t port, const bool want_ipv6, std::string& error)
{
    return ResolveEndpointsEx<boost::asio::ip::udp>(host, port, want_ipv6, error);
}

// ---------------------------------------------------------------------------------------------------------------------

std::string HostPortStr(const std::string& host, const uint16_t port, const bool ipv6)
{
    return (ipv6 ? "[" : "") + host + (ipv6 ? "]" : "") + ":" + std::to_string(port);
}

std::string HostPortStr(const boost::asio::ip::tcp::endpoint& endpoint)
{
    return HostPortStr(endpoint.address().to_string(), endpoint.port(), endpoint.address().is_v6());
}

std::string HostPortStr(const boost::asio::ip::udp::endpoint& endpoint)
{
    return HostPortStr(endpoint.address().to_string(), endpoint.port(), endpoint.address().is_v6());
}

// ---------------------------------------------------------------------------------------------------------------------

bool CredentialsToAuth(const std::string& credentials, std::string& auth_plain, std::string& auth_base64)
{
    bool ok = true;
    if (credentials.size() < 3) {
        ok = false;
    }
    // - "=<base64_encoded_username:password>"
    else if (credentials[0] == '=') {
        auth_base64 = credentials.substr(1);
        auth_plain = string::BufToStr(string::Base64Dec(auth_base64));
    }
    // - "%<path>"
    else if (credentials[0] == '%') {
        std::vector<uint8_t> data;
        if (path::FileSlurp(credentials.substr(1), data) && !data.empty() && (data.size() < StreamOpts::MAX_PATH_LEN)) {
            std::string user_pass = { data.data(), data.data() + data.size() };
            string::StrTrim(user_pass);
            if (user_pass.empty()) {
                return false;
            } else {
                if (user_pass[0] == '=') {
                    auth_base64 = user_pass.substr(1);
                } else {
                    auth_plain = user_pass;
                    auth_base64 = string::Base64Enc(string::StrToBuf(auth_plain));
                }
            }
        } else {
            ok = false;
        }
    }
    // - "<password>", "<username>:<password>"
    else {
        auth_plain = credentials;
        auth_base64 = string::Base64Enc(string::StrToBuf(auth_plain));
    }

    return ok && !auth_plain.empty() && !auth_base64.empty();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StrToAutobaudMode(const std::string& str, AutobaudMode& mode)
{
    bool ok = true;
    // clang-format off
    if      (str == AutobaudModeStr(AutobaudMode::NONE))    { mode = AutobaudMode::NONE;    }
    else if (str == AutobaudModeStr(AutobaudMode::PASSIVE)) { mode = AutobaudMode::PASSIVE; }
    else if (str == AutobaudModeStr(AutobaudMode::UBX))     { mode = AutobaudMode::UBX;     }
    else if (str == AutobaudModeStr(AutobaudMode::FP))      { mode = AutobaudMode::FP;      }
    else if (str == AutobaudModeStr(AutobaudMode::AUTO))    { mode = AutobaudMode::AUTO;    }
    else                                            { ok = false; }
    // clang-format on
    return ok;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
