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
 * @brief Stream UDP server and client
 */
#ifndef __FFXX_STREAM_UDPCLI_HPP__
#define __FFXX_STREAM_UDPCLI_HPP__

/* LIBC/STL */
#include <cstdint>

/* EXTERNAL */
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>
#include <fpsdk_common/thread.hpp>
#include <fpsdk_common/types.hpp>

/* PACKAGE */
#include "base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

// UDP client stream options
struct StreamOptsUdpcli : public StreamOpts
{
    static std::unique_ptr<StreamOptsUdpcli> FromPath(const std::string& path, std::vector<std::string>& errors);

    std::string host_;                           // Host
    bool ipv6_ = false;                          // host_ is IPv6
    uint16_t port_ = 0;                          // Port
    static constexpr uint16_t PORT_MIN = 1;      // Minimal value for port_
    static constexpr uint16_t PORT_MAX = 65535;  // Maximal value for port_
};

// UDP client stream implementation
class StreamUdpcli : public StreamBase
{
   public:
    StreamUdpcli(const StreamOptsUdpcli& opts);
    ~StreamUdpcli();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   private:
    StreamOptsUdpcli opts_;
    // clang-format off
    boost::asio::io_context                      ctx_;
    boost::asio::ip::udp::resolver               resolver_;
    boost::asio::ip::udp::resolver::results_type resolve_results_;
    boost::asio::ip::udp::endpoint               endpoint_;
    boost::asio::ip::udp::socket                 socket_;
    uint8_t                                      tx_buf_data_[16 * 1024];
    std::atomic<std::size_t>                     tx_buf_size_;
    std::size_t                                  tx_buf_sent_;
    // clang-format on

    thread::Thread thread_;
    bool Worker();

    void ConfigSocket();
    void CloseSocket();

    void DoWrite();
    void OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred);

    bool ProcessWrite(const std::size_t size) final;
};

// ---------------------------------------------------------------------------------------------------------------------

// UDP client stream options
struct StreamOptsUdpsvr : public StreamOpts
{
    static std::unique_ptr<StreamOptsUdpsvr> FromPath(const std::string& path, std::vector<std::string>& errors);

    std::string host_;                           // Host
    bool ipv6_ = false;                          // host_ is IPv6
    uint16_t port_ = 0;                          // Port
    static constexpr uint16_t PORT_MIN = 1;      // Minimal value for port_
    static constexpr uint16_t PORT_MAX = 65535;  // Maximal value for port_
};

// UDP client stream implementation
class StreamUdpsvr : public StreamBase
{
   public:
    StreamUdpsvr(const StreamOptsUdpsvr& opts);
    ~StreamUdpsvr();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   private:
    StreamOptsUdpsvr opts_;

    struct Server : private types::NoCopyNoMove
    {
        Server(const std::string& name, boost::asio::io_context& ctx);
        std::string name_;
        boost::asio::ip::udp::socket socket_;
        uint8_t rx_buf_data_[parser::MAX_ADD_SIZE];
        boost::asio::mutable_buffer rx_buf_;
        boost::asio::ip::udp::endpoint sender_;
    };

    boost::asio::io_context ctx_;
    std::vector<std::unique_ptr<Server>> servers_;

    thread::Thread thread_;
    bool Worker();

    void ConfigServer(Server* server);
    void CloseServer(Server* server);

    void DoRead(Server* server);
    void OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred, Server* server);

    bool ProcessWrite(const std::size_t size) final;
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_UDPCLI_HPP__
