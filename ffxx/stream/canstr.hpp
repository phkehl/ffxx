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
#ifndef __FFXX_STREAM_CANSTR_HPP__
#define __FFXX_STREAM_CANSTR_HPP__

/* LIBC/STL */

/* EXTERNAL */
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/basic_stream_descriptor.hpp>
#include <fpsdk_common/can.hpp>
#include <fpsdk_common/thread.hpp>

/* PACKAGE */
#include "base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

// canstr stream options
struct StreamOptsCanstr : public StreamOpts  // clang-format off
{
    static std::unique_ptr<StreamOptsCanstr> FromPath(const std::string& path, std::vector<std::string>& errors);

    std::string device_;
    uint32_t canid_in_ = 0;
    uint32_t canid_out_ = 0;
    bool eff_ = false;
    bool fd_ = false;
    bool brs_ = false;
};  // clang-format on

// filein stream implementation
class StreamCanstr : public StreamBase
{
   public:
    StreamCanstr(const StreamOptsCanstr& opts);
    ~StreamCanstr();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   private:
    StreamOptsCanstr opts_;
    can::RawCan can_;
    boost::asio::io_context ctx_;
    boost::asio::posix::basic_stream_descriptor<> stream_;
    uint8_t rx_buf_data_[sizeof(can::CanFrame)];
    boost::asio::mutable_buffer rx_buf_;
    can::CanFrame tx_frame_;

    thread::Thread thread_;
    bool Worker();

    bool OpenCan();
    void CloseCan();

    void DoRead();
    void OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred);

    void DoWrite();
    void OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred);

    bool ProcessWrite(const std::size_t size) final;
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_CANSTR_HPP__
