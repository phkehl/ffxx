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
#ifndef __FFXX_STREAM_AUTOBAUDER_HPP__
#define __FFXX_STREAM_AUTOBAUDER_HPP__

/* LIBC/STL */
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

/* EXTERNAL */
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <fpsdk_common/parser.hpp>
#include <fpsdk_common/string.hpp>

/* PACKAGE */
#include "base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

// clang-format off
#define AUTOBAUDER_DEBUG(...) if (!stream_.opts_.quiet_) { stream_.StreamLoggingPrint(fpsdk::common::logging::LoggingLevel::DEBUG,   0, __VA_ARGS__); }
#define AUTOBAUDER_WARNING(...)                            stream_.StreamLoggingPrint(fpsdk::common::logging::LoggingLevel::WARNING, 0, __VA_ARGS__)

#if STREAM_TRACE_ENABLED
#  define AUTOBAUDER_TRACE(...) stream_.StreamLoggingPrint(fpsdk::common::logging::LoggingLevel::TRACE, 0, __VA_ARGS__)
#else
#  define AUTOBAUDER_TRACE(...)      /* nothing */
#endif
// clang-format on

#define UBX_TRAINING_SEQUENCE 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
#define UBX_MON_VER_POLL 0xb5, 0x62, 0x0a, 0x04, 0x00, 0x00, 0x0e, 0x34
#define FP_B_VERSION_POLL 0x66, 0x21, 0xfd, 0x08, 0x00, 0x00, 0x00, 0x00, 0x70, 0x20, 0xe0, 0x49

class StreamSerial;
class StreamTelnet;

template <typename StreamT>
class Autobauder
{
    static_assert(std::is_same<StreamSerial, StreamT>::value || std::is_same<StreamTelnet, StreamT>::value,
        "Autobauder implemented only for StreamSerial and StreamTelnet");

   public:
    Autobauder(StreamT& stream) /* clang-format off */ :
        rx_buf_   { boost::asio::buffer(rx_buf_data_) },
        stream_   { stream },
        timer_    { stream_.ctx_ }  // clang-format on
    {
    }

    using Callback = std::function<void(bool success)>;

    bool Start(const AutobaudMode mode, Callback callback)
    {
        if (state_ != State::IDLE) {
            AUTOBAUDER_WARNING("autobaud already running");
            return false;
        }
        callback_ = callback;

        AUTOBAUDER_DEBUG("Autobaud %s", AutobaudModeStr(mode));

        steps_.clear();
        std::string expected_message;
        std::vector<uint8_t> send_data;
        switch (mode) {
            case AutobaudMode::NONE:
                break;
            case AutobaudMode::PASSIVE:
                break;
            case AutobaudMode::UBX:
                expected_message = "UBX-MON-VER";
                send_data = { UBX_TRAINING_SEQUENCE, UBX_MON_VER_POLL };
                break;
            case AutobaudMode::FP:
                expected_message = "FP_B-VERSION";
                break;
            case AutobaudMode::AUTO:
                send_data = { UBX_TRAINING_SEQUENCE, UBX_MON_VER_POLL, FP_B_VERSION_POLL };
                break;
        }

        // First pass: current baudrate, then all others (higher baudrates first)
        steps_.push_back({ stream_.opts_.baudrate_, send_data, expected_message, 500 });
        for (auto baudrate : boost::adaptors::reverse(StreamOpts::BAUDRATES)) {
            if (baudrate != stream_.opts_.baudrate_) {
                steps_.push_back({ baudrate, send_data, expected_message, 500 });
            }
        }

        // Second pass with a longer wait
        for (auto baudrate : boost::adaptors::reverse(StreamOpts::BAUDRATES)) {
            steps_.push_back({ baudrate, send_data, expected_message, 1500 });
        }

        mode_ = mode;
        step_ = steps_.cbegin() - 1;
        state_ = State::RUN;
        boost::asio::post(stream_.ctx_, std::bind(&Autobauder::Run, this));
        return true;
    }

   private:
    enum class State
    {
        IDLE,
        RUN,
        SUCCESS,
        FAILED
    };
    struct Step
    {
        uint32_t baudrate_ = 0;
        std::vector<uint8_t> poll_;
        std::string expected_;
        uint32_t timeout_ = 0;
    };
    // clang-format off
    uint8_t                     rx_buf_data_[parser::MAX_ADD_SIZE];
    boost::asio::mutable_buffer rx_buf_;
    parser::Parser              parser_;
    parser::ParserMsg           msg_;
    State                       state_ = State::IDLE;
    AutobaudMode                mode_ = AutobaudMode::NONE;
    StreamT&                    stream_;
    boost::asio::steady_timer   timer_;
    std::vector<Step>           steps_;
    typename std::vector<Step>::const_iterator step_;
    Callback                    callback_;
    // clang-format on

    void Run()
    {
        timer_.cancel();

        // Magic c++ conditional compilation...
        if constexpr (std::is_same_v<StreamT, StreamSerial>) {
            stream_.port_.cancel();
        }
        if constexpr (std::is_same_v<StreamT, StreamTelnet>) {
            stream_.RawSocket().cancel();
        }

        switch (state_) {
            case State::IDLE:
                break;
            case State::RUN:
                step_++;
                if ((step_ != steps_.cend()) && stream_.SetBaudrate(step_->baudrate_)) {
                    AUTOBAUDER_DEBUG("Autobauder::Run %" PRIu32 " %" PRIuMAX " %s", step_->baudrate_,
                        step_->poll_.size(), step_->expected_.c_str());
                    stream_.SetStateConnecting("autobaud " + string::StrToLower(AutobaudModeStr(mode_)) + " " +
                                               std::to_string(step_ - steps_.cbegin() + 1) + "/" +
                                               std::to_string(steps_.size()) + " " + std::to_string(step_->baudrate_) +
                                               " (" + std::to_string(step_->timeout_) + "ms)");
                    if (!step_->poll_.empty()) {
                        // Magic c++ conditional compilation...
                        if constexpr (std::is_same_v<StreamT, StreamSerial>) {
                            boost::asio::write(stream_.port_, boost::asio::buffer(step_->poll_));
                        }
                        if constexpr (std::is_same_v<StreamT, StreamTelnet>) {
                            if (stream_.opts_.tls_) {
                                boost::asio::write(
                                    *stream_.socket_ssl_, boost::asio::buffer(boost::asio::buffer(step_->poll_)));
                            } else {
                                boost::asio::write(
                                    stream_.socket_raw_, boost::asio::buffer(boost::asio::buffer(step_->poll_)));
                            }
                        }
                    }
                    parser_.Reset();
                    DoTimeout(step_->timeout_);
                    DoRead();
                } else {
                    state_ = State::FAILED;
                    boost::asio::post(stream_.ctx_, std::bind(&Autobauder::Run, this));
                }
                break;
            case State::SUCCESS:
            case State::FAILED:
                AUTOBAUDER_TRACE("Autobauder::Run %s", state_ == State::SUCCESS ? "SUCCESS" : "FAILED");
                callback_(state_ == State::SUCCESS);
                state_ = State::IDLE;
                break;
        }
    }

    void DoTimeout(const uint32_t millis)
    {
        AUTOBAUDER_TRACE("Autobauder::DoTimeout %" PRIu32, millis);
        timer_.cancel();
        timer_.expires_after(std::chrono::milliseconds(millis));
        timer_.async_wait(std::bind(&Autobauder::OnTimeout, this, std::placeholders::_1));
    }

    void OnTimeout(const boost::system::error_code& ec)
    {
        AUTOBAUDER_TRACE("Autobauder::OnTimeout %s", ec.message().c_str());

        // We got cancelled, do nothing
        if (ec == boost::asio::error::operation_aborted) {
        }
        // Continue autobaud
        else {
            Run();
        }
    }

    void DoRead()
    {
        AUTOBAUDER_TRACE("Autobauder::DoRead");
        // Magic c++ conditional compilation...
        if constexpr (std::is_same_v<StreamT, StreamSerial>) {
            stream_.port_.async_read_some(
                rx_buf_, std::bind(&Autobauder::OnRead, this, std::placeholders::_1, std::placeholders::_2));
        }
        if constexpr (std::is_same_v<StreamT, StreamTelnet>) {
            if (stream_.opts_.tls_) {
                stream_.socket_ssl_->async_read_some(
                    rx_buf_, std::bind(&Autobauder::OnRead, this, std::placeholders::_1, std::placeholders::_2));
            } else {
                stream_.socket_raw_.async_read_some(
                    rx_buf_, std::bind(&Autobauder::OnRead, this, std::placeholders::_1, std::placeholders::_2));
            }
        }
    }

    void OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred)
    {
        AUTOBAUDER_TRACE("Autobauder::OnRead %" PRIuMAX " %s", bytes_transferred, ec.message().c_str());
        // We got cancelled, do nothing
        if (ec == boost::asio::error::operation_aborted) {
        }
        // E.g. boost::asio::error::eof (= remote closed connection)
        else if (ec) {
            stream_.SetStateError(StreamError::DEVICE_FAIL, "read: " + ec.message());
            state_ = State::FAILED;
            Run();
        }
        // Process incoming data
        else {
            parser_.Add((const uint8_t*)rx_buf_.data(), bytes_transferred);
            while (parser_.Process(msg_)) {
                // Pass all non-garbage messages to the stream for the user to consume later
                if (msg_.proto_ != parser::Protocol::OTHER) {
                    stream_.ProcessRead((const uint8_t*)rx_buf_.data(), bytes_transferred);
                }
                // Autobaud is successful
                if ((step_->expected_.empty() && (msg_.proto_ != parser::Protocol::OTHER)) ||
                    (!step_->expected_.empty() && (msg_.name_ == step_->expected_))) {
                    AUTOBAUDER_DEBUG("autobaud success %s", msg_.name_.c_str());
                    state_ = State::SUCCESS;
                    Run();
                    return;
                }
            }
            DoRead();
        }
    }
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_AUTOBAUDER_HPP__
