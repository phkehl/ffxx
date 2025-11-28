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

#ifndef __GUI_WIN_INPUT_HPP__
#define __GUI_WIN_INPUT_HPP__

//
#include "ffgui_inc.hpp"
//
#include "gui_widget_log.hpp"
#include "gui_win_data.hpp"
#include "input.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinInput : public GuiWin
{
   public:
    GuiWinInput(
        const std::string& name, const ImVec4& size, const ImGuiWindowFlags flags, const InputPtr& input);
    ~GuiWinInput();

    void Loop(const Time& now) override final;
    void DrawWindow() override final;
    void DrawDataWins(const bool closeFocusedWin);
    void DataWinMenu();
    void DataWinMenuSwitcher();

   protected:
    InputPtr input_;
    GuiWidgetLog log_;
    DataEpochPtr latestEpoch_;
    Duration latestEpochMaxAge_; // set to zero to disable latestEpoch
    DataEpochPtr latestEpochWithSigCnoHist_;
    double latestEpochAge_ = 0.0;
    std::string versionStr_;

    void _Init();

    void _DrawLatestEpochStatus();
    void _UpdateWinTitles();

   private:
    struct Config
    {
        std::set<std::string> dataWinNames;
        bool autoHideDataWins = true;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, dataWinNames)
    Config cfg_;

    DataMsgPtr versionMsg_;

    // +-----+-----------------------+---------+---------------------+
    // | [O] | [A] [B] [C] ...       | [C] [D] | _DrawButtons()  age |
    // +-----+-----------------------+---------+---------------------+
    // | _DrawStatus()                                               |
    // |                                                             |
    // |                                                             |
    // +-------------------------------------------------------------+
    // | _DrawControls()                                             |
    // +-------------------------------------------------------------+
    // | Log                                                         |
    // |                                                             |
    // +-------------------------------------------------------------+
    //

    virtual void _ProcessData(const DataPtr& data) = 0;
    virtual void _Loop(const Time& now) = 0;
    virtual void _ClearData() = 0;

    virtual void _DrawButtons() = 0;
    virtual void _DrawStatus() = 0;
    virtual void _DrawControls() = 0;

    std::list<std::unique_ptr<GuiWinData>> dataWins_;
    template <typename WinT>
    void _AddDataWin(const std::string& baseName, const std::string& prevWinName);
    void _RestoreDataWins();
    void _UpdateDataWins();

    void _ClearLatestEpoch();
    void _ClearDataCommon();
    void _ProcessInputDataMain(const DataPtr& data);
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_INPUT_HPP__
