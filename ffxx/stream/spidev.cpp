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

/* LIBC/STL */
#include <cerrno>
#include <cstring>

/* EXTERNAL */
#include <boost/endian/conversion.hpp>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/math.hpp>
#include <fpsdk_common/string.hpp>

/* PACKAGE */
#include "spidev.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

/*static*/ std::unique_ptr<StreamOptsSpidev> StreamOptsSpidev::FromPath(
    const std::string& path, std::vector<std::string>& errors)
{
    StreamOptsSpidev opts;

    bool ok = true;

    // "<device>[:<speed>[:<bpw>[:<xfersize>[:<mode>]]]]"
    const auto parts = string::StrSplit(path, ":");

    if ((parts.size() >= 1) && (parts.size() <= 5) && !parts[0].empty()) {
        opts.device_ = parts[0];
    } else {
        errors.push_back("bad <device>");
        ok = false;
    }

    uint32_t value = 0;
    if (parts.size() >= 2) {
        if (string::StrToValue(parts[1], value) && (value >= SPEED_MIN) && (value <= SPEED_MAX)) {
            opts.speed_ = value;
        } else {
            errors.push_back("bad <speed>");
            ok = false;
        }
    }
    if (parts.size() >= 3) {
        if (string::StrToValue(parts[2], value) && ((value == 8) || (value == 16) || value == 32)) {
            opts.bits_per_word_ = value;
        } else {
            errors.push_back("bad <bpw>");
            ok = false;
        }
    }
    if (parts.size() >= 4) {
        if (string::StrToValue(parts[3], value) && (value >= XFER_SIZE_MIN) && (value <= XFER_SIZE_MAX) &&
            ((value % 4) == 0)) {
            opts.xfer_size_ = value;
        } else {
            errors.push_back("bad <xfersize>");
            ok = false;
        }
    }
    if (parts.size() >= 5) {
        if (string::StrToValue(parts[4], value)) {
            opts.spi_mode_ = value;
        } else {
            errors.push_back("bad <mode>");
            ok = false;
        }
    }

    opts.path_ = string::Sprintf("%s:%" PRIu32 ":%" PRIu8 ":%" PRIu32 ":0x%" PRIx32, opts.device_.c_str(), opts.speed_,
        opts.bits_per_word_, opts.xfer_size_, opts.spi_mode_);

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsSpidev>(opts);
}

/* ****************************************************************************************************************** */

StreamSpidev::StreamSpidev(const StreamOptsSpidev& opts) /* clang-format off */ :
    StreamBase(opts, opts_),
    opts_          { opts },
    fd_            { -1 },
    thread_        { opts_.name_, std::bind(&StreamSpidev::Worker, this) }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();

    // If they only had thought of designated initializers for this... :-/
    all_0xff_.fill((uint8_t)0xff);
    all_0x00_.fill((uint8_t)0x00);
}

StreamSpidev::~StreamSpidev()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamSpidev::Start()
{
    STREAM_TRACE("Start");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }

    SetStateConnecting();
    if (OpenSpidev() && thread_.Start()) {
        SetStateConnected();
    } else {
        Stop();
        SetStateClosed();
        return false;
    }

    return true;
}

void StreamSpidev::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    UNUSED(timeout);
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
    CloseSpidev();
}

// ---------------------------------------------------------------------------------------------------------------------

// Useful documentation:
// - /usr/include/linux/spi/spidev.h
// - /usr/include/linux/spi/spi.h
// - https://www.kernel.org/doc/html/latest/spi/spi-summary.html
// - https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/tools/spi/spidev_test.c

// ---------------------------------------------------------------------------------------------------------------------

bool StreamSpidev::OpenSpidev()
{
    STREAM_TRACE("OpenSpidev");

    // Open
    {
        // @todo linux/tools/spi/spidev_test.c used O_RDONLY for some reason...
        int fd = open(opts_.device_.c_str(), O_RDWR);
        if (fd < 0) {
            SetStateError(StreamError::DEVICE_FAIL, "open: " + string::StrError(errno));
            return false;
        }
        fd_ = fd;
    }

    // Get lock on file
    int res = flock(fd_, LOCK_EX | LOCK_NB);
    if (res != 0) {
        SetStateError(StreamError::DEVICE_FAIL, "lock: " + string::StrError(errno));
        CloseSpidev();
        return false;
    }

    // Configure SPI interface
    std::vector<std::string> errors;

    const uint32_t mode_arg = opts_.spi_mode_;
    if (ioctl(fd_, SPI_IOC_WR_MODE32, &mode_arg) < 0) {
        errors.push_back(string::Sprintf("SPI_IOC_WR_MODE32: %s", string::StrError(errno).c_str()));
    }

    const uint8_t bpw_arg = opts_.bits_per_word_;
    if (ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bpw_arg) < 0) {
        errors.push_back(string::Sprintf("SPI_IOC_WR_BITS_PER_WORD: %s", string::StrError(errno).c_str()));
    }

    const long unsigned int speed_arg = opts_.speed_;
    if (ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed_arg) != 0) {
        errors.push_back(string::Sprintf("SPI_IOC_WR_MAX_SPEED_HZ: %s", string::StrError(errno).c_str()));
    }

    if (!errors.empty()) {
        SetStateError(StreamError::DEVICE_FAIL, "config: " + string::StrJoin(errors, ", "));
        CloseSpidev();
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamSpidev::CloseSpidev()
{
    STREAM_TRACE("CloseSpidev (%s)", fd_ < 0 ? "closed" : "open");
    if (fd_ < 0) {
        return;
    }

    int res = flock(fd_, LOCK_UN);
    if (res != 0) {
        STREAM_WARNING("spidev unlock: %s", string::StrError(errno).c_str());
    }

    res = close(fd_);
    if (res != 0) {
        STREAM_WARNING("spidev close: %s", string::StrError(errno).c_str());
    }

    fd_ = -1;
}

// ---------------------------------------------------------------------------------------------------------------------

// Some assumptions made in the code below
// - To detect all 0xff messages, see memcmp() below
static_assert(parser::MAX_OTHER_SIZE <= StreamOptsSpidev::XFER_SIZE_MAX, "");

#ifndef SPI_TX_OCTAL
#  define SPI_TX_OCTAL (1 << 13)
#endif
#ifndef SPI_RX_OCTAL
#  define SPI_RX_OCTAL (1 << 14)
#endif

bool StreamSpidev::Worker()
{
    STREAM_TRACE("Worker");

    // Linux kernel transfer control structure
    struct spi_ioc_transfer spi_xfer;
    std::memset(&spi_xfer, 0, sizeof(spi_xfer));
    // spi_xfer.tx_buf = (uintptr_t)tx_buf_.data();
    // spi_xfer.rx_buf = (uintptr_t)rx_buf_.data();
    spi_xfer.len = opts_.xfer_size_;
    // spi_xfer.     = 0;
    spi_xfer.speed_hz = opts_.speed_;
    spi_xfer.bits_per_word = opts_.bits_per_word_;
    // spi_xfer.cs_change = false;
    spi_xfer.tx_nbits =  // clang-format off
        (math::CheckBitsAll(opts_.spi_mode_, (uint32_t)SPI_TX_OCTAL) ? 8 :
        (math::CheckBitsAll(opts_.spi_mode_, (uint32_t)SPI_TX_QUAD) ? 4 :
        (math::CheckBitsAll(opts_.spi_mode_, (uint32_t)SPI_TX_DUAL) ? 2 : 0)));  // clang-format on
    spi_xfer.rx_nbits =  // clang-format off
        (math::CheckBitsAll(opts_.spi_mode_, (uint32_t)SPI_RX_OCTAL) ? 8 :
        (math::CheckBitsAll(opts_.spi_mode_, (uint32_t)SPI_RX_QUAD) ? 4 :
        (math::CheckBitsAll(opts_.spi_mode_, (uint32_t)SPI_RX_DUAL) ? 2 : 0)));  // clang-format on

    parser::ParserMsg msg;
    while (!thread_.ShouldAbort()) {
        // Get next message from the parser
        bool got_data = false;
        while (parser_.Process(msg)) {
            // We expect messages from a known protocol (UBX, NMEA, ...). If we see OTHER this
            // may be an data from unknown protocol or the device telling us that it has no
            // more data (all 0xff) or the device is not connected or not sending (all 0x00).
            if (msg.proto_ == parser::Protocol::OTHER) {
                // Ignore data that is all 0xff or 0x00
                if ((std::memcmp(msg.Data(), all_0xff_.data(), msg.Size()) == 0) ||
                    (std::memcmp(msg.Data(), all_0x00_.data(), msg.Size()) == 0)) {
                    continue;
                }
                // else: it's perhaps useful unknown data
            }
            // else it's a message from a known protocol

            ProcessRead(msg.Data(), msg.Size());
            got_data = true;
        }

        // Sleep a bit. We'll be woken up by user writes, but as we're the SPI master we cannot know if the device has
        // some data for us. So we sleep only a little bit and keep polling the device.
        if (!got_data && write_queue_.Empty()) {  // @todo mutex needed?
            thread_.Sleep(opts_.sleep_millis_);
        }

        // Check if there's anything to write
        std::size_t tx_size = 0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            tx_size = std::min((uint32_t)write_queue_.Used(), opts_.xfer_size_);
            if (tx_size > 0) {
                write_queue_.Read(tx_buf_.data(), tx_size);
            }
        }

        // Prepare tx buffer
        if (tx_size > 0) {
            // Swap byte order of tx data
            switch (opts_.bits_per_word_) {
                case 8:
                    break;
                case 16:
                    for (std::size_t offs = 0; offs < tx_size; offs += sizeof(uint16_t)) {
                        boost::endian::native_to_big_inplace(*((uint16_t*)&tx_buf_[offs]));
                    }
                    break;
                case 32:
                    for (std::size_t offs = 0; offs < tx_size; offs += sizeof(uint32_t)) {
                        boost::endian::native_to_big_inplace(*((uint32_t*)&tx_buf_[offs]));
                    }
                    break;
            }
            // Fill remaining tx buffer with 0xff
            if (tx_size < opts_.xfer_size_) {
                std::memset(&tx_buf_[tx_size], 0xff, opts_.xfer_size_ - tx_size);
            }
            spi_xfer.tx_buf = (uintptr_t)tx_buf_.data();
        }
        // Nothing to write
        else {
            spi_xfer.tx_buf = (uintptr_t)all_0xff_.data();
        }

        // rx buffer
        spi_xfer.rx_buf = (uintptr_t)rx_buf_.data();

        // Do one send+receive transaction
        if (ioctl(fd_, SPI_IOC_MESSAGE(1), &spi_xfer) >= 0) {
            // Swap byte order of rx data
            switch (opts_.bits_per_word_) {
                case 8:
                    break;
                case 16:
                    for (std::size_t offs = 0; offs < opts_.xfer_size_; offs += sizeof(uint16_t)) {
                        boost::endian::native_to_big_inplace(*((uint16_t*)&rx_buf_[offs]));
                    }
                    break;
                case 32:
                    for (std::size_t offs = 0; offs < opts_.xfer_size_; offs += sizeof(uint32_t)) {
                        boost::endian::native_to_big_inplace(*((uint32_t*)&rx_buf_[offs]));
                    }
                    break;
            }

            // Run data through the parser
            if (!parser_.Add(rx_buf_.data(), opts_.xfer_size_)) {
                STREAM_WARNING_THR(1000, "spi parser ovfl");  // we don't expect this ever
                parser_.Reset();
                parser_.Add(rx_buf_.data(), opts_.xfer_size_);
            }
        } else {
            // @todo handle this somehow?
            STREAM_WARNING_THR(2000, "SPI_IOC_MESSAGE fail: %s", string::StrError(errno).c_str());
            break;
        }
    }

    SetStateClosed();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamSpidev::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_ongoing_));
    UNUSED(size);

    thread_.Wakeup();

    return true;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
