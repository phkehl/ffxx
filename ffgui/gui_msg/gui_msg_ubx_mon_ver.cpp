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

//
#include "ffgui_inc.hpp"
//
#include <fpsdk_common/parser/ubx.hpp>
using namespace fpsdk::common::parser::ubx;
//
#include "gui_msg_ubx_mon_ver.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::parser::ubx;

GuiMsgUbxMonVer::GuiMsgUbxMonVer(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input)  // clang-format on
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonVer::Update(const DataMsgPtr& msg)
{
    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if (size >= UBX_MON_VER_V0_MIN_SIZE) {
        UBX_MON_VER_V0_GROUP0 gr0;
        std::size_t offs = UBX_HEAD_SIZE;
        std::memcpy(&gr0, &data[offs], sizeof(gr0));
        gr0.swVersion[sizeof(gr0.swVersion) - 1] = '\0';
        gr0.hwVersion[sizeof(gr0.hwVersion) - 1] = '\0';
        swVersion_ = gr0.swVersion;
        hwVersion_ = gr0.hwVersion;

        offs += sizeof(gr0);
        UBX_MON_VER_V0_GROUP1 gr1;
        extensions_.clear();
        while (offs <= (size - 2 - sizeof(gr1))) {
            std::memcpy(&gr1, &data[offs], sizeof(gr1));
            gr1.extension[sizeof(gr1.extension) - 1] = '\0';
            extensions_.push_back(gr1.extension);
            offs += sizeof(gr1);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonVer::Update(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonVer::Clear()
{
    swVersion_.clear();
    hwVersion_.clear();
    extensions_.clear();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonVer::DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonVer::DrawMsg()
{
    ImGui::Text("swVersion = %s", swVersion_.c_str());
    ImGui::Text("hwVersion = %s", hwVersion_.c_str());
    for (std::size_t ix = 0; ix < extensions_.size(); ix++) {
        ImGui::Text("extension[%" PRIuMAX "] = %s", ix, extensions_[ix].c_str());
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
