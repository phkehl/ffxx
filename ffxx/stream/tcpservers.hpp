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
#ifndef __FFXX_STREAM_TCPSERVERS_HPP__
#define __FFXX_STREAM_TCPSERVERS_HPP__

/* LIBC/STL */
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/* EXTERNAL */
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <fpsdk_common/parser.hpp>
#include <fpsdk_common/thread.hpp>
#include <fpsdk_common/types.hpp>

/* PACKAGE */
#include "base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

// TCP client stream options
struct StreamOptsTcpsvr : public StreamOpts
{
    static std::unique_ptr<StreamOptsTcpsvr> FromPath(const std::string& path, std::vector<std::string>& errors);

    std::string host_;                           // Host
    bool ipv6_ = false;                          // host_ is IPv6
    uint16_t port_ = 0;                          // Port
    static constexpr uint16_t PORT_MIN = 1;      // Minimal value for port_
    static constexpr uint16_t PORT_MAX = 65535;  // Maximal value for port_
};

// TCP client stream implementation
class StreamTcpsvr : public StreamBase
{
   public:
    StreamTcpsvr(const StreamOptsTcpsvr& opts);
    ~StreamTcpsvr();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   private:
    StreamOptsTcpsvr opts_;

    struct Server : private types::NoCopyNoMove
    {
        Server(const std::string& name, boost::asio::io_context& ctx);
        std::string name_;
        boost::asio::ip::tcp::acceptor acceptor_;
    };
    struct Client : private types::NoCopyNoMove
    {
        Client(const std::string& name, boost::asio::ip::tcp::socket socket);
        // clang-format off
        std::string                  name_;
        boost::asio::ip::tcp::socket socket_;
        parser::Parser               parser_;
        uint8_t                      rx_buf_data_[parser::MAX_ADD_SIZE];
        boost::asio::mutable_buffer  rx_buf_;
        std::size_t                  tx_buf_size_;
        std::size_t                  tx_buf_sent_;
        // clang-format on
    };

    boost::asio::io_context ctx_;
    std::vector<std::unique_ptr<Server>> servers_;
    std::unordered_map<std::string, std::unique_ptr<Client>> clients_;
    uint8_t tx_buf_data_[16 * 1024];  // Common for all clients_

    thread::Thread thread_;
    bool Worker();

    void UpdateStateConnected();

    void DoAccept(Server* server);
    void OnAccept(const boost::system::error_code& ec, boost::asio::ip::tcp::socket peer, Server* server);

    void ConfigClient(Client* client);
    void CloseClient(Client* client, const bool remove = true);
    void CloseServer(Server* server);

    void DoRead(Client* client);
    void OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred, Client* client);

    void DoWrite();
    void OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred, Client* client);

    void ProcessWriteAgain(const boost::system::error_code& ec);
    bool ProcessWrite(const std::size_t size) final;
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_TCPSERVERS_HPP__
