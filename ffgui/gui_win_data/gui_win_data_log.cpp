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
#include "gui_win_data_log.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::types;

GuiWinDataLog::GuiWinDataLog(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 80, 25, 0, 0 }, ImGuiWindowFlags_None, input),
    log_   { WinName(), GUI_CFG.receiverLogHistory }  // clang-format on
{
    DEBUG("GuiWinDataLog(%s)", WinName().c_str());
    latestEpochMaxAge_ = {};
    ClearData();
}

GuiWinDataLog::~GuiWinDataLog()
{
    DEBUG("~GuiWinDataLog(%s)", WinName().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataLog::_ProcessData(const DataPtr& data)
{
    switch (data->type_) {
        case DataType::MSG: {
            const auto& msg = DataPtrToDataMsg(data);
            char tmp[256];
            std::snprintf(tmp, sizeof(tmp), "%c %6" PRIuMAX ", size %4" PRIuMAX ", %-20s %s", EnumToVal(msg.origin_),
                msg.msg_.seq_, msg.msg_.Size(), msg.msg_.name_.c_str(),
                msg.msg_.info_.empty() ? "-" : msg.msg_.info_.c_str());
            switch (msg.msg_.proto_) {  // clang-format off
                case Protocol::FP_A:   log_.AddLine(tmp, C_LOG_MSG_FP_A(),   (msg.time_ - GUI_VAR.t0).Stringify() ); break;
                case Protocol::FP_B:   log_.AddLine(tmp, C_LOG_MSG_FP_B(),   (msg.time_ - GUI_VAR.t0).Stringify() ); break;
                case Protocol::NMEA:   log_.AddLine(tmp, C_LOG_MSG_NMEA(),   (msg.time_ - GUI_VAR.t0).Stringify() ); break;
                case Protocol::UBX:    log_.AddLine(tmp, C_LOG_MSG_UBX(),    (msg.time_ - GUI_VAR.t0).Stringify() ); break;
                case Protocol::RTCM3:  log_.AddLine(tmp, C_LOG_MSG_RTCM3(),  (msg.time_ - GUI_VAR.t0).Stringify() ); break;
                case Protocol::UNI_B:  log_.AddLine(tmp, C_LOG_MSG_UNI_B(),  (msg.time_ - GUI_VAR.t0).Stringify() ); break;
                case Protocol::NOV_B:  log_.AddLine(tmp, C_LOG_MSG_NOV_B(),  (msg.time_ - GUI_VAR.t0).Stringify() ); break;
                case Protocol::SBF:    log_.AddLine(tmp, C_LOG_MSG_SBF(),    (msg.time_ - GUI_VAR.t0).Stringify() ); break;
                case Protocol::SPARTN: log_.AddLine(tmp, C_LOG_MSG_SPARTN(), (msg.time_ - GUI_VAR.t0).Stringify() ); break;
                case Protocol::OTHER:  log_.AddLine(tmp, C_LOG_MSG_OTHER(),  (msg.time_ - GUI_VAR.t0).Stringify() ); break;
            }  // clang-format on
            stats_.Update(msg.msg_);
            break;
        }
        case DataType::EPOCH: {
            const auto& epoch = DataPtrToDataEpoch(data);
            char tmp[300];
            std::snprintf(tmp, sizeof(tmp), "E EPOCH %4" PRIuMAX " %s", epoch.epoch_.seq, epoch.epoch_.str);
            log_.AddLine(tmp, C_LOG_EPOCH(), (epoch.time_ - GUI_VAR.t0).Stringify());
            nEpochs_++;
            break;
        }
        case DataType::DEBUG:
        case DataType::EVENT:
            break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataLog::_Loop(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataLog::_ClearData()
{
    log_.Clear();
    stats_ = {};
    nEpochs_ = 0;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataLog::_DrawToolbar()
{
    Gui::VerticalSeparator();
    if (!log_.DrawControls()) {
        ImGui::NewLine();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataLog::_DrawContent()
{
    if (ImGui::BeginTable("Counts", 13, ImGuiTableFlags_HighlightHoveredColumn)) {  // clang-format off

        ImGui::TableSetupColumn("##empty",  ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("  Epochs", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Messages", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("    FP_A", ImGuiTableColumnFlags_WidthFixed | (stats_.n_fpa_    > 0 ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled));
        ImGui::TableSetupColumn("    FP_B", ImGuiTableColumnFlags_WidthFixed | (stats_.n_fpb_    > 0 ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled));
        ImGui::TableSetupColumn("    NMEA", ImGuiTableColumnFlags_WidthFixed | (stats_.n_nmea_   > 0 ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled));
        ImGui::TableSetupColumn("     UBX", ImGuiTableColumnFlags_WidthFixed | (stats_.n_ubx_    > 0 ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled));
        ImGui::TableSetupColumn("   RTCM3", ImGuiTableColumnFlags_WidthFixed | (stats_.n_rtcm3_  > 0 ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled));
        ImGui::TableSetupColumn("   UNI_B", ImGuiTableColumnFlags_WidthFixed | (stats_.n_unib_   > 0 ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled));
        ImGui::TableSetupColumn("   NOV_B", ImGuiTableColumnFlags_WidthFixed | (stats_.n_novb_   > 0 ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled));
        ImGui::TableSetupColumn("     SBF", ImGuiTableColumnFlags_WidthFixed | (stats_.n_sbf_    > 0 ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled));
        ImGui::TableSetupColumn("  SPARTN", ImGuiTableColumnFlags_WidthFixed | (stats_.n_spartn_ > 0 ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled));
        ImGui::TableSetupColumn("   OTHER", ImGuiTableColumnFlags_WidthFixed | (stats_.n_other_  > 0 ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled));
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextUnformatted("Count");
        ImGui::TableNextColumn(); ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_EPOCH());      ImGui::Text("%8" PRIuMAX, nEpochs_);         ImGui::PopStyleColor();
        ImGui::TableNextColumn(); ImGui::PushStyleColor(ImGuiCol_Text, C_C_BRIGHTWHITE());  ImGui::Text("%8" PRIuMAX, stats_.n_msgs_);   ImGui::PopStyleColor();
        ImGui::TableNextColumn(); if (stats_.n_fpa_    > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_FP_A());   ImGui::Text("%8" PRIuMAX, stats_.n_fpa_);    ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_fpb_    > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_FP_B());   ImGui::Text("%8" PRIuMAX, stats_.n_fpb_);    ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_nmea_   > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_NMEA());   ImGui::Text("%8" PRIuMAX, stats_.n_nmea_);   ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_ubx_    > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_UBX());    ImGui::Text("%8" PRIuMAX, stats_.n_ubx_);    ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_rtcm3_  > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_RTCM3());  ImGui::Text("%8" PRIuMAX, stats_.n_rtcm3_);  ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_unib_   > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_UNI_B());  ImGui::Text("%8" PRIuMAX, stats_.n_unib_);   ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_novb_   > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_NOV_B());  ImGui::Text("%8" PRIuMAX, stats_.n_novb_);   ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_sbf_    > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_SBF());    ImGui::Text("%8" PRIuMAX, stats_.n_sbf_);    ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_spartn_ > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_SPARTN()); ImGui::Text("%8" PRIuMAX, stats_.n_spartn_); ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_other_  > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_OTHER());  ImGui::Text("%8" PRIuMAX, stats_.n_other_);  ImGui::PopStyleColor(); }

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextUnformatted("Size");
        ImGui::TableNextColumn(); // ImGui::TextUnformatted("-");
        ImGui::TableNextColumn(); ImGui::PushStyleColor(ImGuiCol_Text, C_C_BRIGHTWHITE());  ImGui::Text("%8" PRIuMAX, stats_.s_msgs_);   ImGui::PopStyleColor();
        ImGui::TableNextColumn(); if (stats_.n_fpa_    > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_FP_A());   ImGui::Text("%8" PRIuMAX, stats_.s_fpa_);    ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_fpb_    > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_FP_B());   ImGui::Text("%8" PRIuMAX, stats_.s_fpb_);    ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_nmea_   > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_NMEA());   ImGui::Text("%8" PRIuMAX, stats_.s_nmea_);   ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_ubx_    > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_UBX());    ImGui::Text("%8" PRIuMAX, stats_.s_ubx_);    ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_rtcm3_  > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_RTCM3());  ImGui::Text("%8" PRIuMAX, stats_.s_rtcm3_);  ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_unib_   > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_UNI_B());  ImGui::Text("%8" PRIuMAX, stats_.s_unib_);   ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_novb_   > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_NOV_B());  ImGui::Text("%8" PRIuMAX, stats_.s_novb_);   ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_sbf_    > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_SBF());    ImGui::Text("%8" PRIuMAX, stats_.s_sbf_);    ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_spartn_ > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_SPARTN()); ImGui::Text("%8" PRIuMAX, stats_.s_spartn_); ImGui::PopStyleColor(); }
        ImGui::TableNextColumn(); if (stats_.n_other_  > 0) { ImGui::PushStyleColor(ImGuiCol_Text, C_LOG_MSG_OTHER());  ImGui::Text("%8" PRIuMAX, stats_.s_other_);  ImGui::PopStyleColor(); }

        ImGui::EndTable();  // clang-format on
    }

    ImGui::Separator();

    log_.DrawLog();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
