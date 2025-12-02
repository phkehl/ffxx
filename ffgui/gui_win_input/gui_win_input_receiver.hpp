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

#ifndef __GUI_WIN_INPUT_RECEIVER_HPP__
#define __GUI_WIN_INPUT_RECEIVER_HPP__

//
#include "ffgui_inc.hpp"
//
#include <fpsdk_common/path.hpp>
using namespace fpsdk::common::path;
//
#include "gui_widget_streamspec.hpp"
#include "gui_win_filedialog.hpp"
#include "gui_win_input.hpp"
#include "receiver.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinInputReceiver : public GuiWinInput
{
   public:
    GuiWinInputReceiver(const std::string& name);
    ~GuiWinInputReceiver();

    // bool WinIsOpen() override final;

   private:
    struct Config
    {
        bool dummy = false;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, dummy)
    Config cfg_;

    void _ProcessData(const DataPtr& data) final;
    void _Loop(const Time& now) final;
    void _ClearData() final;

    void _DrawButtons() final;
    void _DrawStatus() final;
    void _DrawControls() final;

    GuiWidgetStreamSpec streamSpec_;
    bool triggerConnect_ = false;
    void _OnBaudrateChange(const uint32_t baudrate);

    // Recording
    GuiWinFileDialog recordFileDialog_;
    std::unique_ptr<OutputFile> recordLog_;
    ImU32 recordButtonColor_ = C_AUTO();
    std::string recordMessage_;
    std::size_t recordLastSize_ = 0;
    Time recordLastSizeTime_;
    Time recordLastMsgTime_;
    double recordKiBs_ = 0.0;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_INPUT_RECEIVER_HPP__
