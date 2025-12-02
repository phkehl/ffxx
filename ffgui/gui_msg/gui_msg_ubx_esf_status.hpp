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

#ifndef __GUI_MSG_USB_ESF_STATUS_HPP__
#define __GUI_MSG_USB_ESF_STATUS_HPP__

#include <memory>
#include "ffgui_inc.hpp"
#include "gui_msg.hpp"
#include "gui_widget_table.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsgUbxEsfStatus : public GuiMsg
{
   public:
    GuiMsgUbxEsfStatus(const std::string& viewName, const InputPtr& input);

    void Update(const DataMsgPtr& msg) final;
    void Update(const Time& now) final;
    void Clear() final;

    void DrawToolbar() final;
    void DrawMsg() final;

   private:
    GuiWidgetTable table_;
    // static const std::vector<StatusFlag> fusionModeFlags_;
    // static const std::vector<StatusFlag> statusFlags012_;
    // static const std::vector<StatusFlag> statusFlags0123_;

    static constexpr std::array<StatusFlag, 4> FUSION_MODE_FLAGS = // clang-format off
    {{
        { UBX_ESF_STATUS_V2_FUSIONMODE_INIT,      "Init",         CIX_TEXT_WARNING },
        { UBX_ESF_STATUS_V2_FUSIONMODE_FUSION,    "Fusion",       CIX_TEXT_OK      },
        { UBX_ESF_STATUS_V2_FUSIONMODE_SUSPENDED, "Suspended",    CIX_TEXT_ERROR   },
        { UBX_ESF_STATUS_V2_FUSIONMODE_DISABLED,  "Disabled",     CIX_TEXT_WARNING },
    }}; // clang-format on

    static constexpr std::array<StatusFlag, 3> STATUS_FLAGS_012 = // clang-format off
    {{
        { UBX_ESF_STATUS_V2_INITSTATUS1_WTINITSTATUS_OFF,          "Off",          CIX_AUTO },
        { UBX_ESF_STATUS_V2_INITSTATUS1_WTINITSTATUS_INITALIZING,  "Initialising", CIX_TEXT_WARNING },
        { UBX_ESF_STATUS_V2_INITSTATUS1_WTINITSTATUS_INITIALIZED,  "Initialised",  CIX_TEXT_OK },
    }}; // clang-format on

    static constexpr std::array<StatusFlag, 4> STATUS_FLAGS_0123 = // clang-format off
    {{
        { UBX_ESF_STATUS_V2_INITSTATUS1_MNTALGSTATUS_OFF,          "Off",           CIX_AUTO },
        { UBX_ESF_STATUS_V2_INITSTATUS1_MNTALGSTATUS_INITALIZING,  "Initialising",  CIX_TEXT_WARNING },
        { UBX_ESF_STATUS_V2_INITSTATUS1_MNTALGSTATUS_INITIALIZED1, "Initialised",   CIX_TEXT_OK },
        { UBX_ESF_STATUS_V2_INITSTATUS1_MNTALGSTATUS_INITIALIZED2, "Initialised!",  CIX_TEXT_OK },
    }}; // clang-format on

    struct Sensor
    {
        Sensor(const uint8_t* groupData);
        std::string type_;
        bool used_ = false;
        bool ready_ = false;
        std::string calibStatus_;
        bool calibrated_ = false;
        std::string timeStatus_;
        std::string freq_;
        std::string faults_;
        inline bool operator< (const Sensor& rhs) const { return type_ < rhs.type_; }
    };
    bool valid_ = false;
    std::string iTow_;
    int wtInitStatus_ = 0;
    int mntAlgStatus_ = 0;
    int insInitStatus_ = 0;
    int imuInitStatus_ = 0;
    int fusionMode_ = 0;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_USB_ESF_STATUS_HPP__
