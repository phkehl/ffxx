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
#include "gui_msg_ubx_mon_comms.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxMonComms::GuiMsgUbxMonComms(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input),
    table_   { viewName_ }  // clang-format on
{
    table_.AddColumn("Port");
    table_.AddColumn("TX pend", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("TX bytes", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("TX usage", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("RX pend", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("RX bytes", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("RX usage", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Overruns", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Skipped", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("UBX", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("NMEA", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("RTCM3", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("SPARTN", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Other", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonComms::Update(const DataMsgPtr& msg)
{
    Clear();

    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if ((UBX_MON_COMMS_VERSION(data) != UBX_MON_COMMS_V0_VERSION) || (UBX_MON_COMMS_V0_SIZE(data) != size)) {
        return;
    }

    UBX_MON_COMMS_V0_GROUP0 comms;
    std::memcpy(&comms, &data[UBX_HEAD_SIZE], sizeof(comms));

    valid_ = true;
    txErrors_ = comms.txErrors;

    int msgsIxUbx = -1;
    int msgsIxNmea = -1;
    // int msgsIxRtcm2 = -1;
    int msgsIxRtcm3 = -1;
    int msgsIxSpartn = -1;
    int msgsIxOther = -1;
    for (int ix = 0; ix < (int)NumOf(comms.protIds); ix++) {
        switch (comms.protIds[ix]) {  // clang-format off
            case UBX_MON_COMMS_V0_PROTIDS_UBX:    msgsIxUbx    = ix; break;
            case UBX_MON_COMMS_V0_PROTIDS_NMEA:   msgsIxNmea   = ix; break;
            //case UBX_MON_COMMS_V0_PROTIDS_RTCM2: msgsIxRtcm2 = ix; break;
            case UBX_MON_COMMS_V0_PROTIDS_RTCM3:  msgsIxRtcm3  = ix; break;
            case UBX_MON_COMMS_V0_PROTIDS_SPARTN: msgsIxSpartn = ix; break;
            case UBX_MON_COMMS_V0_PROTIDS_OTHER:  msgsIxOther  = ix; break;
        }  // clang-format on
    }

    for (std::size_t portIx = 0, offs = UBX_HEAD_SIZE + sizeof(UBX_MON_COMMS_V0_GROUP0); portIx < comms.nPorts;
        portIx++, offs += sizeof(UBX_MON_COMMS_V0_GROUP1)) {
        UBX_MON_COMMS_V0_GROUP1 port;
        std::memcpy(&port, &data[offs], sizeof(port));

        const uint8_t portBank = port.portId & 0xff;
        const uint8_t portNo = (port.portId >> 8) & 0xff;
        std::array<const char*, 5> portNames = { { "I2C", "UART1", "UART2", "USB", "SPI" } };

        table_.AddCellTextF("%" PRIu8 " %s", portBank, portNo < portNames.size() ? portNames[portNo] : "?");
        table_.AddCellTextF("%" PRIu16, port.txPending);
        table_.AddCellTextF("%" PRIu32, port.txBytes);
        table_.AddCellTextF("%" PRIu8 "%% (%3" PRIu8 "%%)", port.txUsage, port.txPeakUsage);
        table_.AddCellTextF("%" PRIu16, port.rxPending);
        table_.AddCellTextF("%" PRIu32, port.rxBytes);
        table_.AddCellTextF("%" PRIu8 "%% (%3" PRIu8 "%%)", port.rxUsage, port.rxPeakUsage);
        table_.AddCellTextF("%" PRIu16, port.overrunErrors);
        table_.AddCellTextF("%" PRIu32, port.skipped);
        if (msgsIxUbx < 0) {
            table_.AddCellText("-");
        } else {
            table_.AddCellTextF("%u", port.msgs[msgsIxUbx]);
        }
        if (msgsIxNmea < 0) {
            table_.AddCellText("-");
        } else {
            table_.AddCellTextF("%u", port.msgs[msgsIxNmea]);
        }
        if (msgsIxRtcm3 < 0) {
            table_.AddCellText("-");
        } else {
            table_.AddCellTextF("%u", port.msgs[msgsIxRtcm3]);
        }
        if (msgsIxSpartn < 0) {
            table_.AddCellText("-");
        } else {
            table_.AddCellTextF("%u", port.msgs[msgsIxSpartn]);
        }
        if (msgsIxOther < 0) {
            table_.AddCellText("-");
        } else {
            table_.AddCellTextF("%u", port.msgs[msgsIxOther]);
        }
        table_.SetRowUid(port.portId);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonComms::Update(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonComms::Clear()
{
    table_.ClearRows();
    valid_ = false;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonComms::DrawToolbar()
{

}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonComms::DrawMsg()
{
    if (!valid_) {
        return;
    }

    const bool memError = UBX_MON_COMMS_V0_TXERRORS_MEM(txErrors_);
    const bool allocError = UBX_MON_COMMS_V0_TXERRORS_MEM(txErrors_);
    if (memError || allocError) {
        ImGui::TextUnformatted("Errors:");
        ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_WARNING());
        if (memError) {
            ImGui::SameLine();
            ImGui::TextUnformatted("memory");
        }
        if (allocError) {
            ImGui::SameLine();
            ImGui::TextUnformatted("txbuf");
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::TextUnformatted("Errors: none");
    }

    table_.DrawTable();
}

#if 0

// ---------------------------------------------------------------------------------------------------------------------

bool GuiMsgUbxMonComms::Render(const std::shared_ptr<ParserMsg> &msg, const Vec2f &sizeAvail)
{
    UNUSED(msg);
    if (!valid_)
    {
        return false;
    }

    const ImVec2 topSize = _CalcTopSize(1);

    table_.DrawTable(sizeAvail - topSize);

    return true;
}
#endif

/* ****************************************************************************************************************** */
} // ffgui
