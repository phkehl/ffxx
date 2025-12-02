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
#include "gui_win_data.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinData::GuiWinData(const std::string& name, const ImVec4& size, const ImGuiWindowFlags flags,
    const InputPtr& input) /* clang-format off */ :
    GuiWin(name, size, flags),
    input_   { input }  // clang-format on
{
    DEBUG("GuiWinData(%s)", WinName().c_str());

    latestEpochMaxAge_.SetSec(5.5);

    AddDataObserver(input_->srcId_, this, std::bind(&GuiWinData::_ProcessInputDataMain, this, std::placeholders::_1));
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWinData::~GuiWinData()
{
    DEBUG("~GuiWinData(%s)", WinName().c_str());
    RemoveDataObserver(input_->srcId_, this);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinData::_ProcessInputDataMain(const DataPtr& data)
{
    switch (data->type_) {
        case DataType::EPOCH:
            if (!latestEpochMaxAge_.IsZero()) {
                latestEpoch_ = DataPtrToDataEpochPtr(data);
            }
            break;
        case DataType::MSG:
        case DataType::DEBUG:
        case DataType::EVENT:
            break;
    }

    _ProcessData(data);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinData::Loop(const Time& now)
{
    if (latestEpoch_) {
        if (!latestEpochMaxAge_.IsZero() && ((now - latestEpoch_->time_) > latestEpochMaxAge_)) {
            latestEpoch_.reset();
            latestEpochAge_ = NAN;
        } else {
            latestEpochAge_ = (now - latestEpoch_->time_).GetSec();
        }
    }

    _Loop(now);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinData::ClearData()
{
    latestEpoch_.reset();
    _ClearData();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinData::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }

    // Common buttons toolbar
    if (toolbarEna_) {
        // Clear
        if (toolbarClearEna_) {
            if (ImGui::Button(ICON_FK_ERASER "##Clear", GUI_VAR.iconSize)) {
                ClearData();
            }
            Gui::ItemTooltip("Clear all data");
        }

        _DrawToolbar();

        // Age of latest epoch
        if (latestEpoch_ && (input_->receiver_ || input_->corr_)) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - (4 * GUI_VAR.charSizeX));
            ImGui::AlignTextToFramePadding();
            if (latestEpochAge_ < 0.95) {
                ImGui::Text("  .%.0f", latestEpochAge_ * 10.0);
            } else {
                ImGui::Text("%4.1f", latestEpochAge_);
            }
        }

        ImGui::Separator();
    }

    _DrawContent();

    _DrawWindowEnd();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
