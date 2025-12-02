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

#ifndef __GUI_WIN_INPUT_LOGFILE_HPP__
#define __GUI_WIN_INPUT_LOGFILE_HPP__

//
#include "ffgui_inc.hpp"
//
#include "gui_win_filedialog.hpp"
#include "gui_widget_history.hpp"
#include "logfile.hpp"
//
#include "gui_win_input.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinInputLogfile : public GuiWinInput
{
   public:
    GuiWinInputLogfile(const std::string& name);
    ~GuiWinInputLogfile();

   private:
    struct Config
    {
        float speed = 10.0f;
        bool limitSpeed = true;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, speed, limitSpeed)
    Config cfg_;

    GuiWinFileDialog fileDialog_;
    GuiWidgetHistory history_;

    void _ProcessData(const DataPtr& data) final;
    void _Loop(const Time& now) final;
    void _ClearData() final;

    void _DrawButtons() final;
    void _DrawStatus() final;
    void _DrawControls() final;

    std::string progressBarFmt_;
    float seekProgress_ = -1.0f;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_INPUT_LOGFILE_HPP__
