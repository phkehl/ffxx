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
#include "gui_msg_ubx_rxm_sfrbx.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxRxmSfrbx::GuiMsgUbxRxmSfrbx(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input),
    table_   { viewName_ }  // clang-format on
{
    table_.AddColumn("SV");
    table_.AddColumn("Signal");
    table_.AddColumn("Age");
    table_.AddColumn("Nav msg");
    table_.AddColumn("Words");
    table_.SetTableScrollFreeze(2, 1);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmSfrbx::Update(const DataMsgPtr& msg)
{
    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if ((size < UBX_RXM_SFRBX_V2_MIN_SIZE) || (UBX_RXM_SFRBX_VERSION(data) != UBX_RXM_SFRBX_V2_VERSION)) {
        return;
    }

    UBX_RXM_SFRBX_V2_GROUP0 sfrbx;
    std::memcpy(&sfrbx, &data[UBX_HEAD_SIZE], sizeof(sfrbx));

    const SatSig satSig(
        UbxGnssIdSvIdToSat(sfrbx.gnssId, sfrbx.svId), UbxGnssIdSigIdToSignal(sfrbx.gnssId, sfrbx.sigId));

    auto entry = sfrbxInfos_.find(satSig);
    if (entry == sfrbxInfos_.end()) {
        entry = sfrbxInfos_.emplace(satSig, SfrbxInfo()).first;
        auto& i = entry->second;
        i.satSig_ = satSig;
    }

    auto& info = entry->second;
    info.gloSlot_ = UBX_RXM_SFRBX_V2_GROUP0_FREQID_TO_SLOT(sfrbx.freqId);

    const uint32_t* dwrds = (uint32_t*)&data[UBX_HEAD_SIZE + sizeof(sfrbx)];
    info.dwrds_.assign(dwrds, &dwrds[sfrbx.numWords]);

    info.dwrdsHex_.clear();
    for (std::size_t ix = 0; ix < sfrbx.numWords; ix++) {
        info.dwrdsHex_ += Sprintf("%08" PRIx32 " ", dwrds[ix]);
    }

    char str[200];
    UbxRxmSfrbxInfo(str, sizeof(str), data, size);
    info.navMsg_ = std::string(str);

    info.lastTime_ = msg->time_;

    dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmSfrbx::Update(const Time& now)
{
    now_ = now;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmSfrbx::Clear()
{
    sfrbxInfos_.clear();
    table_.ClearRows();
    dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmSfrbx::DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmSfrbx::DrawMsg()
{
    if (dirty_) {
        table_.ClearRows();

        Sat prevSat;
        for (auto& [satSig, info] : sfrbxInfos_) {
            const auto sat = info.satSig_.GetSat();
            table_.AddCellTextF("%s##%" PRIu32, prevSat != sat ? sat.GetStr() : "", info.satSig_.id_);
            prevSat = sat;

            if (info.satSig_.GetGnss() == Gnss::GLO) {
                table_.AddCellTextF("%s (%d)", SignalStr(info.satSig_.GetSignal()), info.gloSlot_);
            } else {
                table_.AddCellText(SignalStr(info.satSig_.GetSignal()));
            }

            if (input_->receiver_) {
                table_.AddCellCb(
                    [this](const void* arg) {
                        const float dt = (now_ - ((SfrbxInfo*)arg)->lastTime_).GetSec();
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

            table_.AddCellText(info.navMsg_);
            table_.AddCellText(info.dwrdsHex_);
            table_.SetRowUid(info.satSig_.id_);
        }
    }

    dirty_ = false;

    if (sfrbxInfos_.empty()) {
        return;
    }

    table_.DrawTable();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
