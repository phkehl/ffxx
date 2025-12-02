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

#ifndef __GUI_MSG_UBX_MON_VER_HPP__
#define __GUI_MSG_UBX_MON_VER_HPP__

//
#include "ffgui_inc.hpp"
//
#include "gui_msg.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsgUbxMonVer : public GuiMsg
{
    public:
        GuiMsgUbxMonVer(const std::string& viewName, const InputPtr& input);

        void Update(const DataMsgPtr& msg) final;
        void Update(const Time& now) final;
        void Clear() final;

        void DrawToolbar() final;
        void DrawMsg() final;
    private:
        std::string swVersion_;
        std::string hwVersion_;
        std::vector<std::string> extensions_;
};

/* ****************************************************************************************************************** */
} // ffgui
#endif // __GUI_MSG_UBX_MON_VER_HPP__
