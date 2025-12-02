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
#include "gui_win_data_fwupdate.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinDataFwupdate::GuiWinDataFwupdate(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 80, 25, 0, 0 }, ImGuiWindowFlags_None, input)  // clang-format on
{
    DEBUG("GuiWinDataFwupdate(%s)", WinName().c_str());

    toolbarClearEna_ = false;
    latestEpochMaxAge_ = {};
}

GuiWinDataFwupdate::~GuiWinDataFwupdate()
{
    DEBUG("GuiWinDataFwupdate(%s)", WinName().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataFwupdate::_ProcessData(const DataPtr& /*data*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataFwupdate::_Loop(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataFwupdate::_ClearData()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataFwupdate::_DrawToolbar()
{
    ImGui::TextUnformatted("...");
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataFwupdate::_DrawContent()
{
    ImGui::TextUnformatted("Yeah, you wished! :-)");
}

/* ****************************************************************************************************************** */
} // ffgui
