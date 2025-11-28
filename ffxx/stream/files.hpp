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
 * @brief Stream file read and write
 */
#ifndef __FFXX_STREAM_FILEIN_HPP__
#define __FFXX_STREAM_FILEIN_HPP__

/* LIBC/STL */

/* EXTERNAL */
#include <fpsdk_common/parser.hpp>
#include <fpsdk_common/thread.hpp>
#include <fpsdk_common/time.hpp>

/* PACKAGE */
#include "base.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

// FILEOUT stream options
struct StreamOptsFileout : public StreamOpts  // clang-format off
{
    static std::unique_ptr<StreamOptsFileout> FromPath(const std::string& path, std::vector<std::string>& errors);

    std::string templ_;
    time::Duration swap_;
    bool align_ = true;
    bool ts_ = false;

    static constexpr int SWAP_MIN = time::SEC_IN_MIN_I;
    static constexpr int SWAP_MAX = time::SEC_IN_DAY_I;

};  // clang-format on

struct FileIdxTsRec
{
    uint64_t offs_ = 0;
    time::Time time_;
};

static_assert(sizeof(FileIdxTsRec) == 16, "");

// FILEOUT stream implementation
class StreamFileout : public StreamBase
{
   public:
    StreamFileout(const StreamOptsFileout& opts);
    ~StreamFileout();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   private:
    StreamOptsFileout opts_;

    std::string data_fn_;
    std::string ts_fn_;
    int data_fd_ = -1;
    int ts_fd_ = -1;
    time::Time data_t0_;
    std::size_t data_size_;
    parser::Parser parser_;
    uint8_t chunk_buf[parser::MAX_ADD_SIZE];
    FileIdxTsRec ts_rec_;

    thread::Thread thread_;
    bool Worker();

    bool OpenFile();
    void CloseFile();
    std::string MakeFilePath();

    bool ProcessWrite(const std::size_t size) final;
};

// ---------------------------------------------------------------------------------------------------------------------

// FILEIN stream options
struct StreamOptsFilein : public StreamOpts  // clang-format off
{
    static std::unique_ptr<StreamOptsFilein> FromPath(const std::string& path, std::vector<std::string>& errors);

    std::string file_;
    double speed_ = 0.0;
    double offs_ = 0.0;
    const double SPEED_MIN = 0.1;
    const double SPEED_MAX = 100.0;
};  // clang-format on

// FILEIN stream implementation
class StreamFilein : public StreamBase
{
   public:
    StreamFilein(const StreamOptsFilein& opts);
    ~StreamFilein();

    bool Start() final;
    void Stop(const uint32_t timeout = 0) final;

   private:
    StreamOptsFilein opts_;

    std::string data_fn_;
    std::string ts_fn_;
    int data_fd_ = -1;
    int ts_fd_ = -1;

    thread::Thread thread_;
    bool Worker();

    bool OpenFile();
    void CloseFile();

    bool ProcessWrite(const std::size_t size) final;
};

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_STREAM_FILEIN_HPP__
