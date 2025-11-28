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
using namespace fpsdk::common::parser::ubx;
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

/* ****************************************************************************************************************** */

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

/* ****************************************************************************************************************** */
}  // namespace ffgui
