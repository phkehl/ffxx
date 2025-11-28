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

#ifndef __GUI_MSG_UBX_MON_HW3_HPP__
#define __GUI_MSG_UBX_MON_HW3_HPP__

//
#include "ffgui_inc.hpp"
//
#include <fpsdk_common/parser/ubx.hpp>
using namespace fpsdk::common::parser::ubx;
//
#include "gui_msg.hpp"
#include "gui_widget_table.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsgUbxMonHw3 : public GuiMsg
{
   public:
    GuiMsgUbxMonHw3(const std::string& viewName, const InputPtr& input);

    void Update(const DataMsgPtr& msg) final;
    void Update(const Time& now) final;
    void Clear() final;

    void DrawToolbar() final;
    void DrawMsg() final;

   private:
    GuiWidgetTable table_;
    bool valid_ = false;
    UBX_MON_HW3_V0_GROUP0 hw3_;

    static constexpr std::array<StatusFlag, 2> RTC_FLAGS =  // clang-format off
    {{
        { true,      "calibrated",         CIX_TEXT_OK      },
        { false,     "uncalibrated",       CIX_TEXT_WARNING },
    }}; // clang-format off

    static constexpr std::array<StatusFlag, 2> XTAL_FLAGS =  // clang-format off
    {{
        { true,      "absent",             CIX_TEXT_OK },
        { false,     "ok",                 CIX_AUTO  },
    }}; // clang-format off

    static constexpr std::array<StatusFlag, 2> SAFEBOOT_FLAGS =  // clang-format off
    {{
        { true,      "active",             CIX_TEXT_WARNING },
        { false,     "inactive",           CIX_AUTO },
    }}; // clang-format off
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_UBX_MON_HW3_HPP__
