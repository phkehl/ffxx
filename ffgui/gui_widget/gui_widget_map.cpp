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

#include "ffgui_inc.hpp"
//
#include <algorithm>
//
#include <fpsdk_common/math.hpp>
#include <fpsdk_common/trafo.hpp>
using namespace fpsdk::common::math;
using namespace fpsdk::common::trafo;
//
#include "gui_widget_map.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWidgetMap::GuiWidgetMap(const std::string& name) /* clang-format off */:
    name_   { name }  // clang-format on
{
    GuiGlobal::LoadObj(name_ + ".GuiWidgetMap", cfg_);

    DEBUG("GuiWidgetMap.%s map=%s centPos=%.6f/%.6f zoom=%.1f", name_.c_str(), cfg_.mapName.c_str(),
        cfg_.centPosLonLat[0], cfg_.centPosLonLat[1], cfg_.zoom);

    auto map = std::find_if(MapParams::BUILTIN_MAPS.begin(), MapParams::BUILTIN_MAPS.begin(),
        [&](const auto& cand) { return (cand.enabled && (cand.name == cfg_.mapName)); });
    if (map == MapParams::BUILTIN_MAPS.end()) {
        map = std::find_if(MapParams::BUILTIN_MAPS.begin(), MapParams::BUILTIN_MAPS.begin(),
            [&](const auto& cand) { return cand.enabled; });
    }

    if (map != MapParams::BUILTIN_MAPS.end()) {
        SetMap(*map, false);
        _SetPosAndZoom(cfg_.centPosLonLat, cfg_.zoom);
    } else {
        SetMap(MapParams::NO_MAP, true);
    }

    tintColour4_ = ImGui::ColorConvertU32ToFloat4(cfg_.tintColour);
}

GuiWidgetMap::~GuiWidgetMap()
{
    GuiGlobal::SaveObj(name_ + ".GuiWidgetMap", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetMap::SetMap(const MapParams& mapParams, const bool resetView)
{
    _mapParams = mapParams;
    cfg_.mapName = _mapParams.name;
    mapTiles_ = std::make_unique<MapTiles>(mapParams);

    if (resetView) {
        _SetPosAndZoom({ 0.0, 0.0 }, 1.0);
    } else {
        _SetPosAndZoom(cfg_.centPosLonLat, cfg_.zoom, zLevel_);
    }
    autoSize_ = resetView;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetMap::SetPos(const double lat, const double lon)
{
    _SetPosAndZoom({ lon, lat }, cfg_.zoom, zLevel_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetMap::SetZoom(const float zoom)
{
    _SetPosAndZoom(cfg_.centPosLonLat, zoom);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetMap::_SetPosAndZoom(const Vec2d& pos, const float zoom, const int zLevel, const float snap)
{
    // Clamp and snap zoom level
    cfg_.zoom = std::clamp(zoom, ZOOM_MIN, ZOOM_MAX);
    if (snap > 0.0f) {
        cfg_.zoom = std::floor((cfg_.zoom * (1.0f / snap)) + (snap / 2.0f)) * snap;
    }

    // The corresponding integer zoom level (tile z)
    const int zLevelZoom = (int)std::floor(cfg_.zoom + 0.01);

    // Choose closest map tile z level
    if (zLevel < 0) {
        zLevel_ = zLevelZoom;
    }
    // Custom map tile z level
    else {
        // Don't allow zooming more than two levels closer (to not download/draw LOTS of tiles)
        const int minZ = zLevelZoom - 10;
        const int maxZ = zLevelZoom + 2;
        zLevel_ = std::clamp(zLevel, minZ, maxZ);
    }

    // Clip to range supported by the map
    zLevel_ = std::clamp(zLevel_, _mapParams.zoomMin, _mapParams.zoomMax);

    numTiles_ = 1 << zLevel_;

    // Update center position
    cfg_.centPosLonLat[0] = std::clamp(pos.x, _mapParams.minLon, _mapParams.maxLon);
    cfg_.centPosLonLat[1] = std::clamp(pos.y, _mapParams.minLat, _mapParams.maxLat);
    centPosXy_ = MapTiles::LonLatToTileXy(cfg_.centPosLonLat, zLevel_);

    // Scale tiles for zoom level
    const float deltaScale = cfg_.zoom - (float)zLevel_;  // can be > 1, e.g. when zoom > max. level the map has
    const float tileScale =
        (std::fabs(deltaScale) > (1.0 - FLT_EPSILON) ? std::pow(2.0, deltaScale) : 1.0 + deltaScale);
    tileSize_ = Vec2d(_mapParams.tileSizeX, _mapParams.tileSizeY) * tileScale;

    // Centre tile top-left corner offset from canvas centre
    const float fracX = centPosXy_.x - std::floor(centPosXy_.x);
    const float fracY = centPosXy_.y - std::floor(centPosXy_.y);
    centTileOffs_ = ImVec2(-std::round(fracX * tileSize_.x), -std::round(fracY * tileSize_.y));

    autoSize_ = false;
}

// ---------------------------------------------------------------------------------------------------------------------

float GuiWidgetMap::PixelPerMetre(const float lat)
{
    const float cosLatPowZoom = std::cos(lat) * std::pow(2.0f, -zLevel_);
    const float sTile = WGS84_C * cosLatPowZoom;  // Tile width in [m]
    return tileSize_.x / sTile;                   // 1 [m] in [px]
}

// ---------------------------------------------------------------------------------------------------------------------

Vec2f GuiWidgetMap::LatLonToScreen(const double lat, const double lon)
{
    // Note: double precision calculations here!
    const Vec2<double> tXy = MapTiles::LonLatToTileXy(Vec2<double>(lon, lat), zLevel_);
    return canvasCent_ + Vec2f((tXy.x - (double)centPosXy_.x) * (double)tileSize_.x,
                             (tXy.y - (double)centPosXy_.y) * (double)tileSize_.y);

    // FIXME: check visibility? Return 0,0 if outside of canvas?
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetMap::BeginDraw()
{
    canvasMin_ = ImGui::GetCursorScreenPos();
    canvasSize_ = ImGui::GetContentRegionAvail();
    if (((int)canvasSize_.x) & 0x1) {
        canvasSize_.x -= 1.0f;
    }  // Make canvas size even number..
    if (((int)canvasSize_.y) & 0x1) {
        canvasSize_.y -= 1.0f;
    }  // ..so that the centre is even, too
    canvasMax_ = canvasMin_ + canvasSize_;
    canvasCent_ = (canvasMin_ + canvasMax_) * 0.5;
    if ((canvasSize_.x < CANVAS_MIN_WIDTH) || (canvasSize_.y < CANVAS_MIN_HEIGHT)) {
        return false;
    }

    if (autoSize_) {
        const int numTilesX = (canvasSize_.x / _mapParams.tileSizeX) + 1;
        const int numTilesY = (canvasSize_.y / _mapParams.tileSizeY) + 1;
        const int numTiles = std::max(numTilesX, numTilesY);
        const float zoom = std::ceil(std::sqrt((float)numTiles));
        SetZoom(zoom);
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(canvasMin_, canvasMax_);

    // Draw centre tile
    _DrawMapTile(draw, 0, 0, 0);

    // Draw tiles around centre tile, in a spiral...
    //
    //                                     dist  ix0        length
    //                              81     5     9*9 9=5+4  11 =5+6
    //   73 74 75 76 77 78 79 80 49        4     7*7 7=4+3   9 =4+5
    //   72 43 44 45 46 47 48 25 50   -3   3     5*5 5=3+2   7 =3+4
    //   71 42 21 22 23 24  9 26 51   -2   2     3*3 3=2+1   5 =2+3
    //   70 41 20  7  8  1 10 27 52   -1   1     1*1 1=1+0   3 =1+2
    //   69 40 19  6 [0] 2 11 28 53
    //   68 39 18  5  4  3 12 29 54              ix0 = (d+(d-1))*(d+(d-1)) = (2d-1)*(2d-1) = 4dd-4d+1 = 4(dd-d)+1
    //   67 38 17 16 15 14 13 30 55
    //   66 37 36 35 34 33 32 31 56
    //   65 64 63 62 61 60 59 58 57
    //

    const int maxDx = ((canvasSize_.x / tileSize_.x) / 2) + 1.5;
    const int maxDy = ((canvasSize_.y / tileSize_.y) / 2) + 1.5;
    const int maxDist = std::max(maxDx, maxDy);
    for (int dist = 1, ix = 1; dist <= maxDist; dist++)  //  1  2  3  4  5 ...
    {
        // int ix = 4 * ((dist * dist) - dist) + 1;   //  1  9 25 49 81 ...
        //  top right -> bottom right
        for (int dx = dist, dy = -dist; dy <= dist; dy++, ix++) {
            _DrawMapTile(draw, ix, dx, dy);
        }
        // bottom right -> bottom left
        for (int dx = dist - 1, dy = dist; dx >= -dist; dx--, ix++) {
            _DrawMapTile(draw, ix, dx, dy);
        }
        // bottom left -> top left
        for (int dx = -dist, dy = dist - 1; dy >= -dist; dy--, ix++) {
            _DrawMapTile(draw, ix, dx, dy);
        }
        // top left -> top right
        for (int dx = 1 - dist, dy = -dist; dx <= (dist - 1); dx++, ix++) {
            _DrawMapTile(draw, ix, dx, dy);
        }
    }

    // draw->PopClipRect(); // nope, later..

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetMap::_DrawMapTile(ImDrawList* draw, const int ix, const int dx, const int dy)
{
    // TODO: wrap around? Hmmm... would only work for global maps
    const int tx = (int)centPosXy_.x + dx;
    const int ty = (int)centPosXy_.y + dy;
    if ((tx >= 0) && (tx < numTiles_) && (ty >= 0) && (ty < numTiles_)) {
        const Vec2f tile0 = canvasCent_ + centTileOffs_ + (Vec2f(dx, dy) * tileSize_);
        const Vec2f tile1 = tile0 + tileSize_;
        if ((tile0.x < canvasMax_.x) && (tile0.y < canvasMax_.y) && (tile1.x > canvasMin_.x) &&
            (tile1.y > canvasMin_.y)) {
            const bool skipDraw = (cfg_.tintColour & IM_COL32_A_MASK) == 0;
            if (skipDraw) {
                if (mapTiles_) {
                    mapTiles_->GetTileTex(tx, ty, zLevel_);
                }
            } else {
                if (mapTiles_) {
                    draw->AddImage({ (ImTextureID)mapTiles_->GetTileTex(tx, ty, zLevel_) }, tile0, tile1, Vec2f(0, 0),
                        Vec2f(1, 1), cfg_.tintColour);
                }
                if (cfg_.debugTiles) {
#if 0
                    draw->AddRect(tile0, tile1, C_MAP_DEBUG());
#else
                    draw->AddLine(ImVec2(tile0.x, tile0.y), ImVec2(tile0.x + 25, tile0.y), C_MAP_DEBUG());
                    draw->AddLine(ImVec2(tile0.x, tile0.y), ImVec2(tile0.x, tile0.y + 25), C_MAP_DEBUG());

                    draw->AddLine(ImVec2(tile1.x, tile0.y), ImVec2(tile1.x - 25, tile0.y), C_MAP_DEBUG());
                    draw->AddLine(ImVec2(tile1.x, tile0.y), ImVec2(tile1.x, tile0.y + 25), C_MAP_DEBUG());

                    draw->AddLine(ImVec2(tile0.x, tile1.y), ImVec2(tile0.x + 25, tile1.y), C_MAP_DEBUG());
                    draw->AddLine(ImVec2(tile0.x, tile1.y), ImVec2(tile0.x, tile1.y - 25), C_MAP_DEBUG());

                    draw->AddLine(ImVec2(tile1.x, tile1.y), ImVec2(tile1.x - 25, tile1.y), C_MAP_DEBUG());
                    draw->AddLine(ImVec2(tile1.x, tile1.y), ImVec2(tile1.x, tile1.y - 25), C_MAP_DEBUG());
#endif
                    draw->AddText(tile0 + (tileSize_ / 2) - Vec2f(40, 30), C_MAP_DEBUG(),
                        Sprintf("#%d %d/%d %.2f %d\n%.1fx%.1f\n%.1f/%.1f\n%.1f/%.1f", ix, tx, ty, cfg_.zoom, zLevel_,
                            tileSize_.x, tileSize_.y, tile0.x, tile0.y, tile1.x, tile1.y)
                            .c_str());
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

#define _CHECK_HOVER()            \
    if (ImGui::IsItemHovered()) { \
        controlsHovered = true;   \
    }

void GuiWidgetMap::EndDraw()
{
    ImGui::SetCursorScreenPos(canvasMin_ + GUI_VAR.imguiStyle->ItemSpacing);

    bool controlsHovered = false;

    // Map control
    {
        Gui::ToggleButton(
            ICON_FK_MAP "##ShowConfig", nullptr, &cfg_.hideConfig, "Map config", nullptr, GUI_VAR.iconSize);
        _CHECK_HOVER();

        ImGui::SameLine();

        // Zoom level input
        {
            ImGui::PushItemWidth((GUI_VAR.charSizeX * 6));
            float mapZoom = cfg_.zoom;
            if (ImGui::InputScalar("##MapZoom", ImGuiDataType_Float, &mapZoom, NULL, NULL, "%.2f")) {
                SetZoom(mapZoom);
            }
            ImGui::PopItemWidth();
            Gui::ItemTooltip("Map zoom level");
            _CHECK_HOVER();
        }

        // Tile download progress bar
        const int numTilesInQueue = mapTiles_ ? mapTiles_->NumTilesInQueue() : 0;
        if (numTilesInQueue > 0) {
            ImGui::SameLine();

            const float progress = (float)numTilesInQueue / (float)mapTiles_->MAX_TILES_IN_QUEUE;
            char str[100];
            std::snprintf(str, sizeof(str), "Loading %d tiles", numTilesInQueue);
            ImGui::ProgressBar(progress, ImVec2(GUI_VAR.charSizeX * 20, 0.0f), str);
            _CHECK_HOVER();
        }

        // Display full map config
        if (!cfg_.hideConfig) {
            ImGui::NewLine();
            ImGui::SameLine();
            // if (ImGui::BeginChildFrame(1234, GUI_VAR.charSize * Vec2f(30, 5.5), ImGuiWindowFlags_NoScrollbar))
            if (ImGui::BeginChild(1234, GUI_VAR.charSize * Vec2f(30, 5.5), false, ImGuiWindowFlags_NoScrollbar)) {
                // Zoom slider
                {
                    float mapZoom = cfg_.zoom;
                    ImGui::PushItemWidth(-1);
                    if (ImGui::SliderFloat("##MapZoom", &mapZoom, ZOOM_MIN, ZOOM_MAX, "%.2f")) {
                        SetZoom(mapZoom);
                    }
                    ImGui::PopItemWidth();
                    Gui::ItemTooltip("Map zoom level");
                }

                // Base map select
                {
                    ImGui::PushItemWidth(-1);
                    if (ImGui::BeginCombo("###BaseMap", _mapParams.title.c_str())) {
                        for (const auto& map : MapParams::BUILTIN_MAPS) {
                            auto entry = GuiGlobal::config_.maps.find(map.name);
                            if ((entry != GuiGlobal::config_.maps.end()) && entry->second) {
                                const bool selected = (map.name == _mapParams.name);
                                if (ImGui::Selectable(map.title.c_str(), selected)) {
                                    if (!selected) {
                                        SetMap(map);
                                    }
                                }
                                if (selected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();
                }

                // Show info box
                {
                    Gui::ToggleButton(ICON_FK_INFO "##ShowInfo", NULL, &cfg_.showInfo, "Showing info box",
                        "Not showing info box", GUI_VAR.iconSize);
                }

                ImGui::SameLine();

                // Tint colour
                {
                    if (ImGui::ColorEdit4("##ColourEdit", (float*)&tintColour4_,
                            ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoInputs)) {
                        cfg_.tintColour = ImGui::ColorConvertFloat4ToU32(tintColour4_);
                    }
                    Gui::ItemTooltip("Tint colour");
                }

                ImGui::SameLine();

                // Debug layout
                {
                    ImGui::Checkbox("##DebugLayout", &cfg_.debugLayout);
                    Gui::ItemTooltip("Debug map layout");
                }

                if (cfg_.debugLayout) {
                    ImGui::SameLine();

                    // Debug tiles
                    ImGui::Checkbox("##Debug", &cfg_.debugTiles);
                    Gui::ItemTooltip("Debug tiles");

                    ImGui::SameLine();

                    // Tile zoom level
                    ImGui::BeginDisabled(!cfg_.debugTiles);
                    int zLevel = zLevel_;
                    ImGui::PushItemWidth(-1);
                    if (ImGui::SliderInt("##TileZ", &zLevel, ZOOM_MIN, ZOOM_MAX)) {
                        _SetPosAndZoom(cfg_.centPosLonLat, cfg_.zoom, zLevel);
                    }
                    ImGui::PopItemWidth();
                    Gui::ItemTooltip("Tile zoom level");
                    ImGui::EndDisabled();
                }
            }
            // ImGui::EndChildFrame();
            ImGui::EndChild();
            _CHECK_HOVER();
        }
    }

    // Map links
    {
        const ImVec2 attr0{ canvasMin_.x + GUI_VAR.imguiStyle->ItemSpacing.x,
            canvasMax_.y - GUI_VAR.charSizeY - (3 * GUI_VAR.imguiStyle->ItemSpacing.y) };
        ImGui::SetCursorScreenPos(attr0);
        const std::string* link = nullptr;
        for (const auto& info : _mapParams.mapLinks) {
            if (ImGui::Button(info.label.c_str())) {
                link = &info.url;
            }
            if (ImGui::IsItemHovered()) {
                controlsHovered = true;
            }
            ImGui::SameLine();
        }
        if (link && !link->empty()) {
            std::string cmd = "xdg-open " + *link;
            if (std::system(cmd.c_str()) != EXIT_SUCCESS) {
                WARNING("Command failed: %s", cmd.c_str());
            }
        }
    }

    // Follow map mode
    if (followButtonEna_) {
        const ImVec2 attr0{ canvasMax_.x - GUI_VAR.imguiStyle->ItemSpacing.x - GUI_VAR.iconSize.x,
            canvasMax_.y - GUI_VAR.imguiStyle->ItemSpacing.y - GUI_VAR.iconSize.y };
        ImGui::SetCursorScreenPos(attr0);
        Gui::ToggleButton(
            ICON_FK_DOT_CIRCLE_O "##Follow", nullptr, &cfg_.followMode, "Follow position", nullptr, GUI_VAR.iconSize);
        if (ImGui::IsItemHovered()) {
            controlsHovered = true;
        }
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    auto& io = ImGui::GetIO();

    // Info box (part 1)
    Vec2f infoBoxCursor;
    if (cfg_.showInfo) {
        const Vec2f infoSize{ GUI_VAR.charSizeX * 23,
            (GUI_VAR.charSizeY * 3) + (2 * GUI_VAR.imguiStyle->ItemSpacing.y) };
        const Vec2f infoRectTopRight =
            Vec2f(canvasMax_.x - GUI_VAR.imguiStyle->ItemSpacing.x, canvasMin_.y + GUI_VAR.imguiStyle->ItemSpacing.y);
        const Vec2f infoRect0 = infoRectTopRight - Vec2f(infoSize.x, 0);
        const Vec2f infoRect1 = infoRectTopRight + Vec2f(0, infoSize.y);
        infoBoxCursor = infoRect0 + GUI_VAR.imguiStyle->FramePadding;
        draw->AddRectFilled(
            infoRect0, infoRect1, ImGui::GetColorU32(ImGuiCol_FrameBg), GUI_VAR.imguiStyle->FrameRounding);
        ImGui::SetCursorScreenPos(infoRect0);
        ImGui::InvisibleButton("infobox", infoRect1 - infoRect0, ImGuiButtonFlags_MouseButtonLeft);
        _CHECK_HOVER();
    }

    // Place an invisible button on top of everything to capture mouse events (and disable windows moving)
    // Note the real buttons above must are placed first so that they will get the mouse events first (before this
    // invisible button)
    ImGui::SetCursorScreenPos(canvasMin_);
    ImGui::InvisibleButton("canvas", canvasSize_, ImGuiButtonFlags_MouseButtonLeft);
    const bool isHovered = !controlsHovered && ImGui::IsItemHovered();
    const bool isActive = isHovered && (io.MouseDown[ImGuiMouseButton_Left] || io.MouseDown[ImGuiMouseButton_Right]);

    // Info box (part 2)
    if (cfg_.showInfo) {
        Vec2f pos(cfg_.centPosLonLat);  // use center pos
        if (isHovered && !isActive)     // unless hovering but not dragging
        {
            const Vec2f delta = (Vec2f(io.MousePos) - canvasCent_) / tileSize_;
            pos = MapTiles::TileXyToLonLat(centPosXy_ + delta, zLevel_);
        }

        ImGui::SetCursorScreenPos(infoBoxCursor);
        infoBoxCursor.y += GUI_VAR.charSizeY;

        if ((pos.y < MapParams::MAX_LAT_RAD) && (pos.y > MapParams::MIN_LAT_RAD)) {
            const DegMinSec dms(RadToDeg(pos.y));
            ImGui::Text(" %2d° %2d' %9.6f\" %c", std::abs(dms.deg_), dms.min_, dms.sec_, dms.deg_ < 0 ? 'S' : 'N');
        } else {
            ImGui::TextUnformatted("Lat: n/a");
        }
        ImGui::SetCursorScreenPos(infoBoxCursor);
        infoBoxCursor.y += GUI_VAR.charSizeY;
        if ((pos.x < MapParams::MAX_LON_RAD) && (pos.x > MapParams::MIN_LON_RAD)) {
            const DegMinSec dms(RadToDeg(pos.x));
            ImGui::Text("%3d° %2d' %9.6f\" %c", std::abs(dms.deg_), dms.min_, dms.sec_, dms.deg_ < 0 ? 'W' : 'E');
        } else {
            ImGui::TextUnformatted("Lon: n/a");
        }

        // Scale bar
        ImGui::SetCursorScreenPos(infoBoxCursor);
        infoBoxCursor.y += GUI_VAR.charSizeY;

        const double m2px = PixelPerMetre(pos.y);
        if (m2px > 0.0) {
            const float px2m = 1.0 / m2px;
            const float scaleLenPx = 100.0f;
            const float scaleLenM = scaleLenPx * px2m;

            const float n = std::floor(std::log10(scaleLenM));
            const float val = std::pow(10, n);
            if (val > 1000.0f) {
                ImGui::Text("%.0f km", val * 1e-3f);
            } else if (val < 1.0f) {
                ImGui::Text("%.2f m", val);
            } else {
                ImGui::Text("%.0f m", val);
            }
            const float len = std::round(val * m2px);
            const Vec2f offs =
                infoBoxCursor + Vec2f(GUI_VAR.charSizeX * 8, std::floor(-0.5f * GUI_VAR.charSizeY) - 2.0f);
            draw->AddRectFilled(offs, offs + Vec2f(len, 6), C_C_BLACK());
            draw->AddRect(offs, offs + Vec2f(len, 6), C_C_WHITE());
            // DEBUG("lenM=%f log=%f val=%f len=%f", scaleLenM, n, val, len);
        }
    }

    // Crosshairs (but not while dragging)
    if (isHovered && !isActive) {
        draw->AddLine(
            ImVec2(io.MousePos.x, canvasMin_.y), ImVec2(io.MousePos.x, canvasMax_.y), C_MAP_CROSSHAIRS(), 3.0);
        draw->AddLine(
            ImVec2(canvasMin_.x, io.MousePos.y), ImVec2(canvasMax_.x, io.MousePos.y), C_MAP_CROSSHAIRS(), 3.0);
    }

    // Detect drag start
    if (isHovered && (isDragging_ == ImGuiMouseButton_COUNT)) {
        if (io.MouseClicked[ImGuiMouseButton_Left]) {
            isDragging_ = ImGuiMouseButton_Left;
            dragStartXy_ = centPosXy_;
        } else if (io.MouseClicked[ImGuiMouseButton_Right]) {
            isDragging_ = ImGuiMouseButton_Right;
            // dragStartXy_ = centPosXy_;
        }
    }

    // Dragging left: move map
    else if (isDragging_ == ImGuiMouseButton_Left) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        const Vec2d delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        if ((delta.x != 0.0) || (delta.y != 0.0)) {
            const Vec2d pos = MapTiles::TileXyToLonLat(dragStartXy_ - (delta / tileSize_), zLevel_);
            _SetPosAndZoom(pos, cfg_.zoom, zLevel_);
        }
        if (io.MouseReleased[ImGuiMouseButton_Left]) {
            isDragging_ = ImGuiMouseButton_COUNT;
        }
    } else if (isDragging_ == ImGuiMouseButton_Right) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        const Vec2f delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);

        if ((delta.x != 0.0) && (delta.y != 0.0)) {
            draw->AddRectFilled(io.MousePos, Vec2f(io.MousePos) - delta, C_MAP_ZOOM_RECT());
            draw->AddRect(io.MousePos, Vec2f(io.MousePos) - delta, C_MAP_CROSSHAIRS());
        }

        if (io.MouseReleased[ImGuiMouseButton_Right]) {
            isDragging_ = ImGuiMouseButton_COUNT;
            if ((delta.x != 0.0) && (delta.y != 0.0)) {
                const Vec2f deltaMin = (Vec2f(io.MousePos) - delta - canvasCent_) / tileSize_;
                const Vec2f llMin = MapTiles::TileXyToLonLat(centPosXy_ + deltaMin, zLevel_);
                const Vec2f deltaMax = (Vec2f(io.MousePos) - canvasCent_) / tileSize_;
                const Vec2f llMax = MapTiles::TileXyToLonLat(centPosXy_ + deltaMax, zLevel_);
                const Vec2f llCent = (llMax + llMin) * 0.5;
                const float deltaLon = std::abs(llMax.x - llMin.x);
                const float deltaLat = std::abs(llMax.y - llMin.y);
                const float zoom = std::log(2 * M_PI / std::min(deltaLon, deltaLat)) / std::log(2.0f);
                _SetPosAndZoom(llCent, zoom + 0.5f);
            }
        }
    }

    // Zoom control using mouse wheel (but not while dragging)
    if (isHovered && !isActive) {
        if (io.MouseWheel != 0.0) {
            float step = ZOOM_STEP;
            if (io.KeyShift) {
                step = ZOOM_STEP_SHIFT;
            } else if (io.KeyCtrl) {
                step = ZOOM_STEP_CTRL;
            }

            // Position at mouse before zoom
            const Vec2f delta0 = (Vec2f(io.MousePos) - canvasCent_) / tileSize_;
            const Vec2f pos0 = MapTiles::TileXyToLonLat(centPosXy_ + delta0, zLevel_);

            // Zoom
            const float deltaZoom = io.MouseWheel * step;
            _SetPosAndZoom(cfg_.centPosLonLat, cfg_.zoom + deltaZoom, -1, step);

            // Position at mouse after zoom
            const Vec2f delta1 = (Vec2f(io.MousePos) - canvasCent_) / tileSize_;
            const Vec2f pos1 = MapTiles::TileXyToLonLat(centPosXy_ + delta1, zLevel_);

            // Update map position so that the original position is at the mouse position
            _SetPosAndZoom(Vec2f(cfg_.centPosLonLat) + (pos0 - pos1), cfg_.zoom, zLevel_, step);
        }
    }

    // Map layout debugging
    if (cfg_.debugLayout) {
        // Entire canvas
        draw->AddRect(canvasMin_, canvasMax_, C_MAP_DEBUG());

        // Centre
        draw->AddLine(canvasCent_ + Vec2f(0, -10), canvasCent_ + Vec2f(0, +10), C_MAP_DEBUG());
        draw->AddLine(canvasCent_ + Vec2f(-10, 0), canvasCent_ + Vec2f(+10, 0), C_MAP_DEBUG());
        draw->AddText(canvasCent_ + Vec2f(2, -GUI_VAR.charSizeY), C_MAP_DEBUG(),
            Sprintf("%.1f/%.1f", canvasCent_.x, canvasCent_.y).c_str());
        draw->AddText(canvasCent_ + Vec2f(2, 0), C_MAP_DEBUG(),
            Sprintf("%.6f/%.6f", RadToDeg(cfg_.centPosLonLat[0]), RadToDeg(cfg_.centPosLonLat[1])).c_str());
        draw->AddText(canvasCent_ + Vec2f(2, GUI_VAR.charSizeY), C_MAP_DEBUG(),
            Sprintf("%.1f/%.1f", centPosXy_.x, centPosXy_.y).c_str());

        // Top left
        draw->AddText(
            canvasMin_ + Vec2f(1, 1), C_MAP_DEBUG(), Sprintf("%.1f/%.1f", canvasMin_.x, canvasMin_.y).c_str());

        // Bottom right
        draw->AddText(canvasMax_ + Vec2f(-12 * GUI_VAR.charSizeX, -GUI_VAR.charSizeY - 1), C_MAP_DEBUG(),
            Sprintf("%.1f/%.1f", canvasSize_.x, canvasSize_.y).c_str());

        // draw->AddText(canvasCent + Vec2f(1,0), GUI_COLOURS(MAP_DEBUG), Sprintf("%.1fx%.1f (%.1f)", tileSize_.x,
        // tileSize_.y, _tileScale).c_str()); draw->AddText(canvasCent + ImVec2(1,15), GUI_COLOURS(MAP_DEBUG),
        // Sprintf("%d %.1f", _zoomLevel, cfg_.zoom).c_str());

        // Bottom right
        if (mapTiles_) {
            const auto stats = mapTiles_->GetStats();
            draw->AddText({ canvasMin_.x + 1, canvasMax_.y - GUI_VAR.charSizeY - 1 }, C_MAP_DEBUG(),
                Sprintf("#tiles %d (%d load, %d avail, %d fail, %d outside)", stats.numTiles, stats.numLoad,
                    stats.numAvail, stats.numFail, stats.numOutside)
                    .c_str());
        }
    }

    draw->PopClipRect();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetMap::EnableFollowButton()
{
    followButtonEna_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetMap::FollowEnabled()
{
    return cfg_.followMode;
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
// eof
