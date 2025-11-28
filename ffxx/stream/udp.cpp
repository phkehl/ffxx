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
 * @brief Stream UDP client and server
 */

/* LIBC/STL */
#include <algorithm>
#include <chrono>
#include <functional>
#include <iterator>

/* EXTERNAL */
#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/post.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/string.hpp>

/* PACKAGE */
#include "udp.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

/*static*/ std::unique_ptr<StreamOptsUdpcli> StreamOptsUdpcli::FromPath(
    const std::string& path, std::vector<std::string>& errors)
{
    StreamOptsUdpcli opts;

    bool ok = true;

    // "<host>:<port>"
    if (!MatchHostPortPath(path, opts.host_, opts.port_, opts.ipv6_) || opts.host_.empty() || (opts.port_ < PORT_MIN) ||
        (opts.port_ > PORT_MAX)) {
        ok = false;
        errors.push_back("bad <host> or <port>");
    }

    opts.path_ = HostPortStr(opts.host_, opts.port_, opts.ipv6_);

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsUdpcli>(opts);
}

/* ****************************************************************************************************************** */

StreamUdpcli::StreamUdpcli(const StreamOptsUdpcli& opts) /* clang-format off */ :
    StreamBase(opts, opts_),
    opts_          { opts },
    resolver_      { ctx_ },
    socket_        { ctx_ },
    tx_buf_size_   { 0 },
    tx_buf_sent_   { 0 },
    thread_        { opts_.name_, std::bind(&StreamUdpcli::Worker, this) }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();
}

StreamUdpcli::~StreamUdpcli()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamUdpcli::Start()
{
    STREAM_TRACE("Start");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }
    if (!thread_.Start()) {
        return false;
    }

    // Find endpoint(s)
    SetStateConnecting("resolve");
    {
        std::string error;
        const auto endpoints = ResolveUdpEndpoints(opts_.host_, opts_.port_, opts_.ipv6_, error);
        if (endpoints.empty()) {
            SetStateError(StreamError::RESOLVE_FAIL, error);
            SetStateClosed();
            return false;
        }

        // Use first endpoint
        endpoint_ = endpoints[0];
    }

    // Connect
    SetStateConnecting(HostPortStr(endpoint_));
    boost::system::error_code ec;

    socket_.connect(endpoint_, ec);
    if (ec) {
        SetStateError(StreamError::CONNECT_FAIL, ec.message());
        return false;
    }
    ConfigSocket();
    SetStateConnected(HostPortStr(endpoint_));

    return true;
}

void StreamUdpcli::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    if (socket_.is_open()) {
        StopWaitTxDone(timeout);
    }
    CloseSocket();
    ctx_.stop();
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamUdpcli::Worker()
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

void StreamUdpcli::ConfigSocket()
{
    STREAM_TRACE("ConfigSocket");
    boost::system::error_code ec;
    // @todo good? bad? s.a. Stream{Tcp,Udp,Ntrip}{cli,svr}
    socket_.set_option(
        boost::asio::socket_base::send_buffer_size(std::min(opts_.TX_BUF_SIZE_MIN, opts_.tx_buf_size_)), ec);
    if (ec) {
        STREAM_WARNING("tx_buf_size_ fail: %s", ec.message().c_str());
    }
    // socket_.set_option(
    //     boost::asio::socket_base::receive_buffer_size(std::min(opts_.RX_BUF_SIZE_MIN, opts_.rx_buf_size_)), ec);
    // if (ec) {
    //     STREAM_WARNING("rx_buf_size_ fail: %s", ec.message().c_str());
    // }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamUdpcli::CloseSocket()
{
    STREAM_TRACE("CloseSocket");
    boost::system::error_code ec;

    // Cancel pending async_read()s (and any other operation)
    if (socket_.is_open()) {
        socket_.cancel(ec);
        if (ec) {
            STREAM_WARNING("socket cancel: %s", ec.message().c_str());
        }
    }

    // Close handle
    if (socket_.is_open()) {
        socket_.close(ec);
        if (ec) {
            STREAM_WARNING("socket close: %s", ec.message().c_str());
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamUdpcli::DoWrite()
{
    // Get data from the write queue_ into the tx buffer and fire the asio write
    std::unique_lock<std::mutex> lock(mutex_);
    tx_buf_size_ = std::min(write_queue_.Used(), sizeof(tx_buf_data_));
    tx_buf_sent_ = 0;
    if (tx_buf_size_ > 0) {
        STREAM_TRACE("DoWrite %" PRIuMAX, tx_buf_size_.load());
        write_queue_.Read(tx_buf_data_, tx_buf_size_);
        socket_.async_send(boost::asio::buffer(tx_buf_data_, tx_buf_size_),
            std::bind(&StreamUdpcli::OnWrite, this, std::placeholders::_1, std::placeholders::_2));
    } else {
        STREAM_TRACE("DoWrite done");
        tx_ongoing_ = false;
        NotifyTxDone();
    }
}

void StreamUdpcli::OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    STREAM_TRACE("OnWrite %" PRIuMAX "/%" PRIuMAX " %s", bytes_transferred, tx_buf_size_.load(), ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Error writing
    // Ignore ECONNREFUSED (see udp(7)). For example for a destination to localhost "it" somehow knows if something is
    // listening or not in which case a first async_send() works but a second one gives this error with
    // bytes_transferred=0. We do another attempt at async_send(), which should succeed. This then repeats until
    // something is listening again. async_send_to() behaves the same.
    else if (ec && (ec != boost::asio::error::connection_refused)) {
        SetStateError(StreamError::CONN_LOST, "write: " + ec.message());
        CloseSocket();
    }
    // Write successful, check if there's more to write
    else {
        // Data was not fully sent, some is left in the tx buffer
        tx_buf_sent_ += bytes_transferred;
        if (tx_buf_sent_ < tx_buf_size_) {
            socket_.async_send(boost::asio::buffer(&tx_buf_data_[tx_buf_sent_], tx_buf_size_ - tx_buf_sent_),
                std::bind(&StreamUdpcli::OnWrite, this, std::placeholders::_1, std::placeholders::_2));
        }
        // Everything sent, but check if there's new data to write
        else {
            tx_buf_size_ = 0;
            tx_buf_sent_ = 0;
            DoWrite();
        }
    }
}

bool StreamUdpcli::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_ongoing_));
    UNUSED(size);

    // Initiate the write in the io context (thread), unless a write is already ongoing
    if (!tx_ongoing_) {
        tx_ongoing_ = true;
        boost::asio::post(ctx_, std::bind(&StreamUdpcli::DoWrite, this));
    }

    return true;
}

/* ****************************************************************************************************************** */

/*static*/ std::unique_ptr<StreamOptsUdpsvr> StreamOptsUdpsvr::FromPath(
    const std::string& path, std::vector<std::string>& errors)
{
    StreamOptsUdpsvr opts;

    bool ok = true;

    // "[<host>]:<port>"
    if (!MatchHostPortPath(path, opts.host_, opts.port_, opts.ipv6_, false) || (opts.port_ < PORT_MIN) ||
        (opts.port_ > PORT_MAX)) {
        ok = false;
        errors.push_back("bad <host> or <port>");
    }

    opts.path_ = HostPortStr(opts.host_, opts.port_, opts.ipv6_);

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsUdpsvr>(opts);
}

/* ****************************************************************************************************************** */

StreamUdpsvr::StreamUdpsvr(const StreamOptsUdpsvr& opts) /* clang-format off */ :
    StreamBase(opts, opts_),
    opts_          { opts },
    thread_        { opts_.name_, std::bind(&StreamUdpsvr::Worker, this) }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();
}

StreamUdpsvr::~StreamUdpsvr()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

StreamUdpsvr::Server::Server(const std::string& name, boost::asio::io_context& ctx) /* clang-format off */ :
    name_     { name },
    socket_   { ctx },
    rx_buf_   { boost::asio::buffer(rx_buf_data_) }  // clang-format on
{
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamUdpsvr::Start()
{
    STREAM_TRACE("Start");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }
    if (!thread_.Start()) {
        return false;
    }

    // Find server endpoint(s)
    SetStateConnecting("resolve");
    std::string error;
    const auto endpoints = ResolveUdpEndpoints(opts_.host_, opts_.port_, opts_.ipv6_, error);
    if (endpoints.empty()) {
        SetStateError(StreamError::RESOLVE_FAIL, error);
        Stop();
        return false;
    }

    // Open server socket(s)
    SetStateConnecting("listen");
    for (auto& endpoint : endpoints) {
        const std::string name = HostPortStr(endpoint);
        STREAM_DEBUG("Listen %s", name.c_str());
        auto server = std::make_unique<Server>(name, ctx_);

        boost::system::error_code ec;

        // Create socket
        server->socket_.open(endpoint.protocol(), ec);
        if (ec) {
            error = " open: " + ec.message();
            break;
        }

        // Set socket options
        server->socket_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec) {
            error = " reuse_address: " + ec.message();
            break;
        }
        if (endpoint.address().is_v6()) {
            server->socket_.set_option(boost::asio::ip::v6_only(true), ec);
            if (ec) {
                error = " v6_only: " + ec.message();
                break;
            }
        }

        // Bind socket to address:port
        server->socket_.bind(endpoint, ec);
        if (ec) {
            error = " bind: " + ec.message();
            break;
        }

        // More socket options
        // @todo Not sure we do the multicast stuff right. See libs/asio/example/cpp11/multicast/receiver.cpp
        if (endpoint.address().is_multicast()) {
            DEBUG("multicast!");
            server->socket_.set_option(boost::asio::ip::multicast::join_group(endpoint.address()), ec);
            if (ec) {
                error = " multicast: " + ec.message();
                break;
            }
        }

        servers_.emplace_back(std::move(server));
    }

    if (!error.empty()) {
        SetStateError(StreamError::CONNECT_FAIL, error);
        Stop();
        return false;
    }

    SetStateConnected();

    // Start reading
    for (auto& server : servers_) {
        DoRead(server.get());
    }

    return true;
}

void StreamUdpsvr::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    UNUSED(timeout);
    for (auto& server : servers_) {
        CloseServer(server.get());
    }
    servers_.clear();
    ctx_.stop();
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamUdpsvr::Worker()
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

void StreamUdpsvr::ConfigServer(Server* server)
{
    STREAM_TRACE("ConfigServer %s", server->name_.c_str());
    boost::system::error_code ec;
    server->socket_.set_option(boost::asio::ip::tcp::no_delay(true), ec);
    if (ec) {
        STREAM_WARNING("no_delay fail: %s", ec.message().c_str());
    }
    // @todo good? bad? s.a. Stream{Tcp,Udp,Ntrip}{cli,svr}
    // server->socket_.set_option(
    //     boost::asio::socket_base::send_buffer_size(std::min(opts_.TX_BUF_SIZE_MIN, opts_.tx_buf_size_)), ec);
    // if (ec) {
    //     STREAM_WARNING("tx_buf_size_ fail: %s", ec.message().c_str());
    // }
    server->socket_.set_option(
        boost::asio::socket_base::receive_buffer_size(std::min(opts_.RX_BUF_SIZE_MIN, opts_.rx_buf_size_)), ec);
    if (ec) {
        STREAM_WARNING("rx_buf_size_ fail: %s", ec.message().c_str());
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamUdpsvr::CloseServer(Server* server)
{
    STREAM_TRACE("CloseServer %s", server->name_.c_str());
    boost::system::error_code ec;

    // Cancel pending async_read()s (and any other operation)
    if (server->socket_.is_open()) {
        server->socket_.cancel(ec);
        if (ec) {
            STREAM_WARNING("socket cancel: %s", ec.message().c_str());
        }
    }

    // Close handle
    if (server->socket_.is_open()) {
        server->socket_.close(ec);
        if (ec) {
            STREAM_WARNING("socket close: %s", ec.message().c_str());
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamUdpsvr::DoRead(Server* server)
{
    STREAM_TRACE("DoRead %s", server->name_.c_str());
    // server->socket_.async_receive(server->rx_buf_,
    //     std::bind(&StreamUdpsvr::OnRead, this, std::placeholders::_1, std::placeholders::_2, server));
    server->socket_.async_receive_from(server->rx_buf_, std::ref(server->sender_),
        std::bind(&StreamUdpsvr::OnRead, this, std::placeholders::_1, std::placeholders::_2, server));
}

void StreamUdpsvr::OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred, Server* server)
{
    STREAM_TRACE("OnRead %s %s %" PRIuMAX " %s", server->name_.c_str(), HostPortStr(server->sender_).c_str(),
        bytes_transferred, ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Error reading
    else if (ec) {
        SetStateError(StreamError::CONN_LOST, "read: " + ec.message());
        CloseServer(server);
        // @todo and now what?! what errors can happen? boost::asio::error::message_size perhaps?
    }
    // Process incoming data, and read again
    else {
        // This feeds the data received from any client into StreamBase::read_parser_. This may lead to garbled
        // messsages... :-/ We could run per-client parsers here and make sure we only feed full messages to
        // ProcessRead(). However, how can we limit the number of such parser we would have to create?
        // @todo Ideas anyone?
        ProcessRead((const uint8_t*)server->rx_buf_.data(), bytes_transferred);
        DoRead(server);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

// This should not ever be called as the stream is RO
bool StreamUdpsvr::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_ongoing_));
    UNUSED(size);
    return false;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
