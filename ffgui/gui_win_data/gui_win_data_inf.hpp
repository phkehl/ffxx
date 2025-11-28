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

#ifndef __GUI_WIN_DATA_INF_HPP__
#define __GUI_WIN_DATA_INF_HPP__

//
#include "ffgui_inc.hpp"
//
#include "gui_widget_log.hpp"
#include "gui_win_data.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinDataInf : public GuiWinData
{
   public:
    GuiWinDataInf(const std::string& name, const InputPtr& input);
    ~GuiWinDataInf();

   private:
    void _ProcessData(const DataPtr& data) final;
    void _Loop(const Time& now) final;
    void _ClearData() final;

    void _DrawToolbar() final;
    void _DrawContent() final;

    GuiWidgetLog log_;

    std::size_t nDebug_ = 0;
    std::size_t nNotice_ = 0;
    std::size_t nWarning_ = 0;
    std::size_t nError_ = 0;
    std::size_t nOther_ = 0;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_DATA_INF_HPP__
