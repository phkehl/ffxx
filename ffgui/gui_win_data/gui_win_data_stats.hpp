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

#ifndef __GUI_WIN_DATA_STATS_HPP__
#define __GUI_WIN_DATA_STATS_HPP__

#include "gui_widget_tabbar.hpp"
#include "gui_widget_table.hpp"
#include "gui_win_data.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinDataStats : public GuiWinData
{
   public:
    GuiWinDataStats(const std::string& name, const InputPtr& input);
    ~GuiWinDataStats();

   private:
    void _ProcessData(const DataPtr& data) final;
    void _Loop(const Time& now) final;
    void _ClearData() final;

    void _DrawToolbar() final;
    void _DrawContent() final;

    GuiWidgetTabbar tabbar_;
    GuiWidgetTable table_;
    Database::Info dbInfo_;

    void _DrawSiglevelPlot();
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_DATA_STATS_HPP__
