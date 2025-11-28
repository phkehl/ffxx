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

#ifndef __GUI_MSG_UBX_RXM_SFRBX_HPP__
#define __GUI_MSG_UBX_RXM_SFRBX_HPP__

//
#include "ffgui_inc.hpp"
//
#include <fpsdk_common/gnss.hpp>
using namespace fpsdk::common::gnss;
//
#include "gui_msg.hpp"
#include "gui_widget_table.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsgUbxRxmSfrbx : public GuiMsg
{
   public:
    GuiMsgUbxRxmSfrbx(const std::string& viewName, const InputPtr& input);

    void Update(const DataMsgPtr& msg) final;
    void Update(const Time& now) final;
    void Clear() final;

    void DrawToolbar() final;
    void DrawMsg() final;

   private:
    GuiWidgetTable table_;

    struct SfrbxInfo
    {
        SatSig satSig_;
        int gloSlot_ = 0;
        Time lastTime_;
        std::vector<uint32_t> dwrds_;
        std::string dwrdsHex_;
        std::string navMsg_;
    };

    std::map<SatSig, SfrbxInfo> sfrbxInfos_;
    Time now_;
    bool dirty_ = false;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_UBX_RXM_SFRBX_HPP__
