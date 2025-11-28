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
#ifndef __FFXX_STREAM_BASE_HPP__
#define __FFXX_STREAM_BASE_HPP__

/* LIBC/STL */
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

/* EXTERNAL */
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/parser.hpp>
#include <fpsdk_common/thread.hpp>
#include <fpsdk_common/types.hpp>
#include <fpsdk_common/utils.hpp>

/* PACKAGE */
#include "../stream.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

#define STREAM_TRACE_ENABLED 0

// clang-format off
#if STREAM_TRACE_ENABLED
#  define STREAM_TRACE(...) StreamLoggingPrint(fpsdk::common::logging::LoggingLevel::TRACE, 0, __VA_ARGS__)
#  define STREAM_TRACE_WARNING() STREAM_WARNING("STREAM_TRACE_ENABLED")
#else
#  define STREAM_TRACE(...)      /* nothing */
#  define STREAM_TRACE_WARNING() /* nothing */
#endif
#define STREAM_DEBUG(...)  if (!opts_.quiet_) { StreamLoggingPrint(fpsdk::common::logging::LoggingLevel::DEBUG,   0, __VA_ARGS__); }
#define STREAM_INFO(...)   if (!opts_.quiet_) { StreamLoggingPrint(fpsdk::common::logging::LoggingLevel::INFO,    0, __VA_ARGS__); }
#define STREAM_WARNING(...)                     StreamLoggingPrint(fpsdk::common::logging::LoggingLevel::WARNING, 0, __VA_ARGS__)
#define STREAM_WARNING_THR(_millis_, ...) do { \
    static uint32_t __last = 0; const uint32_t __now = ::fpsdk::common::time::GetMillis(); \
    static uint32_t __repeat = 0; __repeat++; \
    if ((__now - __last) >= (_millis_)) { __last = __now; \
        StreamLoggingPrint(fpsdk::common::logging::LoggingLevel::WARNING, __repeat, __VA_ARGS__);\
        __repeat = 0; } } while (0)
// clang-format on

// ---------------------------------------------------------------------------------------------------------------------

// Base class for streams
class StreamBase : public Stream, private types::NoCopyNoMove
{
   public:
    StreamBase(const StreamOpts& opts_init, const StreamOpts& opts_final);
    virtual ~StreamBase();

    bool Read(parser::ParserMsg& msg, const uint32_t timeout = 0) final;
    bool Write(const uint8_t* data, const std::size_t size, const uint32_t timeout = 0) final;
    bool Write(const std::vector<uint8_t> data, const uint32_t timeout = 0) final;
    bool Wait(const uint32_t millis) final;

    uint32_t GetBaudrate() override;
    bool SetBaudrate(const uint32_t baudrate) override;
    bool Autobaud(const AutobaudMode mode) override;

    const StreamOpts GetOpts() final;
    StreamState GetState() const final;
    StreamError GetError() const final;
    StreamMode GetMode() const final;
    StreamType GetType() const final;
    std::string GetInfo() final;
    parser::ParserStats GetParserStats() final;
    void AddStateObserver(StateObserver observer) final;
    void AddReadObserver(ReadObserver observer) final;

   protected:
    const StreamOpts& opts_;  // Stream options, see base.cpp for explanation of const&

    void SetStateClosed();
    void SetStateConnecting(const std::string& info = "");
    void SetStateConnected(const std::string& info = "");
    void SetStateError(const StreamError error, const std::string& info = "");

    // io thread -> main, size <= parser::MAX_ADD_SIZE
    void ProcessRead(const uint8_t* data, const std::size_t size);
    // main -> io thread
    virtual bool ProcessWrite(const std::size_t) = 0;
    utils::CircularBuffer write_queue_;
    std::mutex mutex_;
    std::atomic<bool> tx_ongoing_ = false;
    void NotifyTxDone();
    void StopWaitTxDone(const uint32_t timeout);

    void StreamLoggingPrint(const fpsdk::common::logging::LoggingLevel level, const std::size_t repeat, const char* fmt,
        ...) const PRINTF_ATTR(4);

   private:
    std::atomic<StreamState> state_ = StreamState::CLOSED;
    bool state_set_ = false;
    std::atomic<StreamError> error_ = StreamError::NONE;
    std::string info_;
    std::vector<StateObserver> state_observers_;
    std::vector<ReadObserver> rxready_observers_;
    parser::Parser read_parser_;
    thread::BinarySemaphore read_sem_;
    thread::BinarySemaphore write_sem_;
    std::queue<parser::ParserMsg> read_queue_;

    void SetStateEx(const StreamState state, const StreamError error, const std::string& info = "");
};

// ---------------------------------------------------------------------------------------------------------------------

// Match [<host>:]<port> paths, use [addr] for IPv6
bool MatchHostPortPath(
    const std::string& path, std::string& host, uint16_t& port, bool& ipv6, bool require_host = true);

std::vector<boost::asio::ip::tcp::endpoint> ResolveTcpEndpoints(
    const std::string& host, const uint16_t port, const bool want_ipv6, std::string& error);

std::vector<boost::asio::ip::udp::endpoint> ResolveUdpEndpoints(
    const std::string& host, const uint16_t port, const bool want_ipv6, std::string& error);

std::string HostPortStr(const std::string& host, const uint16_t port, const bool ipv6);
std::string HostPortStr(const boost::asio::ip::tcp::endpoint& endpoint);
std::string HostPortStr(const boost::asio::ip::udp::endpoint& endpoint);

bool CredentialsToAuth(const std::string& credentials, std::string& auth_plain, std::string& auth_base64);

bool StrToAutobaudMode(const std::string& str, AutobaudMode& mode);

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_BASE_HPP__
