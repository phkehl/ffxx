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
#ifndef __FFXX_STREAM_SERIAL_HPP__
#define __FFXX_STREAM_SERIAL_HPP__

/* LIBC/STL */
#include <cstdint>

/* EXTERNAL */
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <fpsdk_common/thread.hpp>

/* PACKAGE */
#include "autobauder.hpp"
#include "base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

// Serial client stream options
struct StreamOptsSerial : public StreamOpts
{
    static std::unique_ptr<StreamOptsSerial> FromPath(const std::string& path, std::vector<std::string>& errors);

    // clang-format off
    std::string      device_;                                 // Device
    uint32_t         baudrate_ = 0;                           // Baudrate
    SerialMode       serial_mode_ = SerialMode::UNSPECIFIED;  // Serial port mode
    SerialFlow       serial_flow_ = SerialFlow::UNSPECIFIED;  // Serial port flow control
    AutobaudMode autobaud_    = AutobaudMode::NONE;
    // clang-format on

    void UpdatePath();
};

// Serial stream implementation
class StreamSerial : public StreamBase
{
   public:
    StreamSerial(const StreamOptsSerial& opts);
    ~StreamSerial();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

    uint32_t GetBaudrate() final;
    bool SetBaudrate(const uint32_t baudrate) override final;
    bool Autobaud(const AutobaudMode mode) override final;

   private:
    // clang-format off
    StreamOptsSerial              opts_;
    boost::asio::io_context       ctx_;
    boost::asio::serial_port      port_;
    bool                          port_open_;
    boost::asio::steady_timer     timer_;
    uint8_t                       rx_buf_data_[parser::MAX_ADD_SIZE];
    boost::asio::mutable_buffer   rx_buf_;
    uint8_t                       tx_buf_data_[16 * 1024];
    std::size_t                   tx_buf_size_;
    std::size_t                   tx_buf_sent_;
    // clang-format on

    thread::Thread thread_;
    bool Worker();

    bool OpenPort();
    void ClosePort();

    void DoRetry();
    void OnRetry(const boost::system::error_code& ec);

    void DoOpen();
    void OnOpen(const boost::system::error_code& ec);

    friend Autobauder<StreamSerial>;  // I have no better idea.. :-/
    Autobauder<StreamSerial> autobauder_;

    void DoInactTimeout();
    void OnInactTimeout(const boost::system::error_code& ec);

    void DoRead();
    void OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred);

    void DoWrite();
    void OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred);

    bool ProcessWrite(const std::size_t size) final;
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_SERIAL_HPP__
