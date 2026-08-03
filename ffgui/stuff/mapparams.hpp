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

#ifndef __MAPPARAMS_HPP__
#define __MAPPARAMS_HPP__

#include <vector>
#include <string>
#include <cstdint>
//
// #include <nlohmann/json.hpp>

namespace ffgui {
/* ****************************************************************************************************************** */

struct MapParams
{
    // clang-format off
    bool                      enabled;
    std::string               name; // unique identifer, /^[_a-z0-9]+$/
    std::string               title;
    struct MapLink
    {
        std::string label;
        std::string url;
    };
    std::vector<MapLink>      mapLinks;
    int                       zoomMin;
    int                       zoomMax;
    int                       tileSizeX;
    int                       tileSizeY;
    std::string               downloadUrl;
    std::vector<std::string>  subDomains;
    uint32_t                  downloadTimeout;
    std::string               referer;
    std::string               cachePath;
    double                    minLat; // S [rad]
    double                    maxLat; // N [rad]
    double                    minLon; // W [rad]
    double                    maxLon; // E [rad]
    uint32_t                  threads;

    static constexpr double MIN_LAT_DEG =  -85.0511;
    static constexpr double MAX_LAT_DEG =   85.0511;
    static constexpr double MIN_LON_DEG = -180.0;
    static constexpr double MAX_LON_DEG =  180.0;

    static constexpr double MIN_LAT_RAD = MIN_LAT_DEG * (M_PI / 180.0);
    static constexpr double MAX_LAT_RAD = MAX_LAT_DEG * (M_PI / 180.0);
    static constexpr double MIN_LON_RAD = MIN_LON_DEG * (M_PI / 180.0);
    static constexpr double MAX_LON_RAD = MAX_LON_DEG * (M_PI / 180.0);

    static const std::vector<MapParams> BUILTIN_MAPS;
    static const MapParams NO_MAP;
    // clang-format on
};

// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MapParams::MapLink, label, url)
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MapParams, enabled, name, title, mapLinks, zoomMin, zoomMax, tileSizeX,
//     tileSizeY, downloadUrl, subDomains, downloadTimeout, referer, cachePath, minLat, maxLat, minLon, maxLon, threads)

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __MAPPARAMS_HPP__
