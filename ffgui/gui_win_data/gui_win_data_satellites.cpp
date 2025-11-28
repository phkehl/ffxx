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
//
#include <fpsdk_common/math.hpp>
#include <fpsdk_common/string.hpp>
//
#include "gui_win_data_satellites.hpp"
#include "gui_win_data_signals.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::math;
using namespace fpsdk::common::string;

GuiWinDataSatellites::GuiWinDataSatellites(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 80, 25, 0, 0 }, ImGuiWindowFlags_None, input),
    tabbarMode_     { WinName() + "Mode" },
    tabbarFilter_   { WinName() + "Filter", ImGuiTabBarFlags_FittingPolicyShrink | ImGuiTabBarFlags_TabListPopupButton },
    table_          { WinName() + "List" }  // clang-format on
{
    DEBUG("GuiWinDataSatellites(%s)", WinName().c_str());

    GuiGlobal::LoadObj(WinName() + ".GuiWinDataSatellites", cfg_);

    ClearData();

    table_.AddColumn("SV", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE);
    table_.AddColumn("Azim", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE | GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Elev", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE | GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Orbit", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE);
    table_.AddColumn("Signal L1", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE);
    table_.AddColumn("Signal E6", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE);
    table_.AddColumn("Signal L2", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE);
    table_.AddColumn("Signal L5", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE);
}

GuiWinDataSatellites::~GuiWinDataSatellites()
{
    DEBUG("~GuiWinDataSatellites(%s)", WinName().c_str());
    GuiGlobal::SaveObj(WinName() + ".GuiWinDataSatellites", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSatellites::_ProcessData(const DataPtr& data)
{
    if (data->type_ == DataType::EPOCH) {
        epoch_ = DataPtrToDataEpochPtr(data);
        newEpochAvail_ = true;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSatellites::_Loop(const Time& /*now*/)
{
    if (newEpochAvail_) {
        _UpdateSatellites();
        newEpochAvail_ = false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSatellites::_ClearData()
{
    epoch_.reset();
    sats_.clear();
    _UpdateSatellites();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSatellites::_DrawToolbar()
{
    Gui::VerticalSeparator();

    ImGui::BeginDisabled(!doSky_);
    Gui::ToggleButton(ICON_FK_ARROWS_H, ICON_FK_ARROWS_V, &cfg_.sizingMax, "Sizing: max h/v",
        "Sizing: min h/v", GUI_VAR.iconSize);
    ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSatellites::_DrawContent()
{
    doList_ = false;
    doSky_ = false;

    if (tabbarMode_.Begin()) {
        if (tabbarMode_.Item("List")) {
            doList_ = true;
        };
        if (tabbarMode_.Item("Sky")) {
            doSky_ = true;
        };
        tabbarMode_.End();
    }

    ImGui::SameLine();

    const int filterIx = filterIx_;

    if (tabbarFilter_.Begin()) {
        for (auto& f : FILTERS) {
            if (tabbarFilter_.Item(doList_ ? counts_[f.ix].labelList_ : counts_[f.ix].labelSky_)) {
                filterIx_ = f.ix;
            }
        }
        tabbarFilter_.End();
    }

    if (filterIx != filterIx_) {
        _UpdateSatellites();
    }

    if (doSky_) {
        _DrawSky();
    } else if (doList_) {
        table_.DrawTable();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ void GuiWinDataSatellites::_DrawSignalLevelCb(const void* arg)
{
    const SigInfo* sig = (const SigInfo*)arg;
    ImGui::Text("%-4s %4.1f", sig->signalStrShort, sig->cno);
    ImGui::SameLine();
    GuiWinDataSignals::DrawSignalLevelBar(*sig);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSatellites::_DrawSky()
{
    ImGui::BeginChild("##Plot", ImVec2(0.0f, 0.0f), false,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Canvas
    const Vec2f canvasOffs = ImGui::GetCursorScreenPos();
    const Vec2f canvasSize = ImGui::GetContentRegionAvail();
    const Vec2f canvasCent{ canvasOffs.x + std::floor(canvasSize.x * 0.5f),
        canvasOffs.y + std::floor(canvasSize.y * 0.5f) };

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(canvasOffs, canvasOffs + canvasSize);

    // const ImVec2 cursorOrigin = ImGui::GetCursorPos();
    // const ImVec2 cursorMax = ImGui::GetWindowContentRegionMax();

    const float radiusPx = ((cfg_.sizingMax ? std::max(canvasSize.x, canvasSize.y) : std::min(canvasSize.x, canvasSize.y)) / 2.0f) - (2 * GUI_VAR.charSizeX);

    // Draw grid
    {
        const int step = 30;
        const float f = 0.707106781f;  // cos(pi/4)
        for (int elev = 0; elev < 90; elev += step) {
            const float r1 = radiusPx * (1.0f - (float)elev / 90.f);
            draw->AddCircle(canvasCent, r1, C_PLOT_GRID_MAJOR(), 0);
            const float r2 = radiusPx * (1.0f - (float)(elev + (step / 2)) / 90.f);
            draw->AddCircle(canvasCent, r2, C_PLOT_GRID_MINOR(), 0);
        }
        draw->AddLine(canvasCent - Vec2f(radiusPx, 0), canvasCent + Vec2f(radiusPx, 0), C_PLOT_GRID_MAJOR());
        draw->AddLine(canvasCent - Vec2f(0, radiusPx), canvasCent + Vec2f(0, radiusPx), C_PLOT_GRID_MAJOR());
        draw->AddLine(canvasCent - Vec2f(f * radiusPx, f * radiusPx), canvasCent + Vec2f(f * radiusPx, f * radiusPx),
            C_PLOT_GRID_MINOR());
        draw->AddLine(canvasCent - Vec2f(f * radiusPx, -f * radiusPx), canvasCent + Vec2f(f * radiusPx, f * -radiusPx),
            C_PLOT_GRID_MINOR());
    }

    // Draw satellites
    {
        const Vec2f textOffs(-1.5f * GUI_VAR.charSizeX, -0.5f * GUI_VAR.charSizeY);
        const float svR = 2.0f * GUI_VAR.charSizeX;
        const Vec2f barOffs(-svR, (0.5f * GUI_VAR.charSizeY));
        const Vec2f barSep(0.0f, 5.0f);
        const Vec2f buttonSize(2 * svR, 2 * svR);
        const Vec2f buttonOffs(-svR, -svR);
        const float barScale = svR / 25.0f;
        for (const auto& sat : sats_) {
            // Circle
            const Vec2f svPos = canvasCent + (sat.xy_ * radiusPx);
            draw->AddCircleFilled(svPos, svR, C_SKY_VIEW_SAT());

            // Tooltip
            ImGui::SetCursorScreenPos(svPos + buttonOffs);
            ImGui::InvisibleButton(sat.satInfo_->satStr, buttonSize);
            bool isHovered = false;
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone)) {
                isHovered = true;
                ImGui::BeginTooltip();
                ImGui::Text("%s %s\naz %d el %d\nL1 %2.0f %s\nE6 %2.0f %s\nL2 %2.0f %s\nL5 %2.0f %s",
                    sat.satInfo_->satStr, sat.satInfo_->orbUsedStr, sat.satInfo_->azim, sat.satInfo_->elev,
                    sat.sigL1_ ? sat.sigL1_->cno : 0.0f, sat.sigL1_ ? sat.sigL1_->signalStr : "no signal",
                    sat.sigE6_ ? sat.sigE6_->cno : 0.0f, sat.sigE6_ ? sat.sigE6_->signalStr : "no signal",
                    sat.sigL2_ ? sat.sigL2_->cno : 0.0f, sat.sigL2_ ? sat.sigL2_->signalStr : "no signal",
                    sat.sigL5_ ? sat.sigL5_->cno : 0.0f, sat.sigL5_ ? sat.sigL5_->signalStr : "no signal");
                ImGui::EndTooltip();
            }

            Vec2f barPos = svPos + barOffs;
            if (sat.sigL1_) {
                draw->AddRectFilled(barPos, barPos + Vec2f(sat.sigL1_->cno * barScale, 3.0f),
                    sat.sigL1_->anyUsed ? CNO_COLOUR(sat.sigL1_->cno) : C_SIGNAL_UNUSED());
            }
            barPos += barSep;
            if (sat.sigE6_) {
                draw->AddRectFilled(barPos, barPos + Vec2f(sat.sigE6_->cno * barScale, 3.0f),
                    sat.sigE6_->anyUsed ? CNO_COLOUR(sat.sigE6_->cno) : C_SIGNAL_UNUSED());
            }
            barPos += barSep;
            if (sat.sigL2_) {
                draw->AddRectFilled(barPos, barPos + Vec2f(sat.sigL2_->cno * barScale, 3.0f),
                    sat.sigL2_->anyUsed ? CNO_COLOUR(sat.sigL2_->cno) : C_SIGNAL_UNUSED());
            }
            barPos += barSep;
            if (sat.sigL5_) {
                draw->AddRectFilled(barPos, barPos + Vec2f(sat.sigL5_->cno * barScale, 3.0f),
                    sat.sigL5_->anyUsed ? CNO_COLOUR(sat.sigL5_->cno) : C_SIGNAL_UNUSED());
            }

            // Label
            ImGui::SetCursorScreenPos(svPos + textOffs);
            if (isHovered) {
                ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_HIGHLIGHT_FG());
            }
            ImGui::TextUnformatted(sat.satInfo_->satStr);
            if (isHovered) {
                ImGui::PopStyleColor();
            }
        }
    }

    draw->PopClipRect();

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSatellites::_UpdateSatellites()
{
    counts_.fill({});
    sats_.clear();

    if (epoch_) {
        table_.ClearRows();

        // Update counts, populate list of satellites considering filters
        for (auto& sat : epoch_->epoch_.sats) {
            // Signals from this satellite
            const SigInfo* sigL1 = nullptr;
            const SigInfo* sigE6 = nullptr;
            const SigInfo* sigL2 = nullptr;
            const SigInfo* sigL5 = nullptr;
            bool satUsed = false;
            for (auto ix : sat.sigIxs) {
                if (ix < 0) {
                    continue;
                }
                const SigInfo* sig = &epoch_->epoch_.sigs[ix];
                switch (sig->band) {  // clang-format off
                    case Band::L1: sigL1 = sig; break;
                    case Band::E6: sigE6 = sig; break;
                    case Band::L2: sigL2 = sig; break;
                    case Band::L5: sigL5 = sig; break;
                    case Band::UNKNOWN: break;
                }  // clang-format on
                if (sig->anyUsed) {
                    satUsed = true;
                }
            }

            // Update counts
            for (auto& f : FILTERS) {
                if ((f.ix == 0) || (sat.gnss == f.gnss)) {
                    counts_[f.ix].num_++;
                    if (satUsed) {
                        counts_[f.ix].used_++;
                    }
                    if (sat.elev > 0) {
                        counts_[f.ix].visible_++;
                    }
                }
            }

            // Apply filter for table and sky
            const auto filterGnss = FILTERS[filterIx_].gnss;
            if ((filterGnss != Gnss::UNKNOWN) && (sat.gnss != filterGnss)) {
                continue;
            }

            // Update sky
            if (sat.elev > 0) {
                SkySat ss;
                ss.satInfo_ = &sat;
                ss.sigL1_ = sigL1;
                ss.sigE6_ = sigE6;
                ss.sigL2_ = sigL2;
                ss.sigL5_ = sigL5;
                const float r = 1.0f - ((float)sat.elev * (1.0f / 90.f));
                const float a = (270.0f + (float)sat.azim) * (float)(M_PI / 180.0);
                ss.xy_ = Vec2f(std::cos(a), std::sin(a)) * r;
                sats_.push_back(ss);
            }

            table_.AddCellText(sat.satStr, sat.sat.id_);

            if (sat.orbUsed > SatOrb::NONE) {
                table_.AddCellTextF("%d", sat.azim);
                table_.SetCellSort(sat.azim);
                table_.AddCellTextF("%d", sat.elev);
                table_.SetCellSort(sat.elev + 100);
            } else {
                table_.AddCellText("-", 1);
                table_.AddCellText("-", 1);
            }

            table_.AddCellTextF("%-5s %c%c%c%c", sat.orbUsedStr,
                CheckBitsAll(sat.orbAvail, Bit<int>(EnumToVal(SatOrb::ALM))) ? 'A' : '-',
                CheckBitsAll(sat.orbAvail, Bit<int>(EnumToVal(SatOrb::EPH))) ? 'E' : '-',
                CheckBitsAll(sat.orbAvail, Bit<int>(EnumToVal(SatOrb::PRED))) ? 'P' : '-',
                CheckBitsAll(sat.orbAvail, Bit<int>(EnumToVal(SatOrb::OTHER))) ? 'O' : '-');
            table_.SetCellSort(EnumToVal(sat.orbUsed));

            if (sigL1 && (sigL1->use > SigUse::SEARCH)) {
                table_.AddCellCb(&GuiWinDataSatellites::_DrawSignalLevelCb, sigL1);
                table_.SetCellSort(sigL1->cno * 100);
            } else {
                table_.AddCellText("-", 1);
            }
            if (sigE6 && (sigE6->use > SigUse::SEARCH)) {
                table_.AddCellCb(&GuiWinDataSatellites::_DrawSignalLevelCb, sigE6);
                table_.SetCellSort(sigE6->cno * 100);
            } else {
                table_.AddCellText("-", 1);
            }
            if (sigL2 && (sigL2->use > SigUse::SEARCH)) {
                table_.AddCellCb(&GuiWinDataSatellites::_DrawSignalLevelCb, sigL2);
                table_.SetCellSort(sigL2->cno * 100);
            } else {
                table_.AddCellText("-", 1);
            }
            if (sigL5 && (sigL5->use > SigUse::SEARCH)) {
                table_.AddCellCb(&GuiWinDataSatellites::_DrawSignalLevelCb, sigL5);
                table_.SetCellSort(sigL5->cno * 100);
            } else {
                table_.AddCellText("-", 1);
            }

            if (satUsed) {
                table_.SetRowColour(C_SIGNAL_USED_TEXT());
            } else {
                table_.SetRowColour(C_SIGNAL_UNUSED_TEXT());
            }
            table_.SetRowUid(sat.sat.id_);
        }
    }

    // Update tab labels
    for (auto& f : FILTERS) {  // clang-format off
        counts_[f.ix].labelList_ = (counts_[f.ix].num_ > 0 ?
            Sprintf("%s (%d/%d)###%s", f.label, counts_[f.ix].used_, counts_[f.ix].num_, f.label) : Sprintf("%s###%s", f.label, f.label));
        counts_[f.ix].labelSky_ = (counts_[f.ix].visible_ > 0 ?
            Sprintf("%s (%d/%d)###%s", f.label, counts_[f.ix].used_, counts_[f.ix].visible_, f.label) : Sprintf("%s###%s", f.label, f.label));
    }  // clang-format on
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
