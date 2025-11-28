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

#include <algorithm>
#include <cstdlib>

#include <fpsdk_common/time.hpp>
#include <fpsdk_common/trafo.hpp>

#include "ffgui_inc.hpp"

#include "gui_win_data_map.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::time;
using namespace fpsdk::common::trafo;

GuiWinDataMap::GuiWinDataMap(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 80, 40, 0, 0 }, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse, input),
    map_   { WinName() }  // clang-format on
{
    DEBUG("GuiWinDataMap(%s)", WinName().c_str());
    toolbarEna_ = false;
    latestEpochMaxAge_ = {};
    ClearData();
    map_.EnableFollowButton();
}

GuiWinDataMap::~GuiWinDataMap()
{
    DEBUG("~GuiWinDataMap(%s)", WinName().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataMap::_ProcessData(const DataPtr& /*data*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataMap::_Loop(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataMap::_ClearData()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataMap::_DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataMap::_DrawContent()
{
    if (!map_.BeginDraw()) {
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Draw points
    input_->database_->ProcRows([&](const Database::Row& row) {
        if (row.pos_avail) {
            const Vec2f xy = map_.LatLonToScreen(row.pos_llh_lat, row.pos_llh_lon);
            draw->AddRectFilled(xy - Vec2f(2, 2), xy + Vec2f(2, 2), row.fix_colour);
        }
        return true;
    });

    // Highlight last point, draw accuracy estimate circle
    Database::Row last_row;
    input_->database_->ProcRows(
        [&](const Database::Row& row) {
            if (row.pos_avail) {
                const Vec2f xy = map_.LatLonToScreen(row.pos_llh_lat, row.pos_llh_lon);

                const ImU32 c = row.fix_ok ? C_PLOT_MAP_HL_OK() : C_PLOT_MAP_HL_MASKED();
                const double m2px = map_.PixelPerMetre(row.pos_llh_lat);
                const float r = (0.5 * row.pos_acc_horiz * m2px) + 0.5;
                if (r > 1.0f) {
                    draw->AddCircleFilled(xy, r, C_PLOT_MAP_ACC_EST());
                }
                const double age = (Time::FromClockRealtime() - row.time_in).GetSec();
                const float w = age < 0.1 ? 3.0 : 1.0;
                const float l = 20.0;  // age < 100 ? 40.0 : 20.0; //20 + ((100 - std::min(age, 100)) / 2);

                draw->AddLine(xy - ImVec2(l, 0), xy - ImVec2(3, 0), c, w);
                draw->AddLine(xy + ImVec2(3, 0), xy + ImVec2(l, 0), c, w);
                draw->AddLine(xy - ImVec2(0, l), xy - ImVec2(0, 3), c, w);
                draw->AddLine(xy + ImVec2(0, 3), xy + ImVec2(0, l), c, w);

                last_row = row;
                return false;
            }
            return true;
        },
        true);

    // Draw approximate position of the basestation
    if (last_row.pos_avail && last_row.relpos_avail) {
        // FIXME: this isn't very accurate
        auto xyz = TfEcefNed({ -last_row.relpos_ned_north, -last_row.relpos_ned_east, -last_row.relpos_ned_down },
            { last_row.pos_llh_lat, last_row.pos_llh_lon, last_row.pos_llh_height });
        auto llh = TfWgs84LlhEcef(xyz);

        const Vec2f xyBase = map_.LatLonToScreen(llh.x(), llh.y());
        const Vec2f xyRover = map_.LatLonToScreen(last_row.pos_llh_lat, last_row.pos_llh_lon);
        draw->AddLine(xyBase, xyRover, C_PLOT_MAP_BASELINE(), 4.0f);
        draw->AddCircleFilled(xyBase, 6.0f, C_PLOT_MAP_BASELINE());
    }

    if (last_row.pos_avail && map_.FollowEnabled()) {
        map_.SetPos(last_row.pos_llh_lat, last_row.pos_llh_lon);
    }

    map_.EndDraw();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
