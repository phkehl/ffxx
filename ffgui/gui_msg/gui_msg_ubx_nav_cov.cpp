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
#include "gui_msg_ubx_nav_cov.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxNavCov::GuiMsgUbxNavCov(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input)  // clang-format on
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxNavCov::Update(const DataMsgPtr& msg)
{
    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if ((UBX_NAV_COV_VERSION(data) != UBX_NAV_COV_V0_VERSION) || (size != UBX_NAV_COV_V0_SIZE)) {
        return;
    }

    UBX_NAV_COV_V0_GROUP0 cov;
    std::memcpy(&cov, &data[UBX_HEAD_SIZE], sizeof(cov));

    // clang-format off
    covPos_ = {{ cov.posCovNN, cov.posCovNE, cov.posCovND,
                 cov.posCovNE, cov.posCovEE, cov.posCovED,
                 cov.posCovND, cov.posCovED, cov.posCovDD }};
    covVel_ = {{ cov.velCovNN, cov.velCovNE, cov.velCovND,
                 cov.velCovNE, cov.velCovEE, cov.velCovED,
                 cov.velCovND, cov.velCovED, cov.velCovDD }}; // clang-format on
    covPosValid_ = cov.posCovValid;
    covVelValid_ = cov.velCovValid;
    valid_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxNavCov::Update(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxNavCov::Clear()
{
    covPos_.fill(0.0f);
    covVel_.fill(0.0f);
    valid_ = false;
    covPosValid_ = false;
    covVelValid_ = false;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxNavCov::DrawToolbar()
{

}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxNavCov::DrawMsg()
{
    if (!valid_) {
        return;
    }

    const ImPlotAxisFlags axesFlags = ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks;
    const ImPlotFlags plotFlags = ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText | ImPlotFlags_NoMenus |
                                  ImPlotFlags_NoBoxSelect | ImPlotFlags_NoFrame;
    static constexpr std::array<const char*, 3> xyLabels = { { "N", "E", "D" } };
    const auto sizeAvail = ImGui::GetContentRegionAvail();
    const float plotWidth = 0.5f * (sizeAvail.x - GUI_VAR.imguiStyle->ItemSpacing.x);
    const ImVec2 plotSize(std::min(plotWidth, sizeAvail.y), std::min(plotWidth, sizeAvail.y));

    ImPlot::PushColormap(ImPlotColormap_Cool); // ImPlotColormap_Plasma

    if (ImPlot::BeginPlot("Position NED [m^2]##pos", plotSize, plotFlags)) {
        ImPlot::SetupAxisTicks(ImAxis_X1, 0 + 1.0 / 6.0, 1 - 1.0 / 6.0, 3, xyLabels.data());
        ImPlot::SetupAxisTicks(ImAxis_Y1, 1 - 1.0 / 6.0, 0 + 1.0 / 6.0, 3, xyLabels.data());
        ImPlot::SetupAxis(ImAxis_X1, nullptr, axesFlags);
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, axesFlags);
        ImPlot::SetupFinish();
        ImPlot::PlotHeatmap(
            "heat", covPosValid_ ? covPos_.data() : COV_INVALID.data(), 3, 3, 0.0f, 0.0f, "%.1e");
        ImPlot::EndPlot();
    }

    ImGui::SameLine();

    if (ImPlot::BeginPlot("Velocity NED [m^2/s^2]##vel", plotSize, plotFlags)) {
        ImPlot::SetupAxisTicks(ImAxis_X1, 0 + 1.0 / 6.0, 1 - 1.0 / 6.0, 3, xyLabels.data());
        ImPlot::SetupAxisTicks(ImAxis_Y1, 1 - 1.0 / 6.0, 0 + 1.0 / 6.0, 3, xyLabels.data());
        ImPlot::SetupAxis(ImAxis_X1, nullptr, axesFlags);
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, axesFlags);
        ImPlot::SetupFinish();
        ImPlot::PlotHeatmap(
            "heat", covVelValid_ ? covVel_.data() : COV_INVALID.data(), 3, 3, 0.0f, 0.0f, "%.1e");
        ImPlot::EndPlot();
    }

    ImPlot::PopColormap();
}

/* ****************************************************************************************************************** */
} // ffgui
