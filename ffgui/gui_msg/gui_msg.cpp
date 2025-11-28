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
#include "gui_msg.hpp"
#include "gui_msg_ubx_cfg_valxxx.hpp"
#include "gui_msg_ubx_esf_meas.hpp"
#include "gui_msg_ubx_esf_status.hpp"
#include "gui_msg_ubx_mon_comms.hpp"
#include "gui_msg_ubx_mon_hw.hpp"
#include "gui_msg_ubx_mon_hw2.hpp"
#include "gui_msg_ubx_mon_hw3.hpp"
#include "gui_msg_ubx_mon_rf.hpp"
#include "gui_msg_ubx_mon_span.hpp"
#include "gui_msg_ubx_mon_ver.hpp"
#include "gui_msg_ubx_nav_cov.hpp"
#include "gui_msg_ubx_nav_dop.hpp"
#include "gui_msg_ubx_rxm_rawx.hpp"
#include "gui_msg_ubx_rxm_rtcm.hpp"
#include "gui_msg_ubx_rxm_sfrbx.hpp"
#include "gui_msg_ubx_rxm_spartn.hpp"
#include "gui_msg_ubx_tim_tm2.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsg::GuiMsg(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    viewName_   { viewName },
    input_      { input }  // clang-format on
{
}

// ---------------------------------------------------------------------------------------------------------------------

ImVec2 GuiMsg::_CalcTopSize(const int nLinesOfTopText)
{
    const float topHeight = nLinesOfTopText * (GUI_VAR.charSizeY + GUI_VAR.imguiStyle->ItemSpacing.y);
    return ImVec2(0.0f, topHeight);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsg::_RenderStatusText(const std::string& label, const std::string& text, const float dataOffs)
{
    ImGui::TextUnformatted(label.c_str());
    ImGui::SameLine(dataOffs);
    ImGui::TextUnformatted(text.c_str());
}

void GuiMsg::_RenderStatusText(const std::string& label, const char* text, const float dataOffs)
{
    ImGui::TextUnformatted(label.c_str());
    ImGui::SameLine(dataOffs);
    ImGui::TextUnformatted(text);
}

void GuiMsg::_RenderStatusText(const char* label, const std::string& text, const float dataOffs)
{
    ImGui::TextUnformatted(label);
    ImGui::SameLine(dataOffs);
    ImGui::TextUnformatted(text.c_str());
}

void GuiMsg::_RenderStatusText(const char* label, const char* text, const float dataOffs)
{
    ImGui::TextUnformatted(label);
    ImGui::SameLine(dataOffs);
    ImGui::TextUnformatted(text);
}

/* ****************************************************************************************************************** */

/*static*/ std::unique_ptr<GuiMsg> GuiMsg::GetInstance(const std::string& msgName, const std::string& winName, const InputPtr& input)
{
    // clang-format off
    if      (msgName == UBX_ESF_MEAS_STRID)   { return std::make_unique<GuiMsgUbxEsfMeas>(   winName + "." + msgName, input ); }
    else if (msgName == UBX_ESF_STATUS_STRID) { return std::make_unique<GuiMsgUbxEsfStatus>( winName + "." + msgName, input ); }
    else if (msgName == UBX_MON_COMMS_STRID)  { return std::make_unique<GuiMsgUbxMonComms>(  winName + "." + msgName, input ); }
    else if (msgName == UBX_MON_HW_STRID)     { return std::make_unique<GuiMsgUbxMonHw>(     winName + "." + msgName, input ); }
    else if (msgName == UBX_MON_HW2_STRID)    { return std::make_unique<GuiMsgUbxMonHw2>(    winName + "." + msgName, input ); }
    else if (msgName == UBX_MON_HW3_STRID)    { return std::make_unique<GuiMsgUbxMonHw3>(    winName + "." + msgName, input ); }
    else if (msgName == UBX_MON_RF_STRID)     { return std::make_unique<GuiMsgUbxMonRf>(     winName + "." + msgName, input ); }
    else if (msgName == UBX_MON_SPAN_STRID)   { return std::make_unique<GuiMsgUbxMonSpan>(   winName + "." + msgName, input ); }
    else if (msgName == UBX_MON_VER_STRID)    { return std::make_unique<GuiMsgUbxMonVer>(    winName + "." + msgName, input ); }
    else if (msgName == UBX_NAV_COV_STRID)    { return std::make_unique<GuiMsgUbxNavCov>(    winName + "." + msgName, input ); }
    else if (msgName == UBX_NAV_DOP_STRID)    { return std::make_unique<GuiMsgUbxNavDop>(    winName + "." + msgName, input ); }
    else if (msgName == UBX_RXM_RAWX_STRID)   { return std::make_unique<GuiMsgUbxRxmRawx>(   winName + "." + msgName, input ); }
    else if (msgName == UBX_RXM_RTCM_STRID)   { return std::make_unique<GuiMsgUbxRxmRtcm>(   winName + "." + msgName, input ); }
    else if (msgName == UBX_RXM_SFRBX_STRID)  { return std::make_unique<GuiMsgUbxRxmSfrbx>(  winName + "." + msgName, input ); }
    else if (msgName == UBX_RXM_SPARTN_STRID) { return std::make_unique<GuiMsgUbxRxmSpartn>( winName + "." + msgName, input ); }
    else if (msgName == UBX_TIM_TM2_STRID)    { return std::make_unique<GuiMsgUbxTimTm2>(    winName + "." + msgName, input ); }
    else if (msgName == UBX_CFG_VALGET_STRID) { return std::make_unique<GuiMsgUbxCfgValxxx>( winName + "." + msgName, input ); }
    else if (msgName == UBX_CFG_VALSET_STRID) { return std::make_unique<GuiMsgUbxCfgValxxx>( winName + "." + msgName, input ); }
    else if (msgName == UBX_CFG_VALDEL_STRID) { return std::make_unique<GuiMsgUbxCfgValxxx>( winName + "." + msgName, input ); }
    // clang-format on
    return nullptr;
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
