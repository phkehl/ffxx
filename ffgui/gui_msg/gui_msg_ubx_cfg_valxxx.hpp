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

#ifndef __GUI_MSG_UBX_CFG_VALXXX_HPP__
#define __GUI_MSG_UBX_CFG_VALXXX_HPP__

//
#include "ffgui_inc.hpp"
//
#include "gui_msg.hpp"
#include "gui_widget_log.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsgUbxCfgValxxx : public GuiMsg
{
   public:
    GuiMsgUbxCfgValxxx(const std::string& viewName, const InputPtr& input);

    void Update(const DataMsgPtr& msg) final;
    void Update(const Time& now) final;
    void Clear() final;

    void DrawToolbar() final;
    void DrawMsg() final;

   private:
    GuiWidgetLog log_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_UBX_CFG_VALXXX_HPP__
