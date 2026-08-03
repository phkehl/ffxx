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

#ifndef __GUI_WIN_DATA_EPOCH_HPP__
#define __GUI_WIN_DATA_EPOCH_HPP__

//
#include "ffgui_inc.hpp"
//
#include "gui_widget_table.hpp"
#include "gui_widget_tabbar.hpp"
#include "gui_win_data.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinDataEpoch : public GuiWinData
{
   public:
    GuiWinDataEpoch(const std::string& name, const InputPtr& input);
    ~GuiWinDataEpoch();

   private:
    void _ProcessData(const InputDataPtr& data) final;
    void _Loop(const Time& now) final;
    void _ClearData() final;

    void _DrawToolbar() final;
    void _DrawContent() final;

    GuiWidgetTable table_;
    GuiWidgetTabbar tabbar_;

    void _DrawSiglevelPlot();
    std::array<float, NumOf<SigCnoHist>()> cno_trk_;
    std::array<float, NumOf<SigCnoHist>()> cno_nav_;
    float cno_trk_max_ = 0.0f;
    std::array<float, 4> cno_top5_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_DATA_EPOCH_HPP__
