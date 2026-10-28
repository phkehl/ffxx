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
#ifndef __FFXX_STREAM_IPC_HPP__
#define __FFXX_STREAM_IPC_HPP__

/* LIBC/STL */
#include <memory>

/* EXTERNAL */
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <fpsdk_common/parser/types.hpp>
#include <fpsdk_common/thread.hpp>

/* PACKAGE */
#include "base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

// IPCSVR/IPCCLI stream options
struct StreamOptsIpc : public StreamOpts  // clang-format off
{
    static std::unique_ptr<StreamOptsIpc> FromPath(const std::string& path, std::vector<std::string>& errors, const StreamType type);

    std::string name_;
    bool server_ = false;

};  // clang-format on

// IPCSVR/IPCCLI stream implementation
class StreamIpc : public StreamBase
{
   public:
    StreamIpc(const StreamOptsIpc& opts);
    ~StreamIpc();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   private:
    StreamOptsIpc opts_;

    struct DataChn
    {
        bool ok_ = false;
        boost::interprocess::interprocess_semaphore act_sem_{ 0 };
        // boost::interprocess::interprocess_mutex mutex_; // @todo needed?
        boost::interprocess::interprocess_semaphore tx_sem_{ 1 };
        boost::interprocess::interprocess_semaphore rx_sem_{ 0 };
        uint8_t data_[parser::MAX_ADD_SIZE];
        std::size_t size_ = 0;
    };

    struct ShmData
    {
        boost::interprocess::interprocess_mutex cli_mutex_;
        DataChn svr_to_cli_;
        DataChn cli_to_svr_;
    };

    std::string shm_name_;
    std::unique_ptr<boost::interprocess::shared_memory_object> shm_obj_;
    std::unique_ptr<boost::interprocess::mapped_region> shm_reg_;
    ShmData* shm_data_ = nullptr;
    DataChn* tx_chn_ = nullptr;
    DataChn* rx_chn_ = nullptr;
    bool connected_ = false;

    bool OpenShm();
    void CloseShm();

    thread::Thread thread_;
    bool Worker();

    bool ProcessWrite(const std::size_t size) final;
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_IPC_HPP__
