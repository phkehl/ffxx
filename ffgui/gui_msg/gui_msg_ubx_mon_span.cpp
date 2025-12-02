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
// See the GNU General Public License for more detailspect.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>.

//
#include "ffgui_inc.hpp"
//
#include <fpsdk_common/parser/ubx.hpp>
using namespace fpsdk::common::parser::ubx;
//
#include "gui_msg_ubx_mon_span.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxMonSpan::GuiMsgUbxMonSpan(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input),
    tabbar_   { viewName_ }  // clang-format on
{
    Clear();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonSpan::Update(const DataMsgPtr& msg)
{
    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if ((UBX_MON_SPAN_VERSION(data) != UBX_MON_SPAN_V0_VERSION) || (size < UBX_MON_SPAN_V0_SIZE(data))) {
        return;
    }

    UBX_MON_SPAN_V0_GROUP0 head;
    std::memcpy(&head, &data[UBX_HEAD_SIZE], sizeof(head));
    numSpects_ = std::min<std::size_t>(head.numRfBlocks, spects_.size());

    static_assert(NUM_BINS == UBX_MON_SPAN_V0_NUM_BINS, "");

    for (std::size_t spectIx = 0, offs = UBX_HEAD_SIZE + sizeof(UBX_MON_SPAN_V0_GROUP0); spectIx < numSpects_;
        spectIx++, offs += sizeof(UBX_MON_SPAN_V0_GROUP1)) {
        UBX_MON_SPAN_V0_GROUP1 block;
        std::memcpy(&block, &data[offs], sizeof(block));
        auto& spect = spects_[spectIx];
        const std::size_t numBins = spect.freq_.size();
        spect.centre_ = (float)block.center * 1e-6;  // [MHz]
        spect.span_ = (float)block.span * 1e-6;      // [MHz]
        spect.res_ = (float)block.res * 1e-6;        // [MHz]
        spect.pga_ = (float)block.pga;
        for (std::size_t binIx = 0; binIx < numBins; binIx++) {
            spect.freq_[binIx] =
                (UBX_MON_SPAN_BIN_CENT_FREQ(block.center, block.span, binIx) * 1e-6) - spect.centre_;  // [MHz]
            const float ampl = (float)block.spectrum[binIx] * (100.0 / 255.0);                       // [%]
            spect.ampl_[binIx] = ampl;
            if (std::isnan(spect.min_[binIx]) || (ampl < spect.min_[binIx])) {
                spect.min_[binIx] = ampl;
            }
            if (std::isnan(spect.max_[binIx]) || (ampl > spect.max_[binIx])) {
                spect.max_[binIx] = ampl;
            }
            if (spect.count_ > 0.0) {
                spect.mean_[binIx] += (ampl - spect.mean_[binIx]) / spect.count_;
            } else {
                spect.mean_[binIx] = ampl;
            }
        }
        spect.count_ += 1.0;

        spect.tab_ = Sprintf("RF%" PRIuMAX "(%.0fMHz)##RF%" PRIuMAX, spectIx, spect.centre_, spectIx);
        spect.title_ = Sprintf("RF%" PRIuMAX " (centre %.6f MHz, span %.2f MHz, PGA %.0f dB)###%p", spectIx, spect.centre_, spect.span_, spect.pga_, std::addressof(spect));

        std::memmove(&spect.heat_[NUM_BINS], &spect.heat_[0], sizeof(float) * NUM_BINS * (NUM_HEAT - 1));
        std::memcpy(&spect.heat_[0], spect.ampl_.data(), sizeof(float) * NUM_BINS);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonSpan::Update(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonSpan::Clear()
{
    for (auto& spect : spects_) {
        spect.freq_.fill(0.0);
        spect.ampl_.fill(0.0);
        spect.min_.fill(NAN);
        spect.max_.fill(NAN);
        spect.mean_.fill(NAN);
        spect.heat_.fill(0.0f);
    }
    numSpects_ = 0;
    resetPlotRange_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonSpan::DrawToolbar()
{
    ImGui::SameLine();
    ImGui::BeginDisabled(spects_.empty());
    if (ImGui::Button(ICON_FK_ARROWS_ALT "###ResetPlotRange")) {
        resetPlotRange_ = true;
    }
    ImGui::EndDisabled();
    Gui::ItemTooltip("Reset plot range");
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonSpan::DrawMsg()
{

    if (numSpects_ == 0) {
        return;
    }
    std::size_t spectIx0 = 0;
    std::size_t spectIx1 = numSpects_ - 1;
    if (tabbar_.Begin()) {
        tabbar_.Item("All");
        for (std::size_t spectIx = 0; spectIx < numSpects_; spectIx++) {
            if (tabbar_.Item(spects_[spectIx].tab_)) {
                spectIx0 = spectIx;
                spectIx1 = spectIx;
            }
        }

        tabbar_.End();
    }

    Vec2f plotSize = ImGui::GetContentRegionAvail();
    plotSize.y -= (float)(spectIx1 - spectIx0 + 1 - 1) * GUI_VAR.imguiStyle->ItemInnerSpacing.y;
    plotSize.y /= (float)(spectIx1 - spectIx0 + 1);
    for (std::size_t spectIx = spectIx0; spectIx <= spectIx1; spectIx++) {
        _DrawSpect(spects_[spectIx], plotSize);
    }
    resetPlotRange_ = false;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonSpan::_DrawSpect(const SpectData &spect, const ImVec2 size)
{
    if (ImPlot::BeginSubplots(spect.title_.c_str(), 2, 1, size, ImPlotSubplotFlags_LinkAllX)) {
        const float xmin = -0.50f * spect.span_;
        const float xmax =  0.51f * spect.span_;

        // Spectrum
        if (ImPlot::BeginPlot("##Spectrum")) {

            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels);
            ImPlot::SetupAxis(ImAxis_Y1, "Amplitude [%]");
            const float ymin =  1.0f;
            const float ymax = 99.0f;
            if (resetPlotRange_) {
                ImPlot::SetupAxisLimits(ImAxis_X1, xmin, xmax, ImPlotCond_Always);
                ImPlot::SetupAxisLimits(ImAxis_Y1, ymin, ymax, ImPlotCond_Always);
            }
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, xmin, xmax);
            ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, ymin, ymax);

            ImPlot::SetupFinish();

            // Labels
            const ImPlotRect limits = ImPlot::GetPlotLimits();
            for (auto& label : FREQ_LABELS) {
                const ImVec2 offs(0.5f * GUI_VAR.charSizeY, -GUI_VAR.charSizeX);
                if (std::fabs(spect.centre_ - label.freq_) < spect.span_) {
                    float freq = label.freq_ - spect.centre_;
                    ImPlot::PushStyleColor(ImPlotCol_Line, ImPlot::GetColormapColor(0));
                    ImPlot::PlotInfLines(label.id_, &freq, 1);
                    ImPlot::PopStyleColor();
                    ImPlot::PushStyleColor(ImPlotCol_InlayText, ImPlot::GetColormapColor(0));
                    ImPlot::PlotText(
                        label.title_, freq, limits.Y.Min, offs, ImPlotTextFlags_Vertical | ImPlotTextFlags_AlignLeft);
                    ImPlot::PopStyleColor();
                }
            }

            // Amplitude with min/max
            ImPlot::PushStyleColor(ImPlotCol_Line, ImPlot::GetColormapColor(1, ImPlotColormap_Deep));
            ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.3f);
            ImPlot::PlotShaded(
                "##Amplitude", spect.freq_.data(), spect.max_.data(), spect.min_.data(), spect.freq_.size());
            ImPlot::PlotLine("##Amplitude", spect.freq_.data(), spect.ampl_.data(), spect.freq_.size());
            ImPlot::PopStyleVar();
            ImPlot::PopStyleColor();
            // Mean, initially hidden
            ImPlot::HideNextItem();
            ImPlot::PushStyleColor(ImPlotCol_Line, ImPlot::GetColormapColor(2, ImPlotColormap_Deep));
            ImPlot::PlotLine("Mean", spect.freq_.data(), spect.mean_.data(), spect.freq_.size());
            ImPlot::PopStyleColor();

            ImPlot::EndPlot();
        }
        if (ImPlot::BeginPlot("##Heatmap", {-1, 0}, ImPlotFlags_NoLegend)) {
            ImPlot::PushColormap(ImPlotColormap_Plasma);  // Jet, Viridis, Plasma
            ImPlot::SetupAxis(ImAxis_X1, "Frequency [MHz]");
            ImPlot::SetupAxis(ImAxis_Y1, "Time", ImPlotAxisFlags_Invert);
            // const float xmin = -0.50f * spect.span_;
            // const float xmax =  0.51f * spect.span_;
            const float ymin =  0.0f;
            const float ymax = (float)NUM_HEAT;
            if (resetPlotRange_) {
                ImPlot::SetupAxisLimits(ImAxis_X1, xmin, xmax, ImPlotCond_Always);
                ImPlot::SetupAxisLimits(ImAxis_Y1, ymin, ymax, ImPlotCond_Always);
            }
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, xmin, xmax);
            ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, ymin, ymax);
            ImPlot::SetupFinish();

            ImPlot::PlotHeatmap<float>("ampl", spect.heat_.data(), NUM_HEAT, UBX_MON_SPAN_V0_NUM_BINS, 0.0f, 100.0f,
                nullptr, { -0.5 * spect.span_, NUM_HEAT }, { 0.5 * spect.span_, 0 }, ImPlotHeatmapFlags_None);
            ImPlot::PopColormap();
            ImPlot::EndPlot();
        }
        ImPlot::EndSubplots();
    }
}

/* ****************************************************************************************************************** */
} // ffgui
