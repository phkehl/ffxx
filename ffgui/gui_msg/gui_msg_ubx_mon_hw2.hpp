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

#ifndef __GUI_MSG_UBX_MON_HW2_HPP__
#define __GUI_MSG_UBX_MON_HW2_HPP__

//
#include "ffgui_inc.hpp"
//
#include <list>
//
#include "gui_msg.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsgUbxMonHw2 : public GuiMsg
{
   public:
    GuiMsgUbxMonHw2(const std::string& viewName, const InputPtr& input);

    void Update(const DataMsgPtr& msg) final;
    void Update(const Time& now) final;
    void Clear() final;

    void DrawToolbar() final;
    void DrawMsg() final;

    struct IQ
    {
        float magI_ = 0.0f;
        float magQ_ = 0.0f;
        float offsI_ = 0.0f;
        float offsQ_ = 0.0f;
    };

    static void DrawIQ(const std::list<IQ>& iqs, const ImVec2& size = ImGui::GetContentRegionAvail());

   private:
    static constexpr std::size_t MAX_IQS = 50;
    std::list<IQ> iqs_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_UBX_MON_HW2_HPP__
