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
 * @brief Stream TCP servers
 */

/* LIBC/STL */
#include <algorithm>
#include <functional>
#include <utility>

/* EXTERNAL */
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/string.hpp>

/* PACKAGE */
#include "tcpservers.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

/*static*/ std::unique_ptr<StreamOptsTcpsvr> StreamOptsTcpsvr::FromPath(
    const std::string& path, std::vector<std::string>& errors)
{
    StreamOptsTcpsvr opts;

    bool ok = true;

    // [<host>]:<port>
    if (!MatchHostPortPath(path, opts.host_, opts.port_, opts.ipv6_, false) || (opts.port_ < PORT_MIN) ||
        (opts.port_ > PORT_MAX)) {
        ok = false;
        errors.push_back("bad <host> or <port>");
    }

    opts.path_ = HostPortStr(opts.host_, opts.port_, opts.ipv6_);

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsTcpsvr>(opts);
}

/* ****************************************************************************************************************** */

StreamTcpsvr::StreamTcpsvr(const StreamOptsTcpsvr& opts) /* clang-format off */ :
    StreamBase(opts, opts_),
    opts_     { opts },
    thread_   { opts_.name_, std::bind(&StreamTcpsvr::Worker, this) }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();

    clients_.reserve(opts_.max_clients_);
}

StreamTcpsvr::~StreamTcpsvr()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

StreamTcpsvr::Server::Server(const std::string& name, boost::asio::io_context& ctx) /* clang-format off */ :
    name_       { name },
    acceptor_   { ctx }  // clang-format on
{
}

// ---------------------------------------------------------------------------------------------------------------------

StreamTcpsvr::Client::Client(const std::string& name, boost::asio::ip::tcp::socket socket) /* clang-format off */ :
    name_          { name },
    socket_        { std::move(socket) },
    rx_buf_        { boost::asio::buffer(rx_buf_data_) },
    tx_buf_size_   { 0 },
    tx_buf_sent_   { 0 }  // clang-format on
{
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamTcpsvr::Start()
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
    const auto endpoints = ResolveTcpEndpoints(opts_.host_, opts_.port_, opts_.ipv6_, error);
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

        if (endpoint.address().is_multicast()) {
            error = "cannot use multicast addr";
            break;
        }

        boost::system::error_code ec;

        // Create socket
        server->acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            error = " open: " + ec.message();
            break;
        }

        // Set socket options
        server->acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec) {
            error = " reuse_address: " + ec.message();
            break;
        }
        if (endpoint.address().is_v6()) {
            server->acceptor_.set_option(boost::asio::ip::v6_only(true), ec);
            if (ec) {
                error = " v6_only: " + ec.message();
                break;
            }
        }

        // Bind socket to address:port
        server->acceptor_.bind(endpoint, ec);
        if (ec) {
            error = " bind: " + ec.message();
            break;
        }

        // Start listening
        server->acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec) {
            error = " listen: " + ec.message();
            break;
        }

        servers_.emplace_back(std::move(server));
    }

    if (!error.empty()) {
        SetStateError(StreamError::DEVICE_FAIL, error);
        Stop();
        return false;
    }

    // Start accepting connections
    UpdateStateConnected();
    for (auto& server : servers_) {
        DoAccept(server.get());
    }

    return true;
}

void StreamTcpsvr::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);

    if (!clients_.empty()) {
        StopWaitTxDone(timeout);
    }

    for (auto& server : servers_) {
        CloseServer(server.get());
    }
    servers_.clear();

    for (auto& entry : clients_) {
        CloseClient(entry.second.get(), false);
    }
    clients_.clear();

    ctx_.stop();

    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamTcpsvr::Worker()
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

void StreamTcpsvr::UpdateStateConnected()
{
    SetStateConnected(string::Sprintf("%" PRIuMAX "/%" PRIuMAX " clients", clients_.size(), opts_.max_clients_));
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpsvr::ConfigClient(Client* client)
{
    STREAM_TRACE("ConfigClient %s", client->name_.c_str());
    boost::system::error_code ec;
    client->socket_.set_option(boost::asio::ip::tcp::no_delay(true), ec);
    if (ec) {
        STREAM_WARNING("%s no_delay fail: %s", client->name_.c_str(), ec.message().c_str());
    }
    // @todo good? bad? s.a. Stream{Tcp,Udp,Ntrip}{cli,svr}
    client->socket_.set_option(
        boost::asio::socket_base::send_buffer_size(std::min(opts_.TX_BUF_SIZE_MIN, opts_.tx_buf_size_)), ec);
    if (ec) {
        STREAM_WARNING("%s tx_buf_size_ fail: %s", client->name_.c_str(), ec.message().c_str());
    }
    client->socket_.set_option(
        boost::asio::socket_base::receive_buffer_size(std::min(opts_.RX_BUF_SIZE_MIN, opts_.rx_buf_size_)), ec);
    if (ec) {
        STREAM_WARNING("%s rx_buf_size_ fail: %s", client->name_.c_str(), ec.message().c_str());
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpsvr::CloseClient(Client* client, const bool remove)
{
    STREAM_TRACE("CloseSocket %s", client->name_.c_str());
    boost::system::error_code ec;

    // Cancel pending async_read()s (and any other operation)
    client->socket_.cancel(ec);
    if (ec) {
        STREAM_WARNING("%s socket cancel: %s", client->name_.c_str(), ec.message().c_str());
    }

    // Actively shutdown connection
    client->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec) {
        STREAM_WARNING("%s socket shutdown: %s", client->name_.c_str(), ec.message().c_str());
    }

    // Close handle
    client->socket_.close(ec);
    if (ec) {
        STREAM_WARNING("%s socket close: %s", client->name_.c_str(), ec.message().c_str());
    }

    if (remove) {
        std::unique_lock<std::mutex> lock(mutex_);
        clients_.erase(client->name_);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpsvr::CloseServer(Server* server)
{
    STREAM_TRACE("CloseServer %s", server->name_.c_str());
    boost::system::error_code ec;
    server->acceptor_.cancel(ec);
    if (ec) {
        STREAM_WARNING("acceptor cancel: %s", ec.message().c_str());
    }
    server->acceptor_.close(ec);
    if (ec) {
        STREAM_WARNING("acceptor close: %s", ec.message().c_str());
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpsvr::DoAccept(Server* server)
{
    STREAM_TRACE("DoAccept %s", server->name_.c_str());
    server->acceptor_.async_accept(boost::asio::make_strand(ctx_),
        std::bind(&StreamTcpsvr::OnAccept, this, std::placeholders::_1, std::placeholders::_2, server));
}

void StreamTcpsvr::OnAccept(const boost::system::error_code& ec, boost::asio::ip::tcp::socket peer, Server* server)
{
    STREAM_TRACE("OnAccept %s", ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Accept failed
    // @todo Should we do something? Maybe we should handle at least some the possible errors?
    else if (ec) {
        STREAM_WARNING("%s accept fail: %s", server->name_.c_str(), ec.message().c_str());
    }
    // Create client session
    else {
        const std::string name = HostPortStr(peer.remote_endpoint());
        auto client = std::make_unique<Client>(name, std::move(peer));

        // Limit maximum number of clients, and prevent client with same name (you never know...)
        if ((clients_.size() >= opts_.max_clients_) || (clients_.find(name) != clients_.end())) {
            STREAM_WARNING("Client %s deny", name.c_str());
            CloseClient(client.get());
        }
        // Use client
        else {
            STREAM_INFO("Client %s connect", name.c_str());
            ConfigClient(client.get());
            DoRead(client.get());
            std::unique_lock<std::mutex> lock(mutex_);
            clients_.emplace(name, std::move(client));
        }

        UpdateStateConnected();

        DoAccept(server);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpsvr::DoRead(Client* client)
{
    STREAM_TRACE("DoRead %s", client->name_.c_str());
    client->socket_.async_read_some(
        client->rx_buf_, std::bind(&StreamTcpsvr::OnRead, this, std::placeholders::_1, std::placeholders::_2, client));
}

void StreamTcpsvr::OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred, Client* client)
{
    STREAM_TRACE("OnRead %s %" PRIuMAX " %s", client->name_.c_str(), bytes_transferred, ec.message().c_str());
    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // E.g. boost::asio::error::eof (= remote closed connection)
    else if (ec) {
        STREAM_INFO("Client %s disconnect: %s", client->name_.c_str(), ec.message().c_str());
        CloseClient(client);
        UpdateStateConnected();
    }
    // Process incoming data, and read again
    else {
        // We'll run the data to the parser, so that we can process messages from multiple clients.
        // @todo Messages are parsed again by ProcessRead(). Can we do better?
        if (!client->parser_.Add((const uint8_t*)client->rx_buf_.data(), bytes_transferred)) {
            STREAM_WARNING_THR(1000, "%s rx parser ovfl", client->name_.c_str());  // we don't expect this ever
            client->parser_.Reset();
            client->parser_.Add((const uint8_t*)client->rx_buf_.data(), bytes_transferred);
        }
        parser::ParserMsg msg;
        while (client->parser_.Process(msg)) {
            ProcessRead(msg.Data(), msg.data_.size());
        }

        DoRead(client);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamTcpsvr::DoWrite()
{
    std::unique_lock<std::mutex> lock(mutex_);
    // We'll have to wait until all clients are done because we use a common tx_buf_data_.
    bool all_done = true;
    for (auto& cand : clients_) {
        if (cand.second->tx_buf_size_ > 0) {
            all_done = false;
            break;
        }
    }
    STREAM_TRACE("DoWrite all_done=%s", string::ToStr(all_done));
    if (!all_done) {
        return;
    }
    // We'll be called by every client on async_write_some() done so we can continue when the last client
    // has completed

    // Get data from the write queue into the tx buffer and fire the asio write
    const std::size_t tx_buf_size = std::min(write_queue_.Used(), sizeof(tx_buf_data_));
    if (tx_buf_size > 0) {
        STREAM_TRACE("DoWrite %" PRIuMAX, tx_buf_size);
        write_queue_.Read(tx_buf_data_, tx_buf_size);
        for (auto& entry : clients_) {
            auto& client = entry.second;
            client->tx_buf_size_ = tx_buf_size;
            client->tx_buf_sent_ = 0;
            client->socket_.async_write_some(boost::asio::buffer(tx_buf_data_, client->tx_buf_size_),
                std::bind(&StreamTcpsvr::OnWrite, this, std::placeholders::_1, std::placeholders::_2, client.get()));
        }

    } else {
        STREAM_TRACE("DoWrite done");
        tx_ongoing_ = false;
        NotifyTxDone();
    }
}

void StreamTcpsvr::OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred, Client* client)
{
    STREAM_TRACE("OnWrite %" PRIuMAX "/%" PRIuMAX " %s", bytes_transferred, client->tx_buf_size_, ec.message().c_str());

    // We got cancelled, do nothing
    if (ec == boost::asio::error::operation_aborted) {
    }
    // Error writing
    else if (ec) {
        STREAM_INFO("Client %s disconnect: %s", client->name_.c_str(), ec.message().c_str());
        CloseClient(client);
        UpdateStateConnected();
    }
    // Write successful, check if there's more to write
    else {
        // Data was not fully sent, some is left in the tx buffer
        client->tx_buf_sent_ += bytes_transferred;
        if (client->tx_buf_sent_ < client->tx_buf_size_) {
            client->socket_.async_write_some(
                boost::asio::buffer(&tx_buf_data_[client->tx_buf_sent_], client->tx_buf_size_ - client->tx_buf_sent_),
                std::bind(&StreamTcpsvr::OnWrite, this, std::placeholders::_1, std::placeholders::_2, client));
        }
        // Everything sent, but check if there's new data to write
        else {
            client->tx_buf_size_ = 0;
            client->tx_buf_sent_ = 0;
            DoWrite();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamTcpsvr::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_ongoing_));
    UNUSED(size);

    // Happily discard all data if we have no clients
    if (clients_.empty()) {
        write_queue_.Reset();
        return true;
    }

    // Initiate the write in the io context (thread), unless a write is already ongoing
    if (!tx_ongoing_) {
        tx_ongoing_ = true;
        boost::asio::post(ctx_, std::bind(&StreamTcpsvr::DoWrite, this));
    }

    return true;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
