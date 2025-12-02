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

#ifndef __GUI_MSG_UBX_RXM_RAWX_HPP__
#define __GUI_MSG_UBX_RXM_RAWX_HPP__

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

class GuiMsgUbxRxmRawx : public GuiMsg
{
   public:
    GuiMsgUbxRxmRawx(const std::string& viewName, const InputPtr& input);

    void Update(const DataMsgPtr& msg) final;
    void Update(const Time& now) final;
    void Clear() final;

    void DrawToolbar() final;
    void DrawMsg() final;

   private:
    GuiWidgetTable table_;

    struct Raw
    {  // clang-format off
        SatSig satSig_;
        int    gloSlot_  = 0;
        float  cnoValid_ = false;
        float  cnoMeas_  = 0.0f;
        bool   prValid_  = false;
        double prMeas_   = 0.0f;
        double prStd_    = 0.0;
        bool   cpValid_  = false;
        double cpMeas_   = 0.0f;
        double cpStd_    = 0.0;
        bool   doValid_  = false;
        double doMeas_   = 0.0f;
        double doStd_    = 0.0;
        double lockTime_ = 0.0;
        bool   halfCyc_  = false;
        bool   subHalfCyc_ = false;
        inline bool operator< (const Raw& rhs) const { return satSig_.id_ < rhs.satSig_.id_; }
    };
    bool   valid_ = false;
    int    week_ = 0;
    double rcvTow_ = 0.0;
    int    leapSec_ = 0;
    bool   leapSecValid_ = false;;
    bool   clkReset_ = false;
    std::vector<Raw> raws_;

    Time lastClkReset_;
    Duration clkResetAge_;

    // clang-format on
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_UBX_RXM_RAWX_HPP__
