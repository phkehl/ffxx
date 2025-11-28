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
#include "gui_msg_ubx_tim_tm2.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxTimTm2::GuiMsgUbxTimTm2(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input)  // clang-format on
{
    GuiGlobal::LoadObj(viewName_, cfg_);
}

GuiMsgUbxTimTm2::~GuiMsgUbxTimTm2()
{
    GuiGlobal::SaveObj(viewName_, cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxTimTm2::Update(const DataMsgPtr& msg)
{
    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if (size != UBX_TIM_TM2_V0_SIZE) {
        return;
    }

    UBX_TIM_TM2_V0_GROUP0 tm2;
    std::memcpy(&tm2, &data[UBX_HEAD_SIZE], sizeof(tm2));

    const bool newRisingEdge = UBX_TIM_TM2_V0_FLAGS_NEWRISINGEDGE(tm2.flags);
    const bool newFallingEdge = UBX_TIM_TM2_V0_FLAGS_NEWFALLINGEDGE(tm2.flags);
    const double towRisingEdge =
        ((double)tm2.towMsR * UBX_TIM_TM2_V0_TOW_SCALE) + ((double)tm2.towSubMsR * UBX_TIM_TM2_V0_SUBMS_SCALE);
    const double towFallingEdge =
        ((double)tm2.towMsF * UBX_TIM_TM2_V0_TOW_SCALE) + ((double)tm2.towSubMsF * UBX_TIM_TM2_V0_SUBMS_SCALE);

    if (newRisingEdge) {
        if (!std::isnan(lastRisingEdge_)) {
            periodRisingEdge_ = towRisingEdge - lastRisingEdge_;
        }
        lastRisingEdge_ = towRisingEdge;
    }
    if (newFallingEdge) {
        if (!std::isnan(lastFallingEdge_)) {
            periodFallingEdge_ = towFallingEdge - lastFallingEdge_;
        }
        lastFallingEdge_ = towFallingEdge;

        if (!std::isnan(lastRisingEdge_) && !std::isnan(periodRisingEdge_) && (periodRisingEdge_ > 1e-9)) {
            dutyCycle_ = (lastFallingEdge_ - lastRisingEdge_) / periodRisingEdge_ * 1e2;
        }
    }

    // Edges can come in single messages or both in one message (in arbitrary order)
    if (newRisingEdge && newFallingEdge) {
        if (towRisingEdge < towFallingEdge) {
            plotData_.push_back({ towRisingEdge,  true,  periodRisingEdge_, periodFallingEdge_, dutyCycle_ });
            plotData_.push_back({ towFallingEdge, false, periodRisingEdge_, periodFallingEdge_, dutyCycle_ });
        } else {
            plotData_.push_back({ towFallingEdge, false, periodRisingEdge_, periodFallingEdge_, dutyCycle_ });
            plotData_.push_back({ towRisingEdge,  true,  periodRisingEdge_, periodFallingEdge_, dutyCycle_ });
        }
    } else if (newRisingEdge || newFallingEdge) {
        plotData_.push_back({ newRisingEdge ? towRisingEdge : towFallingEdge, newRisingEdge, periodRisingEdge_,
            periodFallingEdge_, dutyCycle_ });
    }

    while (plotData_.size() > MAX_PLOT_DATA) {
        plotData_.pop_front();
    }

    const double tow = plotData_.back().tow_;

    // Set x range on first data
    if (plotRangeX_.Size() == 0.0) {
        plotRangeX_.Min = tow - 5.0;
        plotRangeX_.Max = tow + 115.0;
        setRangeX_ = true;
    }

    // move xrange on new data
    if (cfg_.fitMode == FitMode::FOLLOW_X) {
        if (!std::isnan(lastTow_)) {
            const double dTow = tow - lastTow_;
            plotRangeX_.Min += dTow;
            plotRangeX_.Max += dTow;
        }
        lastTow_ = tow;
        setRangeX_ = true;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxTimTm2::Update(const Time& now)
{
    UNUSED(now);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxTimTm2::Clear()
{
    plotData_.clear();
    lastRisingEdge_ = NAN;
    lastFallingEdge_ = NAN;
    periodRisingEdge_ = NAN;
    periodFallingEdge_ = NAN;
    dutyCycle_ = NAN;
    plotRangeX_ = {};
    lastTow_ = NAN;
    setRangeX_ = false;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxTimTm2::DrawToolbar()
{
    Gui::VerticalSeparator();

    // Fit mode
    const bool fitFollowX = (cfg_.fitMode == FitMode::FOLLOW_X);
    const bool fitAutofit = (cfg_.fitMode == FitMode::AUTOFIT);
    // - Follow X
    if (!fitFollowX) { ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM()); }
    if (ImGui::Button(ICON_FK_ARROW_RIGHT "##FollowX", GUI_VAR.iconSize)) {
        cfg_.fitMode = (fitFollowX ? FitMode::NONE : FitMode::FOLLOW_X);
    }
    if (!fitFollowX) { ImGui::PopStyleColor(); }
    Gui::ItemTooltip("Follow x (automatically move x axis)");
    ImGui::SameLine();
    // - Autofit
    if (!fitAutofit) { ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM()); }
    if (ImGui::Button(ICON_FK_ARROWS_ALT "##Autofit", GUI_VAR.iconSize)) {
        cfg_.fitMode = (fitAutofit ? FitMode::NONE : FitMode::AUTOFIT);
    }
    if (!fitAutofit) { ImGui::PopStyleColor(); }
    Gui::ItemTooltip("Automatically fit axes ranges to data");
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxTimTm2::DrawMsg()
{
    if (plotData_.empty()) {
        return;
    }

    if (cfg_.fitMode ==  FitMode::AUTOFIT) {
        ImPlot::SetNextAxesToFit();
    }

    auto plotSize = ImGui::GetContentRegionAvail();
    const float maxHeight = GUI_VAR.lineHeight * 20.0f;
    if (plotSize.y > maxHeight) {
        plotSize.y = maxHeight;
    }

    if (ImPlot::BeginPlot("##plot", plotSize, ImPlotFlags_Crosshairs | ImPlotFlags_NoFrame | ImPlotFlags_NoMenus)) {
        ImPlot::SetupAxis(ImAxis_X1, "TOW [s]");
        if (setRangeX_) {
            ImPlot::SetupAxisLimits(ImAxis_X1, plotRangeX_.Min, plotRangeX_.Max, ImPlotCond_Always);
            setRangeX_ = false;
        }

        ImPlot::SetupAxis(ImAxis_Y1, "Duty [%]", ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -5.0, 105.0, ImPlotCond_Always);

        ImPlot::SetupAxis(ImAxis_Y2, "Period [s] (Frequency [Hz])", ImPlotAxisFlags_Opposite | ImPlotAxisFlags_NoGridLines);
        ImPlot::SetupAxisLimits(ImAxis_Y2, 0.5, 2.5, ImPlotCond_Once);
        ImPlot::SetupAxisLimitsConstraints(ImAxis_Y2, 0.0, 3600.0);
        ImPlot::SetupAxisFormat(ImAxis_Y2, [](const double value, char* str, const int size, void*) {
            return std::snprintf(str, size, "%g (%g)", value, value > 0.0 ? 1.0 / value : NAN); });

        ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);

        ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
        ImPlot::PlotLineG(
            "Period rising",
            [](int ix, void* arg) {
                const PlotData& pd = ((const std::deque<PlotData>*)arg)->at(ix);
                return ImPlotPoint(pd.tow_, pd.pRising_);
            },
            &plotData_, plotData_.size());
        ImPlot::PlotLineG(
            "Period falling",
            [](int ix, void* arg) {
                const PlotData& pd = ((const std::deque<PlotData>*)arg)->at(ix);
                return ImPlotPoint(pd.tow_, pd.pFalling_);
            },
            &plotData_, plotData_.size());

        ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
        ImPlot::PlotLineG(
            "Duty cycle",
            [](int ix, void* arg) {
                const PlotData& pd = ((const std::deque<PlotData>*)arg)->at(ix);
                return ImPlotPoint(pd.tow_, pd.dutyCycle_);
            },
            &plotData_, plotData_.size());
        ImPlot::PlotStairsG(
            "Signal",
            [](int ix, void* arg) {
                const PlotData& pd = ((const std::deque<PlotData>*)arg)->at(ix);
                return ImPlotPoint(pd.tow_, pd.signal_ ? 100.0 : 0.0);
            },
            &plotData_, plotData_.size());

        plotRangeX_ = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y1).X;

        ImPlot::EndPlot();
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
