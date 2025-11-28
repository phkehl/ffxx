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

#ifndef __GUI_WIN_DATA_HPP__
#define __GUI_WIN_DATA_HPP__

//
#include "ffgui_inc.hpp"
//
#include "input.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinData : public GuiWin
{
   public:
    GuiWinData(const std::string& name, const ImVec4& size, const ImGuiWindowFlags flags, const InputPtr& input);
    virtual ~GuiWinData();

    void Loop(const Time& now) override final;
    void DrawWindow() override final;
    void ClearData();

   protected:
    InputPtr input_;

    // Common functionality
    bool toolbarEna_ = true;      // Use toolbar?
    bool toolbarClearEna_ = true;  // Clear button?
    Duration latestEpochMaxAge_;  // Set to zero to disable latestEpoch
    DataEpochPtr latestEpoch_;
    double latestEpochAge_ = 0.0;

    virtual void _ProcessData(const DataPtr& data) = 0;
    virtual void _Loop(const Time& now) = 0;
    virtual void _ClearData() = 0;
    virtual void _DrawToolbar() = 0;
    virtual void _DrawContent() = 0;

   private:

    void _ProcessInputDataMain(const DataPtr& data);

    std::vector< std::unique_ptr<GuiWinData> > dataWins_;
    void _CreateDataWin(const std::string& winName = "");
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_DATA_HPP__
