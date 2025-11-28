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
#include "gui_msg_ubx_mon_hw.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxMonHw::GuiMsgUbxMonHw(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input),
    table_   { viewName_ }  // clang-format on
{
    table_.AddColumn("Pin");
    table_.AddColumn("Function");
    table_.AddColumn("Type");
    table_.AddColumn("Level");
    table_.AddColumn("IRQ");
    table_.AddColumn("Pull");
    table_.AddColumn("Virtual");
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw::Update(const DataMsgPtr& msg)
{
    Clear();

    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if (size != UBX_MON_HW_V0_SIZE) {
        return;
    }

    std::memcpy(&hw_, &data[UBX_HEAD_SIZE], sizeof(hw_));
    valid_ = true;

    for (uint32_t pinIx = 0, pinMask = 0x00000001; (pinIx < (uint32_t)NumOf(hw_.VP)) && (pinIx < PIN_NAMES.size());
        pinIx++, pinMask <<= 1) {
        table_.AddCellText(PIN_NAMES[pinIx]);
        const uint8_t vPinIx = hw_.VP[pinIx];
        if (CheckBitsAll(hw_.pinSel, pinMask)) {
            table_.AddCellText(PIO_NAMES[pinIx]);
        } else {
            table_.AddCellText(CheckBitsAll(hw_.pinBank, pinMask) ? PERIPHB_FUNCS[pinIx] : PERIPHA_FUNCS[pinIx]);
        }

        // (G)PIO
        if (CheckBitsAll(hw_.pinSel, pinMask)) {
            table_.AddCellText(CheckBitsAll(hw_.pinDir, pinMask) ? "PIO_OUT" : "PIO_IN");
        }
        // PERIPH function
        else {
            table_.AddCellText(CheckBitsAll(hw_.pinBank, pinMask) ? "PERIPH_B" : "PERIPH_A");
        }

        table_.AddCellText(CheckBitsAll(hw_.pinVal, pinMask) ? "HIGH" : "LOW");
        table_.AddCellText(CheckBitsAll(hw_.pinIrq, pinMask) ? "yes" : "no");  // FIXME: correct?
        const bool pullLo = CheckBitsAll(hw_.pullL, pinMask);
        const bool pullHi = CheckBitsAll(hw_.pullH, pinMask);
        table_.AddCellText(pullLo && pullHi ? "LOW HIGH" : (pullLo ? "LOW" : (pullHi ? "HIGH" : "")));
        if (CheckBitsAll(hw_.usedMask, pinMask)) {
            table_.AddCellTextF(
                "%u - %s", vPinIx, (vPinIx < NumOf(VIRT_FUNCS)) && VIRT_FUNCS[vPinIx] ? VIRT_FUNCS[vPinIx] : "?");
        } else {
            table_.AddCellEmpty();
        }
        table_.SetRowUid(pinIx + 1);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw::Update(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw::Clear()
{
    table_.ClearRows();
    valid_ = false;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw::DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonHw::DrawMsg()
{
    if (!valid_) {
        return;
    }

    const auto sizeAvail = ImGui::GetContentRegionAvail();
    const float dataOffs = 16 * GUI_VAR.charSizeX;
    const ImVec2 topSize = _CalcTopSize(6);
    const ImVec2 topSize2{ 0.5f * (sizeAvail.x - GUI_VAR.imguiStyle->ItemSpacing.x), topSize.y };

    if (ImGui::BeginChild("##Status", topSize2)) {  // clang-format off
        _RenderStatusFlag(RTC_FLAGS,      CheckBitsAll(hw_.flags, UBX_MON_HW_V0_FLAGS_RTCCALIB),   "RTC mode",       dataOffs);
        _RenderStatusFlag(XTAL_FLAGS,     CheckBitsAll(hw_.flags, UBX_MON_HW_V0_FLAGS_XTALABSENT), "RTC XTAL",       dataOffs);
        _RenderStatusFlag(ASTATUS_FLAGS,  hw_.aStatus,                                             "Antenna status", dataOffs);
        _RenderStatusFlag(APOWER_FLAGS,   hw_.aPower,                                              "Antenna power",  dataOffs);
        _RenderStatusFlag(SAFEBOOT_FLAGS, CheckBitsAll(hw_.flags, UBX_MON_HW_V0_FLAGS_SAFEBOOT),   "Safeboot mode",  dataOffs);
        _RenderStatusFlag(JAMMING_FLAGS,  UBX_MON_HW_V0_FLAGS_JAMMINGSTATE(hw_.flags),             "Jamming status", dataOffs);
    }  // clang-format on
    ImGui::EndChild();

    // Gui::VerticalSeparator();
    ImGui::SameLine();

    if (ImGui::BeginChild("##Gauges", topSize2)) {
        char str[100];

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Noise level");
        ImGui::SameLine(dataOffs);
        const float noise = (float)hw_.noisePerMS / (float)UBX_MON_HW_V0_NOISEPERMS_MAX;
        snprintf(str, sizeof(str), "%u", hw_.noisePerMS);
        ImGui::ProgressBar(noise, ImVec2(-1.0f, 0.0f), str);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("AGC monitor");
        ImGui::SameLine(dataOffs);
        const float agc = (float)hw_.agcCnt / (float)UBX_MON_HW_V0_AGCCNT_MAX;
        snprintf(str, sizeof(str), "%.1f%%", agc * 1e2f);
        ImGui::ProgressBar(agc, ImVec2(-1.0f, 0.0f), str);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("CW jamming");
        ImGui::SameLine(dataOffs);
        const float jam = (float)hw_.jamInd / (float)UBX_MON_HW_V0_JAMIND_MAX;
        snprintf(str, sizeof(str), "%.1f%%", jam * 1e2f);
        ImGui::ProgressBar(jam, ImVec2(-1.0f, 0.0f), str);
    }
    ImGui::EndChild();

    table_.DrawTable();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
