/* ****************************************************************************************************************** */
// flipflip's gui (ffgui)
//
// Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
// https://oinkzwurgl.org/projaeggd/ffxx/
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>.

//
#include "ffgui_inc.hpp"
//
#include <filesystem>
//
#include <boost/beast/core/detail/base64.hpp>
#include <fpsdk_common/parser/ubx.hpp>
#include <fpsdk_common/path.hpp>
using namespace fpsdk::common::parser::ubx;
using namespace fpsdk::common::path;
//
#include "utils.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class CacheWiper
{
   public:
    CacheWiper(const std::string& path, const double maxAge) : basePath_{ path }, maxAge_{ maxAge }
    {
        if (path.empty() || (maxAge < 0.0)) {
            throw std::runtime_error("Bad parameters!"); /// FIXME: pfui!
        }
    }

    void Wipe()
    {
        _refTime = std::filesystem::file_time_type::clock::now();
        _Wipe(basePath_);
        DEBUG("CacheWiper: wiped %d/%d files and %d/%d dirs (%.1f/%.1fMiB) in %s", numFilesWiped_, numFilesTotal_,
            numDirsWiped_, numDirsTotal_, (double)sizeWiped_ / 1024.0 / 1024.0, (double)sizeTotal_ / 1024.0 / 1024.0,
            basePath_.c_str());
    }

   private:
    std::filesystem::path basePath_;
    double maxAge_;
    std::filesystem::file_time_type _refTime;
    int numFilesTotal_ = 0;
    int numDirsTotal_ = 0;
    int numFilesWiped_ = 0;
    int numDirsWiped_ = 0;
    std::size_t sizeTotal_ = 0;
    std::size_t sizeWiped_ = 0;

    bool _Wipe(const std::filesystem::path& path)
    {
        bool dirEmpty = true;
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            // Check files
            if (entry.is_regular_file()) {
                numFilesTotal_++;
                const auto size = entry.file_size();
                sizeTotal_ += size;
                if ((_Age(entry) > maxAge_) && _Remove(entry.path())) {
                    numFilesWiped_++;
                    sizeWiped_ += size;
                } else {
                    dirEmpty = false;
                }
            }
            // Iterate directories
            else if (entry.is_directory()) {
                numDirsTotal_++;
                if (!_Wipe(entry.path())) {
                    dirEmpty = false;
                }
            }
            // There should be no special files here (hmmm...?)
            else {
                WARNING("CacheWiper: Ignoring '%s'!", entry.path().string().c_str());
            }
        }
        // Remove this directory if it's empty now, and if it's not the base dir
        if (dirEmpty && (path != basePath_)) {
            if (_Remove(path)) {
                numDirsWiped_++;
            }
        }

        return dirEmpty;
    }

    double _Age(const std::filesystem::directory_entry& entry)
    {
        return std::chrono::duration<double, std::ratio<86400> /*std::chrono::hours*/>(
            _refTime - entry.last_write_time())
            .count();
    }

    bool _Remove(const std::filesystem::path& path)
    {
        std::error_code err;
        std::filesystem::remove(path, err);
        if (err) {
            WARNING("Failed wiping %s: %s", path.c_str(), err.message().c_str());
            return false;
        } else {
            return true;
        }
    }
};

void WipeCache(const std::string& path, const double maxAge)
{
    if (path.empty()) {
        ERROR("Won't wipe empty path!");
        return;
    }

    CacheWiper wiper(path, maxAge);
    wiper.Wipe();
}

// ---------------------------------------------------------------------------------------------------------------------

EnumeratedPorts EnumeratePorts()
{
    std::vector<EnumeratedPort> ports;

    const std::filesystem::path devSerialByIdDir("/dev/serial/by-id");

    if (!std::filesystem::exists(devSerialByIdDir)) {
        return ports;
    }
    for (auto& entry : std::filesystem::directory_iterator(devSerialByIdDir)) {
        if (entry.is_symlink()) {
            // auto source = entry.path();
            auto target = std::filesystem::weakly_canonical(devSerialByIdDir / std::filesystem::read_symlink(entry));

            ports.push_back({target, entry.path().filename().string()});
        }
    }
    std::sort(ports.begin(), ports.end(), [](const auto& a, const auto& b) { return a.port < b.port; });

    return ports;
}

// ---------------------------------------------------------------------------------------------------------------------

ThreadsInfo::ThreadsInfo()
{
    lastT_.SetClockRealtime();
}

void ThreadsInfo::Update()
{
    const auto now = Time::FromClockRealtime();
    const double dt = (now - lastT_).GetSec();
    lastT_ = now;
    const double tck = sysconf(_SC_CLK_TCK);
    const double f = 1e2 / (tck > 0.0 ? tck : 1.0) / (dt > 0.0 ? dt : 1.0);

    std::map<uint64_t, Info> threads;
    for (auto& entry : std::filesystem::directory_iterator("/proc/self/task")) {
        std::vector<uint8_t> buf;
        if (entry.is_directory() && FileSlurp(entry.path().string() + "/stat", buf)) {
            const auto line = BufToStr(buf);
            uint64_t tid;
            char comm[64];
            char state;
            uint64_t utime;
            uint64_t stime;
            if (std::sscanf(line.c_str(), "%" SCNu64 " %63s %c %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %" SCNu64 " %" SCNu64, &tid, comm, &state, &utime, &stime) == 5) {
                auto te = threads_.find(tid);
                // DEBUG("%" PRIu64 " [%s] '%c' %" PRIu64 " %" PRIu64, tid, comm, state, utime, stime);
                if (te != threads_.end()) {
                    auto& ti = te->second;
                    ti.comm_ = comm;
                    ti.state_ = state;
                    ti.cpuUsr_ = (double)(utime - ti.utime_) * f;
                    ti.cpuSys_ = (double)(stime - ti.stime_) * f;
                    ti.utime_ = utime;
                    ti.stime_ = stime;
                    te = threads.emplace(tid, ti).first;
                    // DEBUG("  cpu %.2f %.2f", ti.cpuUsr_, ti.cpuSys_);
                } else {
                    Info ti { tid, std::string(comm), state, 0.0f, 0.0f, utime, stime, ""};
                    te = threads.emplace(tid, ti).first;
                }
                auto& ti = te->second;
                ti.str_ = Sprintf("%8" PRIu64 " %-20s %c %5.1f %5.1f %5.1f", ti.tid_, ti.comm_.c_str(), ti.state_,
                    ti.cpuUsr_, ti.cpuSys_, ti.cpuUsr_ + ti.cpuSys_);
            }
        }
    }
    std::swap(threads_, threads);
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
