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

#ifndef __GUI_WIDGET_MAP_HPP__
#define __GUI_WIDGET_MAP_HPP__

#include "ffgui_inc.hpp"
//
#include <array>
//
#include "mapparams.hpp"
#include "maptiles.hpp"
#include "utils.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWidgetMap
{
   public:
    GuiWidgetMap(const std::string& name);
    virtual ~GuiWidgetMap();

    void SetMap(const MapParams& mapParams, const bool resetView = false);
    void SetZoom(const float zoom);
    void SetPos(const double lat, const double lon);  // [rad]

    // 1. Draw base map
    bool BeginDraw();

    // 2. Draw custom stuff
    float PixelPerMetre(const float lat);                      // [rad]
    Vec2f LatLonToScreen(const double lat, const double lon);  // [rad]

    // 3. Draw controls, handle dragging, zooming, etc.
    void EndDraw();

    // Other FIXME: solve this better
    void EnableFollowButton();
    bool FollowEnabled();

   private:
    std::string name_;

    struct Config
    {  // clang-format off
        std::string           mapName;
        std::array<double, 2> centPosLonLat = { { 0.0, 0.0 } };                  // Position (lon/lat) at canvas centre
        float                 zoom          = 0.0f;                              // Zoom level
        bool                  hideConfig    = true;                              // Hide map config (base layer, zoom, etc.)
        bool                  showInfo      = true;                              // Show map info (mouse position, scale bar)
        bool                  debugLayout   = false;                             // Debug map layout
        bool                  debugTiles    = false;                             // Debug tiles
        ImU32                 tintColour    = IM_COL32(0xff, 0xff, 0xff, 0xff);  // Tint colour
        bool                  followMode    = false;                             // Follow mode
    };  // clang-format on
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        Config, mapName, centPosLonLat, zoom, hideConfig, showInfo, debugLayout, debugTiles, tintColour)
    Config cfg_;

    // Currently used map parameters and map tiles
    MapParams _mapParams;
    std::unique_ptr<MapTiles> mapTiles_;

    // Settings and state
    Vec2d centPosXy_;                          // Position (tile x/y) at canvas centre
    Vec2d centTileOffs_;                       // Centre tile (top left) offset from canvasCent_
    Vec2d tileSize_;                           // Tile size (_mapParams.tileSize[XY] with scaling applied)
    int zLevel_ = 0;                           // Map tiles zoom level
    int numTiles_ = 0;                         // Number of tiles in x and y (at zLevel_)
    bool autoSize_ = false;                    // Auto-size map on next draw
    Vec2d dragStartXy_;                        // Start of dragging map (tile coordinates)
    int isDragging_ = ImGuiMouseButton_COUNT;  // Dragging in progress
    bool followButtonEna_ = false;             // Follow mode button enabled
    ImVec4 tintColour4_;
    void _SetPosAndZoom(
        const Vec2d& lonLat, const float zoom, const int zLevel = -1, const float snap = ZOOM_STEP_SHIFT);
    void _DrawMapTile(ImDrawList* draw, const int ix, const int dx, const int dy);

    // Map canvas in screen coordinates
    Vec2f canvasMin_;
    Vec2f canvasSize_;
    Vec2f canvasMax_;
    Vec2f canvasCent_;

    // clang-format off
    static constexpr float CANVAS_MIN_WIDTH  = 100.0f;
    static constexpr float CANVAS_MIN_HEIGHT =  50.0f;
    static constexpr float ZOOM_MIN        =  0.0;
    static constexpr float ZOOM_MAX        = 28.0;
    static constexpr float ZOOM_STEP       =  0.1;
    static constexpr float ZOOM_STEP_SHIFT =  0.05;
    static constexpr float ZOOM_STEP_CTRL  =  0.5;
    // clang-format on
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIDGET_MAP_HPP__
