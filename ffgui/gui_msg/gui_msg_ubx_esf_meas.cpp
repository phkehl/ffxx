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
#include "gui_msg_ubx_esf_meas.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxEsfMeas::GuiMsgUbxEsfMeas(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input),
    table_   { viewName_ }  // clang-format on
{
    table_.AddColumn("Measurement");
    table_.AddColumn("Value", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Raw");
    table_.AddColumn("Ttag sensor", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Ttag rx", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    // table_.AddColumn("Source"); // TODO: u-center shows "1" for source. What's that?
    table_.AddColumn("Provider");
    table_.AddColumn("Count", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
    table_.AddColumn("Age", 0.0f, GuiWidgetTable::ColumnFlags::ALIGN_RIGHT);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxEsfMeas::Update(const DataMsgPtr& msg)
{
    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if (UBX_ESF_MEAS_V0_SIZE(data) != size) {
        return;
    }

    UBX_ESF_MEAS_V0_GROUP0 head;
    std::memcpy(&head, &data[UBX_HEAD_SIZE], sizeof(head));
    const std::size_t numMeas = UBX_ESF_MEAS_V0_FLAGS_NUMMEAS(head.flags);
    const uint8_t timeMarkSent = UBX_ESF_MEAS_V0_FLAGS_TIMEMARKSENT(head.flags);
    const bool calibTtagValid = UBX_ESF_MEAS_V0_FLAGS_CALIBTTAGVALID(head.flags);

    uint32_t calibTtag = 0;
    if (calibTtagValid) {
        UBX_ESF_MEAS_V0_GROUP2 ttag;
        std::memcpy(&ttag,
            &data[UBX_HEAD_SIZE + sizeof(UBX_ESF_MEAS_V0_GROUP0) + (numMeas * sizeof(UBX_ESF_MEAS_V0_GROUP1))],
            sizeof(ttag));
        calibTtag = ttag.calibTtag;
    }

    for (std::size_t measIx = 0, offs = UBX_HEAD_SIZE + sizeof(UBX_ESF_MEAS_V0_GROUP0); measIx < numMeas;
        measIx++, offs += sizeof(UBX_ESF_MEAS_V0_GROUP1)) {
        UBX_ESF_MEAS_V0_GROUP1 meas;
        std::memcpy(&meas, &data[offs], sizeof(meas));
        const int dataType = UBX_ESF_MEAS_V0_DATA_DATATYPE(meas.data);
        const uint32_t dataField = UBX_ESF_MEAS_V0_DATA_DATAFIELD(meas.data);

        auto* measDef =
            (dataType < (int)MEAS_DEFS.size()) && MEAS_DEFS[dataType].name_ ? &MEAS_DEFS[dataType] : nullptr;

        // (Unique) ID name for this entry
        const uint32_t uid = dataType;
        const std::string name = Sprintf("%s (%d)##%u", measDef ? measDef->name_ : "Unknown", dataType, uid);

        // Find entry, create new one if necessary
        auto entry = measInfos_.find(uid);
        if (entry == measInfos_.end()) {
            entry = measInfos_.emplace(uid, MeasInfo()).first;
        }

        auto& info = entry->second;

        // Populate/update info
        info.nMeas_++;
        info.lastTime_ = msg->time_;
        info.name_ = name;
        info.rawHex_ = Sprintf("0x%06x", dataField);
        info.ttagSens_ = Sprintf("%.3f", (double)head.timeTag * UBX_ESF_MEAS_V0_CALIBTTAG_SCALE);
        info.ttagRx_ = calibTtagValid ? Sprintf("%.3f", (double)calibTtag * UBX_ESF_MEAS_V0_CALIBTTAG_SCALE) : "";
        info.provider_ = std::to_string(head.id);

        if (measDef) {
            double value = 0.0f;
            switch (measDef->type_) {
                case MeasDef::SIGNED: {
                    // sign extend à la https://graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend
                    struct
                    {
                        signed int x : 24;
                    } s;
                    s.x = dataField;
                    const int32_t v = s.x;
                    value = (double)v * measDef->scale_;
                    info.value_ = Sprintf(measDef->fmt_, value);
                    break;
                }
                case MeasDef::UNSIGNED:
                    value = (double)dataField * measDef->scale_;
                    info.value_ = Sprintf(measDef->fmt_, value);
                    break;
                case MeasDef::TICK: {
                    const int32_t v =
                        (int32_t)(dataField & 0x007fffff) * (CheckBitsAll<uint32_t>(dataField, 0x00800000) ? -1 : 1);
                    value = (double)v * measDef->scale_;
                    info.value_ = Sprintf(measDef->fmt_, value);
                    break;
                }
            }
            if (measDef->unit_) {
                info.value_ += " ";
                info.value_ += measDef->unit_;
            }

            info.plotData_.push_back(value);
            while (info.plotData_.size() > MAX_PLOT_DATA) {
                info.plotData_.pop_front();
            }
        }

        UNUSED(timeMarkSent);  // TODO
    }

    dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxEsfMeas::Update(const Time& now)
{
    now_ = now;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxEsfMeas::Clear()
{
    measInfos_.clear();
    table_.ClearRows();
    resetPlotRange_ = true;
    dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxEsfMeas::DrawToolbar()
{
    ImGui::SameLine();

    ImGui::BeginDisabled(measInfos_.empty() || autoPlotRange_);
    if (ImGui::Button(ICON_FK_ARROWS_ALT "###ResetPlotRange")) {
        resetPlotRange_ = true;
    }
    Gui::ItemTooltip("Reset plot range");
    ImGui::EndDisabled();

    ImGui::SameLine();
    Gui::ToggleButton(ICON_FK_CROP "###AutoPlotRange", NULL, &autoPlotRange_, "Automatically adjusting plot range",
        "Not automatically adjusting plot range", GUI_VAR.iconSize);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxEsfMeas::DrawMsg()
{
    if (dirty_) {
        table_.ClearRows();

        for (auto& [uid, info] : measInfos_) {
            table_.AddCellText(info.name_, uid);
            table_.AddCellText(info.value_);
            table_.AddCellText(info.rawHex_);
            table_.AddCellText(info.ttagSens_);
            table_.AddCellText(info.ttagRx_);
            // table_.AddCellText(info.source_);
            table_.AddCellText(info.provider_);
            table_.AddCellTextF("%u", info.nMeas_);
            if (input_->receiver_) {
                table_.AddCellCb(
                    [this](const void* arg) {
                        const float dt = (now_ - ((MeasInfo*)arg)->lastTime_).GetSec();
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
            table_.SetRowUid(uid);
        }
    }

    dirty_ = false;

    if (measInfos_.empty()) {
        return;
    }

    const auto sizeAvail = ImGui::GetContentRegionAvail();
    const float tableReqHeight =
        (table_.GetNumRows() + 1) * (GUI_VAR.charSizeY + GUI_VAR.imguiStyle->ItemSpacing.y + 2.0f);
    Vec2f tableSize{ sizeAvail.x, (0.5f * sizeAvail.y) - GUI_VAR.imguiStyle->ItemSpacing.y };
    if (tableSize.y > tableReqHeight) {
        tableSize.y = tableReqHeight;
    }

    table_.DrawTable(tableSize);

    if (resetPlotRange_ || autoPlotRange_) {
        ImPlot::SetNextAxesToFit();
    }
    if (ImPlot::BeginPlot("##meas", ImVec2(sizeAvail.x, sizeAvail.y - tableSize.y - GUI_VAR.imguiStyle->ItemSpacing.y),
            ImPlotFlags_Crosshairs | ImPlotFlags_NoFrame)) {
        ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);
        if (resetPlotRange_ || autoPlotRange_) {
            ImPlot::SetupAxesLimits(ImAxis_X1, 0, MAX_PLOT_DATA, ImGuiCond_Always);
        }
        ImPlot::SetupFinish();

        for (auto& entry : measInfos_) {
            if (!table_.IsSelected(entry.first)) {
                continue;
            }
            auto& info = entry.second;

            ImPlot::PlotLineG(
                info.name_.c_str(),
                [](int ix, void* arg) {
                    const std::deque<double>* pd = (const std::deque<double>*)arg;
                    return ImPlotPoint(ix, pd->at(ix));
                },
                &info.plotData_, info.plotData_.size());
        }

        ImPlot::EndPlot();
    }

    resetPlotRange_ = false;
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
