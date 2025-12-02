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
#include "gui_msg_ubx_mon_rf.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxMonRf::GuiMsgUbxMonRf(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input)  // clang-format on
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonRf::Update(const DataMsgPtr& msg)
{
    rfBlocks_.clear();

    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if ((UBX_MON_RF_VERSION(data) != UBX_MON_RF_V0_VERSION) || (UBX_MON_RF_V0_SIZE(data) != size)) {
        return;
    }
    UBX_MON_RF_V0_GROUP0 rf;
    std::memcpy(&rf, &data[UBX_HEAD_SIZE], sizeof(rf));

    const auto nBlocks = std::min<std::size_t>(MAX_BLOCKS, rf.nBlocks);

    blockIqs_.resize(nBlocks);
    rfBlocks_.resize(nBlocks);

    for (std::size_t ix = 0, offs = UBX_HEAD_SIZE + sizeof(rf); ix < nBlocks;
        ix++, offs += sizeof(UBX_MON_RF_V0_GROUP1)) {
        std::memcpy(&rfBlocks_[ix], &data[offs], sizeof(UBX_MON_RF_V0_GROUP1));

        blockIqs_[ix].push_back(
            { (float)rfBlocks_[ix].magI * (1.0f / 255.0f), (float)rfBlocks_[ix].magQ * (1.0f / 255.0f),
              (float)rfBlocks_[ix].ofsI * (2.0f / 255.0f), (float)rfBlocks_[ix].ofsQ * (2.0f / 255.0f) });

        while (blockIqs_[ix].size() > MAX_IQS) {
            blockIqs_[ix].pop_front();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonRf::Update(const Time& now)
{
    UNUSED(now);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonRf::Clear()
{
    blockIqs_.clear();
    rfBlocks_.clear();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonRf::DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxMonRf::DrawMsg()
{
    if (rfBlocks_.empty()) {
        return;
    }

    const float dataOffs = GUI_VAR.charSizeX * 16;

    const auto sizeAvail = ImGui::GetContentRegionAvail();
    const ImVec2 blockSize{ (sizeAvail.x - GUI_VAR.imguiStyle->ItemSpacing.x) / (float)rfBlocks_.size(), sizeAvail.y };

    for (std::size_t ix = 0; ix < rfBlocks_.size(); ix++) {
        const UBX_MON_RF_V0_GROUP1& block = rfBlocks_[ix];

        if (ImGui::BeginChild(ix + 1, blockSize)) {
            Gui::TextTitleF("RF block #%" PRIuMAX, ix);
            // clang-format off
            _RenderStatusFlag(ASTATUS_FLAGS, block.antStatus,                               "Antenna status", dataOffs);
            _RenderStatusFlag(APOWER_FLAGS,  block.antPower,                                "Antenna power",  dataOffs);
            _RenderStatusFlag(JAMMING_FLAGS, UBX_MON_RF_V0_FLAGS_JAMMINGSTATE(block.flags), "Jamming status", dataOffs);
            // clang-format on

            char str[100];

            snprintf(str, sizeof(str), "0x%08x", block.postStatus);
            _RenderStatusText("POST status", str, dataOffs);

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Noise level");
            ImGui::SameLine(dataOffs);
            const float noise = (float)block.noisePerMS / (float)UBX_MON_RF_V0_NOISEPERMS_MAX;
            snprintf(str, sizeof(str), "%u", block.noisePerMS);
            ImGui::ProgressBar(noise, ImVec2(-1.0f, 0.0f), str);

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("AGC monitor");
            ImGui::SameLine(dataOffs);
            const float agc = (float)block.agcCnt / (float)UBX_MON_RF_V0_AGCCNT_MAX;
            snprintf(str, sizeof(str), "%.1f%%", agc * 1e2f);
            ImGui::ProgressBar(agc, ImVec2(-1.0f, 0.0f), str);

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("CW jamming");
            ImGui::SameLine(dataOffs);
            const float jam = (float)block.jamInd / (float)UBX_MON_RF_V0_JAMIND_MAX;
            snprintf(str, sizeof(str), "%.1f%%", jam * 1e2f);
            ImGui::ProgressBar(jam, ImVec2(-1.0f, 0.0f), str);

            GuiMsgUbxMonHw2::DrawIQ(blockIqs_[ix]);
        }
        ImGui::EndChild();

        if (ix < (rfBlocks_.size() - 1)) {
            ImGui::SameLine();
        }
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
