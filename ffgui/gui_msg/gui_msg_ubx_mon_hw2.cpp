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
#include <fpsdk_common/parser/ubx.hpp>
using namespace fpsdk::common::parser::ubx;
//
#include "gui_msg_ubx_mon_hw2.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxMonHw2::GuiMsgUbxMonHw2(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input)  // clang-format on
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw2::Update(const DataMsgPtr& msg)
{
    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if (size != UBX_MON_HW2_V0_SIZE) {
        return;
    }

    UBX_MON_HW2_V0_GROUP0 hw;
    std::memcpy(&hw, &data[UBX_HEAD_SIZE], sizeof(hw));

    iqs_.push_back({ (float)hw.magI * (1.0f / 255.0f), (float)hw.magQ * (1.0f / 255.0f),
        (float)hw.ofsI * (2.0f / 255.0f), (float)hw.ofsQ * (2.0f / 255.0f) });

    while (iqs_.size() > MAX_IQS) {
        iqs_.pop_front();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw2::Update(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw2::Clear()
{
    iqs_.clear();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw2::DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw2::DrawMsg()
{
    DrawIQ(iqs_);
}

/* static */ void GuiMsgUbxMonHw2::DrawIQ(const std::list<IQ>& iqs, const ImVec2& size)
{
    if (ImGui::BeginChild("##iqplot", size)) {
        // Canvas
        const Vec2f offs = ImGui::GetCursorScreenPos();
        const Vec2f cent = Vec2f(offs.x + std::floor(size.x * 0.5), offs.y + std::floor(size.y * 0.5));

        // ImGui::Text("I: %5.1f%% @ %+5.1f%%", iqLatest.magI * 1e2f, iqLatest.offsI * 1e2f);
        // ImGui::Text("Q: %5.1f%% @ %+5.1f%%", iqLatest.magQ * 1e2f, iqLatest.offsQ * 1e2f);

        const IQ& iqLatest = iqs.back();
        ImGui::Text("I: %5.1f%%", iqLatest.magI_ * 1e2f);
        ImGui::Text("  %+6.1f%%", iqLatest.offsI_ * 1e2f);
        ImGui::Text("Q: %5.1f%%", iqLatest.magQ_ * 1e2f);
        ImGui::Text("  %+6.1f%%", iqLatest.offsQ_ * 1e2f);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float radiusPx = 0.4f * std::min(size.x, size.y);

        // Draw grid
        draw->AddLine(cent - ImVec2(radiusPx, 0), cent + ImVec2(radiusPx, 0), C_PLOT_GRID_MAJOR());
        draw->AddLine(cent - ImVec2(0, radiusPx), cent + ImVec2(0, radiusPx), C_PLOT_GRID_MAJOR());
        draw->AddCircle(cent, 1.00f * radiusPx, C_PLOT_GRID_MAJOR(), 0);
        draw->AddCircle(cent, 0.25f * radiusPx, C_PLOT_GRID_MINOR(), 0);
        draw->AddCircle(cent, 0.50f * radiusPx, C_PLOT_GRID_MINOR(), 0);
        draw->AddCircle(cent, 0.75f * radiusPx, C_PLOT_GRID_MINOR(), 0);

        // Labels
        const ImVec2 charSize = GUI_VAR.charSize;
        ImGui::SetCursorScreenPos(cent + ImVec2(radiusPx + charSize.x, -0.5 * charSize.y));
        ImGui::TextUnformatted("+I");
        ImGui::SetCursorScreenPos(cent - ImVec2(radiusPx + (3 * charSize.x), +0.5 * charSize.y));
        ImGui::TextUnformatted("-I");
        ImGui::SetCursorScreenPos(cent + ImVec2(-charSize.x, radiusPx));
        ImGui::TextUnformatted("-Q");
        ImGui::SetCursorScreenPos(cent + ImVec2(-charSize.x, -radiusPx - charSize.y));
        ImGui::TextUnformatted("+Q");

        // Ellipses
        auto col4bg = C4_C_RED();
        col4bg.w = 0.5f;
        const ImU32 colBg = ImGui::ColorConvertFloat4ToU32(col4bg);
        for (auto& iq : iqs) {
            const bool isLatest = (&iq == &iqs.back());
            const float lw = isLatest ? 3.0f : 2.0f;
            const ImU32 col = isLatest ? C_C_BRIGHTORANGE() : colBg;

            const float radiusX = radiusPx * iq.magQ_;
            const float radiusY = radiusPx * iq.magI_;
            const Vec2f offsXY = Vec2f(iq.offsQ_ * radiusPx, -iq.offsI_ * radiusPx);
            const Vec2f centXY = cent + offsXY;
            draw->AddLine(centXY - Vec2f(radiusX, 0), centXY + Vec2f(radiusX, 0), col, lw);
            draw->AddLine(centXY - Vec2f(0, radiusY), centXY + Vec2f(0, radiusY), col, lw);
            ImVec2 points[30];
            for (std::size_t ix = 0; ix < NumOf(points); ix++) {
                const double t = (double)ix * (2 * M_PI / (double)NumOf(points));
                const double dx = radiusX * std::cos(t);
                const double dy = radiusY * std::sin(t);
                points[ix] = centXY + Vec2f(dx, dy);
            }
            draw->AddPolyline(points, (int)NumOf(points), col, true, lw);
        }
    }
    ImGui::EndChild();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
