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
#include <ffxx/utils.hpp>
#include <fpsdk_common/parser/fpa.hpp>
#include <fpsdk_common/parser/nmea.hpp>
#include <fpsdk_common/parser/ubx.hpp>
using namespace ffxx;
//
#include "gui_win_data_control.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinDataControl::GuiWinDataControl(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 40, 10, 0, 0 }, ImGuiWindowFlags_None, input),
    tabbar_   { WinName() }  // clang-format on
{
    DEBUG("GuiWinDataControl(%s)", WinName().c_str());
    toolbarEna_ = false;
    latestEpochMaxAge_ = {};

    ClearData();
}

GuiWinDataControl::~GuiWinDataControl()
{
    DEBUG("~GuiWinDataControl(%s)", WinName().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataControl::_ProcessData(const DataPtr& /*data*/)
{
    // if (data->type_ == DataType::MSG) {
    //     const auto& msg = DataPtrToDataMsg(data);
    // }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataControl::_Loop(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataControl::_ClearData()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataControl::_DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataControl::_DrawContent()
{
    if (tabbar_.Begin()) {
        tabbar_.Item("u-blox", [this]() { _DrawUblox(); });
        tabbar_.Item("Fixposition", [this]() { _DrawFixposition(); });
        tabbar_.Item("Quectel", [this]() { _DrawQuectel(); });
#ifndef NDEBUG
        tabbar_.Item("Debug", [this]() {
#  define _EVAL(_which_)                                                                                   \
      {                                                                                                    \
          const auto& msg = GetCommonMessage(CommonMessage::_which_);                                      \
          ImGui::Text("%-25s %-20s %2" PRIuMAX " (%s)", STRINGIFY(_which_), msg.name_.c_str(), msg.data_.size(), msg.info_.c_str()); \
      }
            _EVAL(UBX_MON_VER);
            _EVAL(FP_A_VERSION);
            _EVAL(FP_B_VERSION);
            _EVAL(UBX_RESET_HOT)
            _EVAL(UBX_RESET_WARM)
            _EVAL(UBX_RESET_COLD)
            _EVAL(UBX_RESET_SOFT)
            _EVAL(UBX_RESET_HARD)
            _EVAL(UBX_RESET_GNSS_STOP)
            _EVAL(UBX_RESET_GNSS_START)
            _EVAL(UBX_RESET_GNSS_RESTART)
            _EVAL(UBX_RESET_DEFAULT_1)
            _EVAL(UBX_RESET_DEFAULT_2)
            _EVAL(UBX_RESET_FACTORY_1)
            _EVAL(UBX_RESET_FACTORY_2)
            _EVAL(UBX_RESET_SAFEBOOT)
            _EVAL(UBX_TRAINING)
            _EVAL(QUECTEL_LC29H_HOT)
            _EVAL(QUECTEL_LC29H_WARM)
            _EVAL(QUECTEL_LC29H_COLD)
            _EVAL(QUECTEL_LC29H_REBOOT)
            _EVAL(QUECTEL_LC29H_VERNO)
            _EVAL(QUECTEL_LG290P_HOT)
            _EVAL(QUECTEL_LG290P_WARM)
            _EVAL(QUECTEL_LG290P_COLD)
            _EVAL(QUECTEL_LG290P_REBOOT)
            _EVAL(QUECTEL_LG290P_VERNO)
        });
#  undef _EVAL
#endif
        tabbar_.End();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataControl::_DrawUblox()
{
    auto& receiver = *input_->receiver_;

    ImGui::BeginDisabled(receiver.GetState() != StreamState::CONNECTED);

    if (ImGui::Button(ICON_FK_INFO_CIRCLE, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::UBX_MON_VER));
    }
    Gui::ItemTooltip("Poll version information (UBX-MON-VER)");

    Gui::VerticalSeparator();

    if (ImGui::Button(ICON_FK_THERMOMETER_FULL, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::UBX_RESET_HOT));
    }
    Gui::ItemTooltip("Hotstart\nKeep all navigation data, like u-center");

    ImGui::SameLine();

    if (ImGui::Button(ICON_FK_THERMOMETER_HALF, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::UBX_RESET_WARM));
    }
    Gui::ItemTooltip("Warmstart\nDelete ephemerides, like u-center");

    ImGui::SameLine();

    if (ImGui::Button(ICON_FK_THERMOMETER_EMPTY, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::UBX_RESET_COLD));
    }
    Gui::ItemTooltip("Coldstart\nDelete all navigation data, like u-center");

    Gui::VerticalSeparator();

    if (ImGui::Button(ICON_FK_POWER_OFF, GUI_VAR.iconSize)) {
        ImGui::OpenPopup("ResetCommands");
    }
    if (ImGui::BeginPopup("ResetCommands")) {
        if (ImGui::MenuItem("Controlled software reset (0x01)")) {
            receiver.Write(GetCommonMessage(CommonMessage::UBX_RESET_SOFT));
        }
        if (ImGui::MenuItem("Hardware reset (watchdog) after shutdown (0x04)")) {
            receiver.Write(GetCommonMessage(CommonMessage::UBX_RESET_HARD));
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Revert config to default, keep nav data")) {
            receiver.Write(GetCommonMessage(CommonMessage::UBX_RESET_DEFAULT_1));
            receiver.Write(GetCommonMessage(CommonMessage::UBX_RESET_DEFAULT_2));
        }
        if (ImGui::MenuItem("Revert config to default and coldstart")) {
            receiver.Write(GetCommonMessage(CommonMessage::UBX_RESET_FACTORY_1));
            receiver.Write(GetCommonMessage(CommonMessage::UBX_RESET_FACTORY_2));
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Stop navigation")) {
            receiver.Write(GetCommonMessage(CommonMessage::UBX_RESET_GNSS_STOP));
        }
        if (ImGui::MenuItem("Start navigation")) {
            receiver.Write(GetCommonMessage(CommonMessage::UBX_RESET_GNSS_START));
        }
        if (ImGui::MenuItem("Restart navigation")) {
            receiver.Write(GetCommonMessage(CommonMessage::UBX_RESET_GNSS_RESTART));
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Safeboot")) {
            receiver.Write(GetCommonMessage(CommonMessage::UBX_RESET_SAFEBOOT));
        }
        ImGui::EndPopup();
    }
    Gui::ItemTooltip("Reset receiver");

    ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataControl::_DrawFixposition()
{
    auto& receiver = *input_->receiver_;

    ImGui::BeginDisabled(receiver.GetState() != StreamState::CONNECTED);

    if (ImGui::Button(ICON_FK_INFO_CIRCLE "##FpbVersion", GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::FP_B_VERSION));
    }
    Gui::ItemTooltip("Poll version information (FP_B-VERSION)");

    ImGui::SameLine();

    if (ImGui::Button(ICON_FK_INFO_CIRCLE "##FpaVersion", GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::FP_A_VERSION));
    }
    Gui::ItemTooltip("Poll version information (FP_A-VERSION)");

    ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataControl::_DrawQuectel()
{
    auto& receiver = *input_->receiver_;
    ImGui::BeginDisabled(receiver.GetState() != StreamState::CONNECTED);

    // ----- LC29H -----
    ImGui::AlignTextToFramePadding();
    Gui::TextTitle("LC29H ");
    ImGui::SameLine();

    ImGui::PushID("LC29H");
    if (ImGui::Button(ICON_FK_INFO_CIRCLE, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::QUECTEL_LC29H_VERNO));
    }
    Gui::ItemTooltip("Poll version information");

    Gui::VerticalSeparator();

    if (ImGui::Button(ICON_FK_THERMOMETER_FULL, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::QUECTEL_LC29H_HOT));
    }
    Gui::ItemTooltip("Hotstart");

    ImGui::SameLine();

    if (ImGui::Button(ICON_FK_THERMOMETER_HALF, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::QUECTEL_LC29H_WARM));
    }
    Gui::ItemTooltip("Warmstart");

    ImGui::SameLine();

    if (ImGui::Button(ICON_FK_THERMOMETER_EMPTY, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::QUECTEL_LC29H_COLD));
    }
    Gui::ItemTooltip("Coldstart");

    Gui::VerticalSeparator();

    if (ImGui::Button(ICON_FK_POWER_OFF, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::QUECTEL_LC29H_REBOOT));
    }
    Gui::ItemTooltip("Reboot");

    ImGui::PopID();

    ImGui::Separator();

    // ----- LG290P -----
    ImGui::AlignTextToFramePadding();
    Gui::TextTitle("LG290P");
    ImGui::SameLine();

    ImGui::PushID("LG290P");
    if (ImGui::Button(ICON_FK_INFO_CIRCLE, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::QUECTEL_LG290P_VERNO));
    }
    Gui::ItemTooltip("Poll version information");

    Gui::VerticalSeparator();

    if (ImGui::Button(ICON_FK_THERMOMETER_FULL, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::QUECTEL_LG290P_HOT));
    }
    Gui::ItemTooltip("Hotstart");

    ImGui::SameLine();

    if (ImGui::Button(ICON_FK_THERMOMETER_HALF, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::QUECTEL_LG290P_WARM));
    }
    Gui::ItemTooltip("Warmstart");

    ImGui::SameLine();

    if (ImGui::Button(ICON_FK_THERMOMETER_EMPTY, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::QUECTEL_LG290P_COLD));
    }
    Gui::ItemTooltip("Coldstart");

    Gui::VerticalSeparator();

    if (ImGui::Button(ICON_FK_POWER_OFF, GUI_VAR.iconSize)) {
        receiver.Write(GetCommonMessage(CommonMessage::QUECTEL_LG290P_REBOOT));
    }
    Gui::ItemTooltip("Reboot");

    ImGui::PopID();

    ImGui::EndDisabled();
}

/* ******************************************************************************************************************
 */
}  // namespace ffgui
