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

#ifndef __GUI_MSG_UBX_RXM_RTCM_HPP__
#define __GUI_MSG_UBX_RXM_RTCM_HPP__

//
#include "ffgui_inc.hpp"
//
#include "gui_msg.hpp"
#include "gui_widget_table.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsgUbxRxmRtcm : public GuiMsg
{
   public:
    GuiMsgUbxRxmRtcm(const std::string& viewName, const InputPtr& input);

    void Update(const DataMsgPtr& msg) final;
    void Update(const Time& now) final;
    void Clear() final;

    void DrawToolbar() final;
    void DrawMsg() final;

   private:
    GuiWidgetTable table_;

    struct RtcmInfo
    {
        std::string name_;
        uint16_t msgType_ = 0;
        uint16_t subType_ = 0;
        int refSta_ = 0;
        uint64_t uid_ = 0;
        uint32_t nUsed_ = 0;
        uint32_t nUnused_ = 0;
        uint32_t nUnknown_ = 0;
        uint32_t nCrcFailed_ = 0;
        Time lastTime_;
        const char* desc_ = nullptr;
    };

    std::map<uint64_t, RtcmInfo> rtcmInfos_;
    bool dirty_ = false;
    Time now_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_UBX_RXM_RTCM_HPP__
