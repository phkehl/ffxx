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
#include <fpsdk_common/parser/fpa.hpp>
#include <fpsdk_common/parser/nmea.hpp>
#include <fpsdk_common/parser/ubx.hpp>
//
#include "gui_win_data_inf.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinDataInf::GuiWinDataInf(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 80, 25, 0, 0 }, ImGuiWindowFlags_None, input),
    log_   { WinName(), GUI_CFG.recieverInfHistory } // clang-format off
{
    DEBUG("GuiWinDataInf(%s)", WinName().c_str());
    latestEpochMaxAge_ = {};
    ClearData();
}

GuiWinDataInf::~GuiWinDataInf()
{
    DEBUG("~GuiWinDataInf(%s)", WinName().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataInf::_ProcessData(const DataPtr& data)
{
    if (data->type_ == DataType::MSG) {
        const auto& msg = DataPtrToDataMsg(data);

        if ( (msg.msg_.proto_ == Protocol::UBX) && (ubx::UbxClsId(msg.msg_.Data()) == ubx::UBX_INF_CLSID) )
        {
            switch (ubx::UbxMsgId(msg.msg_.Data())) { // clang-format off
                case ubx::UBX_INF_DEBUG_MSGID:   log_.AddLine("Debug:   " + msg.msg_.info_, C_DEBUG_DEBUG()  ); nDebug_++;   break;
                case ubx::UBX_INF_NOTICE_MSGID:  log_.AddLine("Notice:  " + msg.msg_.info_, C_DEBUG_NOTICE() ); nNotice_++;  break;
                case ubx::UBX_INF_WARNING_MSGID: log_.AddLine("Warning: " + msg.msg_.info_, C_DEBUG_WARNING()); nWarning_++; break;
                case ubx::UBX_INF_ERROR_MSGID:   log_.AddLine("Error:   " + msg.msg_.info_, C_DEBUG_ERROR()  ); nError_++;   break;
                case ubx::UBX_INF_TEST_MSGID:    log_.AddLine("Test:    " + msg.msg_.info_, C_DEBUG_INFO()   ); nOther_++;   break;
                default:                         log_.AddLine("Other:   " + msg.msg_.info_, C_DEBUG_INFO()   ); nOther_++;   break;
            }  // clang-format on
        } else if ((msg.msg_.proto_ == Protocol::NMEA) &&
                   (StrEndsWith(msg.msg_.name_, "-TXT") && (msg.msg_.Size() > 21))) {
            // 01234567890123456789012345 6
            // $GNTXT,01,01,02,blabla*XX\r\n
            const std::string str{ (const char*)&msg.msg_.data_[16], msg.msg_.Size() - 21 };
            switch (msg.msg_.data_[14]) {  // clang-format off
                    case '2': log_.AddLine("Notice:  " + str, C_DEBUG_NOTICE());  nNotice_++;  break;
                    case '1': log_.AddLine("Warning: " + str, C_DEBUG_WARNING()); nWarning_++; break;
                    case '0': log_.AddLine("Error:   " + str, C_DEBUG_ERROR());   nError_++;   break;
                    default:  log_.AddLine("Other:   " + str, C_DEBUG_INFO());    nOther_++;   break;
                }  // clang-format on
        }
        if ((msg.msg_.proto_ == Protocol::FP_A) && (msg.msg_.name_ == fpa::FpaTextPayload::MSG_NAME)) {
            fpa::FpaTextPayload p;
            if (p.SetFromBuf(msg.msg_.data_)) {
                switch (p.level) {  // clang-format off
                        case fpa::FpaTextLevel::INFO:    log_.AddLine("Info:    " + std::string(p.text), C_DEBUG_NOTICE());  nNotice_++;  break;
                        case fpa::FpaTextLevel::WARNING: log_.AddLine("Warning: " + std::string(p.text), C_DEBUG_WARNING()); nWarning_++; break;
                        case fpa::FpaTextLevel::ERROR:   log_.AddLine("Error:   " + std::string(p.text), C_DEBUG_ERROR());   nError_++;   break;
                        default:                         log_.AddLine("Other:   " + std::string(p.text), C_DEBUG_INFO());    nOther_++;   break;
                }  // clang-format on
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataInf::_Loop(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataInf::_ClearData()
{
    log_.Clear();
    nDebug_ = 0;
    nNotice_ = 0;
    nWarning_ = 0;
    nError_ = 0;
    nOther_ = 0;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataInf::_DrawToolbar()
{
    Gui::VerticalSeparator();
    log_.DrawControls();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataInf::_DrawContent()
{
    if (ImGui::BeginTable("Counts", 6, ImGuiTableFlags_HighlightHoveredColumn)) {
        ImGui::TableSetupColumn("   Total", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("  Notice");
        ImGui::TableSetupColumn(" Warning");
        ImGui::TableSetupColumn("   Error");
        ImGui::TableSetupColumn("   Debug");
        ImGui::TableSetupColumn("   Other");
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%8" PRIuMAX, nDebug_ + nNotice_ + nWarning_ + nError_ + nOther_);
        // clang-format off
        ImGui::TableNextColumn(); ImGui::PushStyleColor(ImGuiCol_Text, C_DEBUG_NOTICE());  ImGui::Text("%8" PRIuMAX, nNotice_);  ImGui::PopStyleColor();
        ImGui::TableNextColumn(); ImGui::PushStyleColor(ImGuiCol_Text, C_DEBUG_WARNING()); ImGui::Text("%8" PRIuMAX, nWarning_); ImGui::PopStyleColor();
        ImGui::TableNextColumn(); ImGui::PushStyleColor(ImGuiCol_Text, C_DEBUG_ERROR());   ImGui::Text("%8" PRIuMAX, nError_);   ImGui::PopStyleColor();
        ImGui::TableNextColumn(); ImGui::PushStyleColor(ImGuiCol_Text, C_DEBUG_DEBUG());   ImGui::Text("%8" PRIuMAX, nDebug_);   ImGui::PopStyleColor();
        ImGui::TableNextColumn(); ImGui::PushStyleColor(ImGuiCol_Text, C_DEBUG_INFO());    ImGui::Text("%8" PRIuMAX, nOther_);   ImGui::PopStyleColor();
        // clang-format on
        ImGui::EndTable();
    }

    ImGui::Separator();

    log_.DrawLog();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
