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
#include <fpsdk_common/parser/spartn.hpp>
using namespace fpsdk::common::parser::spartn;
//
#include "gui_msg_ubx_rxm_spartn.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxRxmSpartn::GuiMsgUbxRxmSpartn(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input),
    table_   { viewName_ }  // clang-format on
{
    table_.AddColumn("Message");
    table_.AddColumn("#Used", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("#Unused", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("#Unknown", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Age");
    table_.AddColumn("Desc");
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmSpartn::Update(const DataMsgPtr& msg)
{
    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if ((size != UBX_RXM_SPARTN_V1_SIZE) || (UBX_RXM_RTCM_VERSION(data) != UBX_RXM_SPARTN_V1_VERSION)) {
        return;
    }

    UBX_RXM_SPARTN_V1_GROUP0 spartn;
    std::memcpy(&spartn, &data[UBX_HEAD_SIZE], sizeof(spartn));

    // UID for this entry, also the std::map sort
    const uint64_t uid = ((uint64_t)spartn.subType) | (uint64_t)spartn.msgType << 16;

    auto entry = spartnInfos_.find(uid);
    if (entry == spartnInfos_.end()) {
        entry = spartnInfos_.emplace(uid, SpartnInfo()).first;
        auto& i = entry->second;
        i.msgType_ = spartn.msgType;
        i.subType_ = spartn.subType;
        i.name_ = std::to_string(spartn.msgType) + "." + std::to_string(spartn.subType);
        i.desc_ = SpartnGetTypeDesc(spartn.msgType, spartn.subType);
        if (!i.desc_) {
            i.desc_ = "Unknown SPARTN message";
        }
    }

    auto& info = entry->second;

    // Update statistics
    // const bool crcFailed = CheckBitsAll(spartn.flags, UBX_RXM_SPARTN_V1_FLAGS_CRCFAILED);
    // if (crcFailed)
    // {
    //     info->nCrcFailed++;
    // }
    // else
    {
        switch (UBX_RXM_SPARTN_V1_FLAGS_MSGUSED(spartn.flags)) {  // clang-format off
            case UBX_RXM_SPARTN_V1_FLAGS_MSGUSED_UNUSED:  info.nUnused_++;  break;
            case UBX_RXM_SPARTN_V1_FLAGS_MSGUSED_USED:    info.nUsed_++;    break;
            case UBX_RXM_SPARTN_V1_FLAGS_MSGUSED_UNKNOWN:
            default:                                      info.nUnknown_++; break;
        }  // clang-format on
    }
    info.lastTime_ = msg->time_;

    dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmSpartn::Update(const Time& now)
{
    now_ = now;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmSpartn::Clear()
{
    spartnInfos_.clear();
    table_.ClearRows();
    dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmSpartn::DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmSpartn::DrawMsg()
{
    if (dirty_) {
        table_.ClearRows();
        for (auto& [uid, info] : spartnInfos_) {
            table_.AddCellTextF("%s##%" PRIx32, info.name_.c_str(), info.uid_);
            table_.AddCellTextF("%" PRIu32, info.nUsed_);
            table_.AddCellTextF("%" PRIu32, info.nUnused_);
            table_.AddCellTextF("%" PRIu32, info.nUnknown_);
            if (input_->receiver_) {
                table_.AddCellCb(
                    [this](const void* arg) {
                        const float dt = (now_ - ((SpartnInfo*)arg)->lastTime_).GetSec();
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

    if (spartnInfos_.empty()) {
        return;
    }

    table_.DrawTable();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
