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
#include "gui_msg_ubx_mon_hw.hpp"
#include "gui_msg_ubx_mon_hw3.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxMonHw3::GuiMsgUbxMonHw3(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input),
    table_   { viewName_ }  // clang-format on
{
    table_.AddColumn("Pin");
    table_.AddColumn("Direction");
    table_.AddColumn("State");
    table_.AddColumn("Peripheral");
    table_.AddColumn("IRQ");
    table_.AddColumn("Pull");
    table_.AddColumn("Virtual");
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw3::Update(const DataMsgPtr& msg)
{
    Clear();

    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if ((UBX_MON_HW3_VERSION(data) != UBX_MON_HW3_V0_VERSION) || (UBX_MON_HW3_V0_SIZE(data) != size)) {
        return;
    }
    std::memcpy(&hw3_, &data[UBX_HEAD_SIZE], sizeof(hw3_));
    valid_ = true;

    for (int pinIx = 0, offs = UBX_HEAD_SIZE + (int)sizeof(UBX_MON_HW3_V0_GROUP0); pinIx < (int)hw3_.nPins;
        pinIx++, offs += (int)sizeof(UBX_MON_HW3_V0_GROUP1)) {
        UBX_MON_HW3_V0_GROUP1 pin;
        std::memcpy(&pin, &data[offs], sizeof(pin));

        const uint8_t pinBank = pin.pinId & 0xff;
        const uint8_t pinNo = (pin.pinId >> 8) & 0xff;
        table_.AddCellTextF("%d %2d##%d", pinBank, pinNo, pinIx);
        table_.AddCellText(
            UBX_MON_HW3_V0_PINMASK_DIRECTION(pin.pinMask) == UBX_MON_HW3_V0_PINMASK_DIRECTION_OUT ? "output" : "input");
        table_.AddCellText(UBX_MON_HW3_V0_PINMASK_VALUE(pin.pinMask) ? "high" : "low");
        if (UBX_MON_HW3_V0_PINMASK_PERIPHPIO(pin.pinMask)) {
            table_.AddCellText("PIO");
        } else {
            const uint8_t periph = UBX_MON_HW3_V0_PINMASK_PINBANK(pin.pinMask);
            const char periphChars[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H' };
            table_.AddCellTextF("%c", periph < NumOf(periphChars) ? periphChars[periph] : '?');
        }
        table_.AddCellText(UBX_MON_HW3_V0_PINMASK_PIOIRQ(pin.pinMask) ? "yes" : "no");
        const bool pullLo = UBX_MON_HW3_V0_PINMASK_PIOPULLLOW(pin.pinMask);
        const bool pullHi = UBX_MON_HW3_V0_PINMASK_PIOPULLHIGH(pin.pinMask);
        table_.AddCellText(pullLo && pullHi ? "low high" : (pullLo ? "low" : (pullHi ? "high" : "")));
        if (UBX_MON_HW3_V0_PINMASK_VPMANAGER(pin.pinMask)) {
            table_.AddCellTextF("%u - %s", pin.VP,
                (pin.VP < NumOf(GuiMsgUbxMonHw::VIRT_FUNCS)) && GuiMsgUbxMonHw::VIRT_FUNCS[pin.VP]
                    ? GuiMsgUbxMonHw::VIRT_FUNCS[pin.VP]
                    : "?");
        } else {
            table_.AddCellEmpty();
        }
        table_.SetRowUid(pinIx + 1);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw3::Update(const Time& now)
{
    UNUSED(now);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw3::Clear()
{
    table_.ClearRows();
    valid_ = false;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw3::DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw3::DrawMsg()
{
    if (!valid_) {
        return;
    }

    const float dataOffs = 20 * GUI_VAR.charSizeX;
    // clang-format off
    _RenderStatusFlag(RTC_FLAGS,       UBX_MON_HW3_V0_FLAGS_RTCCALIB(hw3_.flags),   "RTC mode",      dataOffs);
    _RenderStatusFlag(XTAL_FLAGS,      UBX_MON_HW3_V0_FLAGS_XTALABSENT(hw3_.flags), "RTC XTAL",      dataOffs);
    _RenderStatusFlag(SAFEBOOT_FLAGS,  UBX_MON_HW3_V0_FLAGS_SAFEBOOT(hw3_.flags),   "Safeboot mode", dataOffs);
    // clang-format on
    char str[sizeof(hw3_.hwVersion) + 1];
    std::memcpy(str, hw3_.hwVersion, sizeof(hw3_.hwVersion));
    str[sizeof(str) - 1] = '\0';
    _RenderStatusText("Hardware version", str, dataOffs);

    table_.DrawTable();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
