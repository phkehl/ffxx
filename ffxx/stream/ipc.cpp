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
 * @brief Stream IPC
 */

/* LIBC/STL */
#include <chrono>
#include <cstring>
#include <exception>
#include <regex>

/* EXTERNAL */
#include <boost/interprocess/permissions.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <sys/stat.h>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/string.hpp>

/* PACKAGE */
#include "ipc.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common;

/*static*/ std::unique_ptr<StreamOptsIpc> StreamOptsIpc::FromPath(
    const std::string& path, std::vector<std::string>& errors, const StreamType type)
{
    StreamOptsIpc opts;
    bool ok = true;

    // client: "<name>"
    // client: "<server>"
    const std::regex path_re("^([-_a-zA-Z0-9]{3,30})$");
    std::smatch m;
    if (std::regex_match(path, m, path_re) && (m.size() == 2)) {
        TRACE("match [%s] [%s]", m[0].str().c_str(), m[1].str().c_str());
        opts.name_ = m[1].str();
    } else {
        ok = false;
        errors.push_back("bad <name>");
    }

    opts.server_ = (type == StreamType::IPCSVR);

    opts.path_ = opts.name_;

    if (!ok) {
        return nullptr;
    }
    return std::make_unique<StreamOptsIpc>(opts);
}

/* ****************************************************************************************************************** */

StreamIpc::StreamIpc(const StreamOptsIpc& opts) /* clang-format off */ :
    StreamBase(opts, opts_),
    opts_     { opts },
    thread_   { opts_.name_, std::bind(&StreamIpc::Worker, this) }  // clang-format on
{
    SetStateClosed();
    STREAM_TRACE_WARNING();

    shm_name_ = "ffxx_stream_" + opts_.name_;
}

StreamIpc::~StreamIpc()
{
    Stop();
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamIpc::Start()
{
    STREAM_TRACE("Start");
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        return false;
    }

    if (!thread_.Start()) {
        return false;
    }

    if (!opts_.hotplug_ || opts_.server_) {
        SetStateConnecting();
        if (!OpenShm()) {
            Stop();
            return false;
        }
    }

    return true;
}

void StreamIpc::Stop(const uint32_t timeout)
{
    STREAM_TRACE("Stop %" PRIu32, timeout);
    UNUSED(timeout);
    if (thread_.GetStatus() != thread_.Status::STOPPED) {
        thread_.Stop();
    }
    if (!opts_.hotplug_ || opts_.server_) {
        CloseShm();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamIpc::OpenShm()
{
    STREAM_TRACE("OpenShm");

    bool ok = true;
    try {
        // Server: create the shared memory object
        if (opts_.server_) {
            shm_obj_ = std::make_unique<boost::interprocess::shared_memory_object>(boost::interprocess::create_only,
                shm_name_.c_str(), boost::interprocess::read_write,
                boost::interprocess::permissions(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));
        }
        // Client: open the shared memory object created by the server
        else {
            shm_obj_ = std::make_unique<boost::interprocess::shared_memory_object>(
                boost::interprocess::open_only, shm_name_.c_str(), boost::interprocess::read_write);
        }

        // Server: size the shared memory object
        if (opts_.server_) {
            shm_obj_->truncate(sizeof(ShmData));
        }

        // Mapping memory region to gain access to the shared memory object
        shm_reg_ = std::make_unique<boost::interprocess::mapped_region>(*shm_obj_, boost::interprocess::read_write);

        // Server: initialise the memory
        if (opts_.server_) {
            std::memset(shm_reg_->get_address(), 0, shm_reg_->get_size());
            shm_data_ = new (shm_reg_->get_address()) ShmData;
            tx_chn_ = &shm_data_->svr_to_cli_;
            rx_chn_ = &shm_data_->cli_to_svr_;
        }
        // Client: use memory, check server
        else {
            shm_data_ = static_cast<ShmData*>(shm_reg_->get_address());
            if (!shm_data_->cli_to_svr_.ok_) {
                throw std::runtime_error(std::strerror(EFAULT));
            }
            if (!shm_data_->cli_mutex_.try_lock()) {
                throw std::runtime_error(std::strerror(EADDRINUSE));
            }
            tx_chn_ = &shm_data_->cli_to_svr_;
            rx_chn_ = &shm_data_->svr_to_cli_;
        }

        // We're ready to receive
        rx_chn_->ok_ = true;

        connected_ = true;
        SetStateConnected();

    } catch (std::exception& ex) {
        SetStateError(StreamError::DEVICE_FAIL, ex.what());
        ok = false;
    }

    return ok;
}

// ---------------------------------------------------------------------------------------------------------------------

void StreamIpc::CloseShm()
{
    STREAM_TRACE("CloseShm");
    if (connected_) {
        // We're no longer ready to receive
        rx_chn_->ok_ = false;
        tx_chn_->rx_sem_.post();   // fake "something's ready to be received"
        tx_chn_->act_sem_.post();  // wakeup
        if (opts_.server_) {
            //  If there's a client, this will wait until it has completed CloseShm()
            shm_data_->cli_mutex_.lock();  // no need for an unlock() anywhere, the memory is abandoned blow
            // Now we can remove the memory
            boost::interprocess::shared_memory_object::remove(shm_name_.c_str());
        } else {
            shm_data_->cli_mutex_.unlock();
        }
        shm_obj_.reset();
        shm_reg_.reset();
        shm_data_ = nullptr;
        tx_chn_ = nullptr;
        rx_chn_ = nullptr;
        connected_ = false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamIpc::Worker()
{
    STREAM_TRACE("Worker");

    bool server_gone = false;

    while (!thread_.ShouldAbort()) {
        if (server_gone) {
            CloseShm();
            server_gone = false;
            if (opts_.hotplug_) {
                thread_.Sleep(opts_.retry_to_.count());
            }
        }

        if (!connected_) {
            if (!opts_.server_ && opts_.hotplug_) {
                SetStateConnecting();
                if (!OpenShm()) {
                    thread_.Sleep(opts_.retry_to_.count());
                    continue;
                }
            } else {
                thread_.Sleep(337);
                continue;
            }
        }

        // Wait for activity
        {
            const auto deadline =
                boost::posix_time::microsec_clock::universal_time() + boost::posix_time::millisec(337);
            if (!rx_chn_->act_sem_.timed_wait(deadline)) {
                continue;
            }
        }

        // Check if there's anything to read
        if (rx_chn_->rx_sem_.try_wait()) {
            // boost::interprocess::scoped_lock lock(rx_chn_->mutex_);
            if (!opts_.server_ && !tx_chn_->ok_) {
                SetStateError(StreamError::DEVICE_FAIL, "server gone");
                if (opts_.hotplug_) {
                    server_gone = true;
                    continue;
                } else {
                    break;
                }
            }

            ProcessRead(rx_chn_->data_, rx_chn_->size_);
            rx_chn_->size_ = 0;
            rx_chn_->tx_sem_.post();
        }

        // Check if there's anything to write
        {
            std::unique_lock<std::mutex> write_queue_lock(mutex_);
            while (tx_chn_->ok_ && (write_queue_.Used() > 0)) {
                const auto deadline =
                    boost::posix_time::microsec_clock::universal_time() + boost::posix_time::millisec(5);
                if (tx_chn_->tx_sem_.timed_wait(deadline)) {
                    // boost::interprocess::scoped_lock lock(tx_chn_->mutex_);
                    tx_chn_->size_ = std::min(sizeof(tx_chn_->data_), write_queue_.Used());
                    write_queue_.Read(tx_chn_->data_, tx_chn_->size_);
                    tx_chn_->rx_sem_.post();
                    tx_chn_->act_sem_.post();  // wakeup remote
                } else {
                    break;  // Remote end gone
                }
            }
            if (!tx_chn_->ok_) {
                write_queue_.Reset();
                // @todo need to handle server gone?
            }
        }
    }

    if (!opts_.server_ && opts_.hotplug_) {
        CloseShm();
    }

    SetStateClosed();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

bool StreamIpc::ProcessWrite(const std::size_t size)
{
    STREAM_TRACE("ProcessWrite %" PRIuMAX "/%" PRIuMAX " %s", size, write_queue_.Used(), string::ToStr(tx_chn_->ok_));
    UNUSED(size);

    if (!connected_) {
        return false;
    }

    rx_chn_->act_sem_.post();  // wakeup ourselves
    return true;
}

/* ****************************************************************************************************************** */
}  // namespace ffxx
