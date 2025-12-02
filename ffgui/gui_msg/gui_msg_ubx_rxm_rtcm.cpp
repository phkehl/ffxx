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
#include <fpsdk_common/parser/rtcm3.hpp>
using namespace fpsdk::common::parser::rtcm3;
//
#include "gui_msg_ubx_rxm_rtcm.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxRxmRtcm::GuiMsgUbxRxmRtcm(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input),
    table_   { viewName_ }  // clang-format on
{
    table_.AddColumn("Message");
    table_.AddColumn("Ref");
    table_.AddColumn("#Used", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("#Unused", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("#Unknown", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("#CrcFail", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Age");
    table_.AddColumn("Desc");
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmRtcm::Update(const DataMsgPtr& msg)
{
    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if ((size != UBX_RXM_RTCM_V2_SIZE) || (UBX_RXM_RTCM_VERSION(data) != UBX_RXM_RTCM_V2_VERSION)) {
        return;
    }

    UBX_RXM_RTCM_V2_GROUP0 rtcm;
    std::memcpy(&rtcm, &data[UBX_HEAD_SIZE], sizeof(rtcm));

    // UID for this entry, also sort key
    const uint64_t uid =  // clang-format off
        ((uint64_t)std::max<uint16_t>(rtcm.refStation, 0) ) |
        ((uint64_t)rtcm.subType << 16) |
        ((uint64_t)rtcm.msgType << 32);  // clang-format on

    // Create new entry if necessary
    auto entry = rtcmInfos_.find(uid);
    if (entry == rtcmInfos_.end()) {
        entry = rtcmInfos_.emplace(uid, RtcmInfo()).first;
        auto& i = entry->second;
        i.msgType_ = rtcm.msgType;
        i.subType_ = rtcm.subType;
        i.refSta_ = rtcm.refStation == 0xffff ? -1 : rtcm.refStation;
        i.name_ = std::to_string(rtcm.msgType) +
                  (rtcm.msgType == RTCM3_TYPE4072_MSGID ? "." + std::to_string(rtcm.subType) : "");
        i.desc_ = Rtcm3GetTypeDesc(rtcm.msgType, rtcm.subType);
        if (!i.desc_) {
            i.desc_ = "Unknown RTCM3 message";
        }
    }

    auto& info = entry->second;

    // Update statistics
    if (UBX_RXM_RTCM_V2_FLAGS_CRCFAILED(rtcm.flags)) {
        info.nCrcFailed_++;
    } else {
        switch (UBX_RXM_RTCM_V2_FLAGS_MSGUSED(rtcm.flags)) {  // clang-format off
            case UBX_RXM_RTCM_V2_FLAGS_MSGUSED_UNUSED:  info.nUnused_++;  break;
            case UBX_RXM_RTCM_V2_FLAGS_MSGUSED_USED:    info.nUsed_++;    break;
            case UBX_RXM_RTCM_V2_FLAGS_MSGUSED_UNKNOWN:
            default:                                    info.nUnknown_++; break;
        }  // clang-format on
    }
    info.lastTime_ = msg->time_;

    dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmRtcm::Update(const Time& now)
{
    now_ = now;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmRtcm::Clear()
{
    rtcmInfos_.clear();
    table_.ClearRows();
    dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmRtcm::DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmRtcm::DrawMsg()
{
    if (dirty_) {
        table_.ClearRows();
        for (auto& [uid, info] : rtcmInfos_) {
            table_.AddCellTextF("%s##%" PRIx32, info.name_.c_str(), info.uid_);
            table_.AddCellTextF("%d", info.refSta_);
            table_.AddCellTextF("%d", info.nUsed_);
            table_.AddCellTextF("%" PRIu32, info.nUnused_);
            table_.AddCellTextF("%" PRIu32, info.nUnknown_);
            table_.AddCellTextF("%" PRIu32, info.nCrcFailed_);
            if (input_->receiver_) {
                table_.AddCellCb(
                    [this](const void* arg) {
                        const float dt = (now_ - ((RtcmInfo*)arg)->lastTime_).GetSec();
                        if (dt < 1000.0f) {
                            ImGui::Text("%5.1f", dt);
                        } else {
                            ImGui::TextUnformatted("oo");
                        }
                    },
                    &info);

            } else {
                table_.AddCellEmpty();
            }
            table_.AddCellText(info.desc_);
            table_.SetRowUid(info.uid_);
        }
    }
    dirty_ = false;

    if (rtcmInfos_.empty()) {
        return;
    }

    table_.DrawTable();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
