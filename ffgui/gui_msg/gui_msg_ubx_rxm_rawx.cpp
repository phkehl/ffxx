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
#include "gui_msg_ubx_rxm_rawx.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxRxmRawx::GuiMsgUbxRxmRawx(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input),
    table_   { viewName_ }  // clang-format on
{
    table_.AddColumn("SV");
    table_.AddColumn("Signal");
    table_.AddColumn("Bd.");
    table_.AddColumn("CNo", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Pseudo range [m]", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Carrier phase [cycles]", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Doppler [Hz]", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Lock [s]", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Half cycle");
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmRawx::Update(const DataMsgPtr& msg)
{
    valid_ = false;

    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if ((UBX_RXM_RAWX_VERSION(data) != UBX_RXM_RAWX_V1_VERSION) || (UBX_RXM_RAWX_V1_SIZE(data) != size)) {
        return;
    }

    UBX_RXM_RAWX_V1_GROUP0 rawx;
    std::memcpy(&rawx, &data[UBX_HEAD_SIZE], sizeof(rawx));

    week_ = rawx.week;
    rcvTow_ = rawx.rcvTow;
    leapSec_ = rawx.leapS;
    leapSecValid_ = UBX_RXM_RAWX_V1_RECSTAT_LEAPSEC(rawx.recStat);
    clkReset_ = UBX_RXM_RAWX_V1_RECSTAT_CLKRESET(rawx.recStat);
    if (clkReset_) {
        lastClkReset_ = msg->time_;
    }

    raws_.clear();
    for (std::size_t measIx = 0, offs = UBX_HEAD_SIZE + sizeof(rawx); measIx < rawx.numMeas;
        measIx++, offs += sizeof(UBX_RXM_RAWX_V1_GROUP1)) {
        UBX_RXM_RAWX_V1_GROUP1 meas;
        std::memcpy(&meas, &data[offs], sizeof(meas));

        const SatSig satSig(
            UbxGnssIdSvIdToSat(meas.gnssId, meas.svId), UbxGnssIdSigIdToSignal(meas.gnssId, meas.sigId));

        Raw raw;
        raw.satSig_ = satSig;
        raw.gloSlot_ = UBX_RXM_RAWX_V1_GROUP1_FREQID_TO_SLOT(meas.freqId);

        raw.cnoValid_ = true;
        raw.cnoMeas_ = (float)meas.cno;

        raw.prValid_ = UBX_RXM_RAWX_V1_TRKSTAT_PRVALID(meas.trkStat);
        raw.prMeas_ = meas.prMeas;
        raw.prStd_ = UBX_RXM_RAWX_V1_PRSTD_SCALE(UBX_RXM_RAWX_V1_PRSTDEV_PRSTD(meas.prStdev));

        raw.cpValid_ = UBX_RXM_RAWX_V1_TRKSTAT_CPVALID(meas.trkStat);
        raw.cpMeas_ = meas.cpMeas;
        raw.cpStd_ = UBX_RXM_RAWX_V1_CPSTD_SCALE(UBX_RXM_RAWX_V1_CPSTDEV_CPSTD(meas.cpStdev));

        raw.doValid_ = true;
        raw.doMeas_ = meas.doMeas;
        raw.doStd_ = UBX_RXM_RAWX_V1_DOSTD_SCALE(UBX_RXM_RAWX_V1_DOSTDEV_DOSTD(meas.doStdev));

        raw.lockTime_ = (double)meas.locktime * UBX_RXM_RAWX_V1_LOCKTIME_SCALE;
        raw.halfCyc_ = UBX_RXM_RAWX_V1_TRKSTAT_HALFCYC(meas.trkStat);
        raw.subHalfCyc_ = UBX_RXM_RAWX_V1_TRKSTAT_SUBHALFCYC(meas.trkStat);

        raws_.push_back(raw);
    }
    std::sort(raws_.begin(), raws_.end());

    table_.ClearRows();
    Sat prevSat;
    for (const auto& raw : raws_) {
        const auto sat = raw.satSig_.GetSat();
        table_.AddCellTextF("%s##%" PRIu32, prevSat != sat ? sat.GetStr() : "", raw.satSig_.id_);
        prevSat = sat;

        if (raw.satSig_.GetGnss() == Gnss::GLO) {
            table_.AddCellTextF("%s (%d)", SignalStr(raw.satSig_.GetSignal()), raw.gloSlot_);
        } else {
            table_.AddCellText(SignalStr(raw.satSig_.GetSignal()));
        }

        table_.AddCellText(BandStr(raw.satSig_.GetBand()));
        table_.AddCellTextF("%.1f", raw.cnoMeas_);
        if (!raw.cnoValid_) {
            table_.SetCellColour(C_TEXT_DIM());
        }

        table_.AddCellTextF("%.2f %6.2f", raw.prMeas_, raw.prStd_);
        if (!raw.prValid_) {
            table_.SetCellColour(C_TEXT_DIM());
        }
        table_.AddCellTextF("%.2f %6.3f", raw.cpMeas_, raw.cpStd_);
        if (!raw.cpValid_) {
            table_.SetCellColour(C_TEXT_DIM());
        }
        table_.AddCellTextF("%.1f %6.3f", raw.doMeas_, raw.doStd_);
        if (!raw.doValid_) {
            table_.SetCellColour(C_TEXT_DIM());
        }

        table_.AddCellTextF("%.3f", raw.lockTime_);

        table_.AddCellTextF("%s %s", raw.halfCyc_ ? "valid  " : "invalid", raw.subHalfCyc_ ? "sub" : "");

        table_.SetRowUid(raw.satSig_.id_);
    }

    valid_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmRawx::Update(const Time& now)
{
    if (!lastClkReset_.IsZero()){
        clkResetAge_ = now - lastClkReset_;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmRawx::Clear()
{
    valid_ = false;
    week_ = 0;
    rcvTow_ = 0.0;
    leapSec_ = 0;
    leapSecValid_ = false;
    clkReset_ = false;
    raws_.clear();
    lastClkReset_ = {};
    clkResetAge_ = {};
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmRawx::DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxRxmRawx::DrawMsg()
{
    if (!valid_) {
        return;
    }

    const ImVec2 topSize = _CalcTopSize(3);
    const float dataOffs = 25 * GUI_VAR.charSizeX;

    if (ImGui::BeginChild("##Status", topSize)) {
        ImGui::TextUnformatted("Receiver local time");
        ImGui::SameLine(dataOffs);
        ImGui::Text("%04d:%010.3f", week_, rcvTow_);
        ImGui::SameLine();
        if (clkReset_) {
            ImGui::TextUnformatted(" (clock reset)");
        } else if (clkResetAge_.IsZero()) {
            ImGui::TextUnformatted(" (last clock reset unknown)");
        } else {
            ImGui::Text(" (last clock reset %s ago)", clkResetAge_.Stringify().c_str());
        }
        ImGui::TextUnformatted("Leap second:");
        ImGui::SameLine(dataOffs);
        ImGui::Text("%d (%s)", leapSec_, leapSecValid_ ? "valid" : " invalid");
        ImGui::TextUnformatted("Measurements:");
        ImGui::SameLine(dataOffs);
        ImGui::Text("%" PRIuMAX, raws_.size());
    }
    ImGui::EndChild();

    table_.DrawTable();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
