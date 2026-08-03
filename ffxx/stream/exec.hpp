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
#ifndef __FFXX_STREAM_EXEC_HPP__
#define __FFXX_STREAM_EXEC_HPP__

/* LIBC/STL */
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/* EXTERNAL */
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <fpsdk_common/parser/types.hpp>
#include <fpsdk_common/thread.hpp>

/* PACKAGE */
#include "base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

// EXEC stream options
struct StreamOptsExec : public StreamOpts
{
    static std::unique_ptr<StreamOptsExec> FromPath(const std::string& path, std::vector<std::string>& errors);

    std::vector<std::string> argv_;
};

// EXEC stream implementation
class StreamExec : public StreamBase
{
   public:
    StreamExec(const StreamOptsExec& opts);
    ~StreamExec();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   private:
    StreamOptsExec opts_;

    // clang-format off
    boost::asio::io_context                      ctx_;
    boost::asio::steady_timer                    timer_;
    std::unique_ptr<boost::process::async_pipe>  stdin_pipe_;
    std::unique_ptr<boost::process::async_pipe>  stdout_pipe_;
    std::unique_ptr<boost::process::async_pipe>  stderr_pipe_;
    std::unique_ptr<boost::process::child>       child_;

    uint8_t                                      rx_buf_data_[fpsdk::common::parser::MAX_ADD_SIZE];
    boost::asio::mutable_buffer                  rx_buf_;
    uint8_t                                      stderr_buf_data_[fpsdk::common::parser::MAX_ADD_SIZE];
    boost::asio::mutable_buffer                  stderr_buf_;
    uint8_t                                      tx_buf_data_[16 * 1024];
    std::size_t                                  tx_buf_size_;
    std::size_t                                  tx_buf_sent_;

    // clang-format on

    fpsdk::common::thread::Thread thread_;
    bool Worker();

    bool StartChild();
    void StopChild();

    void OnExit(const int status, const std::error_code& ec);

    void DoStart();

    void DoInactTimeout();
    void OnInactTimeout(const boost::system::error_code& ec);

    void DoRetry();
    void OnRetry(const boost::system::error_code& ec);

    void DoWrite();
    void OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred);

    void DoRead();
    void OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred);

    void DoStderr();
    void OnStderr(const boost::system::error_code& ec, std::size_t bytes_transferred);

    bool ProcessWrite(const std::size_t size) final;
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_EXEC_HPP__
