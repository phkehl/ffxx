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

#ifndef __GUI_MSG_UBX_MON_RF_HPP__
#define __GUI_MSG_UBX_MON_RF_HPP__

//
#include "ffgui_inc.hpp"
//
#include <fpsdk_common/parser/ubx.hpp>
using namespace fpsdk::common::parser::ubx;
//
#include "gui_msg.hpp"
#include "gui_msg_ubx_mon_hw2.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsgUbxMonRf : public GuiMsg
{
   public:
    GuiMsgUbxMonRf(const std::string& viewName, const InputPtr& input);

    void Update(const DataMsgPtr& msg) final;
    void Update(const Time& now) final;
    void Clear() final;

    void DrawToolbar() final;
    void DrawMsg() final;

   private:
    std::vector<UBX_MON_RF_V0_GROUP1> rfBlocks_;
    std::vector< std::list<GuiMsgUbxMonHw2::IQ> > blockIqs_;
    static constexpr std::size_t MAX_BLOCKS = 4;
    static constexpr std::size_t MAX_IQS = 50;

    static constexpr std::array<StatusFlag, 5> ASTATUS_FLAGS =  // clang-format off
    {{
        { UBX_MON_RF_V0_ANTSTATUS_INIT,     "init",           CIX_AUTO },
        { UBX_MON_RF_V0_ANTSTATUS_DONTKNOW, "unknown",        CIX_AUTO },
        { UBX_MON_RF_V0_ANTSTATUS_OK,       "OK",             CIX_TEXT_OK },
        { UBX_MON_RF_V0_ANTSTATUS_SHORT,    "short",          CIX_TEXT_WARNING },
        { UBX_MON_RF_V0_ANTSTATUS_OPEN,     "open",           CIX_TEXT_WARNING  },
    }}; // clang-format off

    static constexpr std::array<StatusFlag, 3> APOWER_FLAGS =  // clang-format off
    {{
        { UBX_MON_HW_V0_APOWER_OFF,       "off",              CIX_AUTO },
        { UBX_MON_HW_V0_APOWER_ON,        "on",               CIX_TEXT_OK },
        { UBX_MON_HW_V0_APOWER_UNKNOWN,   "unknown",          CIX_AUTO },
    }}; // clang-format off

    static constexpr std::array<StatusFlag, 4> JAMMING_FLAGS =  // clang-format off
    {{
        { UBX_MON_RF_V0_FLAGS_JAMMINGSTATE_UNKN,   "unknown",   CIX_AUTO },
        { UBX_MON_RF_V0_FLAGS_JAMMINGSTATE_OK,     "ok",        CIX_TEXT_OK },
        { UBX_MON_RF_V0_FLAGS_JAMMINGSTATE_WARN,   "warning",   CIX_TEXT_WARNING },
        { UBX_MON_RF_V0_FLAGS_JAMMINGSTATE_CRIT,   "critical",  CIX_TEXT_ERROR },
    }}; // clang-format off

};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_UBX_MON_RF_HPP__
