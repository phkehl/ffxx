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
 * @brief Stream spidev
 */
#ifndef __FFXX_STREAM_SPIDEV_HPP__
#define __FFXX_STREAM_SPIDEV_HPP__

/* LIBC/STL */
#include <array>
#include <cstdint>

/* EXTERNAL */
#include <linux/spi/spidev.h>
#include <fpsdk_common/parser.hpp>
#include <fpsdk_common/thread.hpp>

/* PACKAGE */
#include "base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

// spidev client stream options
struct StreamOptsSpidev : public StreamOpts  // clang-format off
{
   static std::unique_ptr<StreamOptsSpidev> FromPath(const std::string& path, std::vector<std::string>& errors);

    std::string device_;                     // Device
    uint32_t    speed_         = 1000000;    // SPI speed [Hz] (should probably be a of 1000000)
    uint8_t     bits_per_word_ = 32;         // SPI bits per word (8, 16 or 32)
    uint32_t    xfer_size_     = 64;         // SPI transfer size [bytes] (must be a multiple of 4)
    uint32_t    spi_mode_      = SPI_MODE_0; // SPI mode (see linux/spi/spi.h)

    static constexpr std::size_t SPEED_MIN     =             1000;
    static constexpr std::size_t SPEED_MAX     = 25 * 1000 * 1000;
    static constexpr std::size_t XFER_SIZE_MIN =   64;
    static constexpr std::size_t XFER_SIZE_MAX = 2048;

    // not currently configurable
    static constexpr uint32_t sleep_millis_ = 10; // Sleep [ms] if no data

};  // clang-format on

// spidev stream implementation
class StreamSpidev : public StreamBase
{
   public:
    StreamSpidev(const StreamOptsSpidev& opts);
    ~StreamSpidev();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   private:
    StreamOptsSpidev opts_;
    int fd_;
    parser::Parser parser_;
    thread::Thread thread_;
    bool Worker();

    bool OpenSpidev();
    void CloseSpidev();

    alignas(8) std::array<uint8_t, StreamOptsSpidev::XFER_SIZE_MAX> all_0xff_;
    alignas(8) std::array<uint8_t, StreamOptsSpidev::XFER_SIZE_MAX> all_0x00_;
    alignas(8) std::array<uint8_t, StreamOptsSpidev::XFER_SIZE_MAX> tx_buf_;
    alignas(8) std::array<uint8_t, StreamOptsSpidev::XFER_SIZE_MAX> rx_buf_;

    bool ProcessWrite(const std::size_t size) final;
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_SPIDEV_HPP__
