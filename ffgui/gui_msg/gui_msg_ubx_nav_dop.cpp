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
#include "gui_msg_ubx_nav_dop.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxNavDop::GuiMsgUbxNavDop(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input)  // clang-format on
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxNavDop::Update(const DataMsgPtr& msg)
{
    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if (size != UBX_NAV_DOP_V0_SIZE) {
        return;
    }

    UBX_NAV_DOP_V0_GROUP0 dop;
    std::memcpy(&dop, &data[UBX_HEAD_SIZE], sizeof(dop));

    gDop_.val_ = (float)dop.gDOP * (float)UBX_NAV_DOP_V0_XDOP_SCALE;
    gDop_.str_ = Sprintf("%.2f", gDop_.val_);
    gDop_.frac_ = std::log10(std::clamp(gDop_.val_, LOG_SCALE, MAX_DOP) * (1.0 / LOG_SCALE)) / MAX_LOG;

    pDop_.val_ = (float)dop.pDOP * (float)UBX_NAV_DOP_V0_XDOP_SCALE;
    pDop_.str_ = Sprintf("%.2f", pDop_.val_);
    pDop_.frac_ = std::log10(std::clamp(pDop_.val_, LOG_SCALE, MAX_DOP) * (1.0 / LOG_SCALE)) / MAX_LOG;

    tDop_.val_ = (float)dop.tDOP * (float)UBX_NAV_DOP_V0_XDOP_SCALE;
    tDop_.str_ = Sprintf("%.2f", tDop_.val_);
    tDop_.frac_ = std::log10(std::clamp(tDop_.val_, LOG_SCALE, MAX_DOP) * (1.0 / LOG_SCALE)) / MAX_LOG;

    vDop_.val_ = (float)dop.vDOP * (float)UBX_NAV_DOP_V0_XDOP_SCALE;
    vDop_.str_ = Sprintf("%.2f", vDop_.val_);
    vDop_.frac_ = std::log10(std::clamp(vDop_.val_, LOG_SCALE, MAX_DOP) * (1.0 / LOG_SCALE)) / MAX_LOG;

    hDop_.val_ = (float)dop.hDOP * (float)UBX_NAV_DOP_V0_XDOP_SCALE;
    hDop_.str_ = Sprintf("%.2f", hDop_.val_);
    hDop_.frac_ = std::log10(std::clamp(hDop_.val_, LOG_SCALE, MAX_DOP) * (1.0 / LOG_SCALE)) / MAX_LOG;

    nDop_.val_ = (float)dop.nDOP * (float)UBX_NAV_DOP_V0_XDOP_SCALE;
    nDop_.str_ = Sprintf("%.2f", nDop_.val_);
    nDop_.frac_ = std::log10(std::clamp(nDop_.val_, LOG_SCALE, MAX_DOP) * (1.0 / LOG_SCALE)) / MAX_LOG;

    eDop_.val_ = (float)dop.eDOP * (float)UBX_NAV_DOP_V0_XDOP_SCALE;
    eDop_.str_ = Sprintf("%.2f", eDop_.val_);
    eDop_.frac_ = std::log10(std::clamp(eDop_.val_, LOG_SCALE, MAX_DOP) * (1.0 / LOG_SCALE)) / MAX_LOG;

    valid_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxNavDop::Update(const Time& now)
{
    UNUSED(now);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxNavDop::Clear()
{
    valid_ = false;
    gDop_ = {};
    pDop_ = {};
    tDop_ = {};
    vDop_ = {};
    hDop_ = {};
    nDop_ = {};
    eDop_ = {};
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxNavDop::DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxNavDop::DrawMsg()
{
    if (!valid_) {
        return;
    }

    const float charWidth = GUI_VAR.charSizeX;
    const float dataOffs = 16 * charWidth;
    const float width = ImGui::GetContentRegionAvail().x - dataOffs;

    ImGui::AlignTextToFramePadding();

    const ImVec2 size{ -1.0f, 0.0f };

    ImGui::TextUnformatted("Geometric DOP");
    ImGui::SameLine(dataOffs);
    ImGui::ProgressBar(gDop_.frac_, size, gDop_.str_.c_str());

    ImGui::TextUnformatted("Position DOP");
    ImGui::SameLine(dataOffs);
    ImGui::ProgressBar(pDop_.frac_, size, pDop_.str_.c_str());

    ImGui::TextUnformatted("Time DOP");
    ImGui::SameLine(dataOffs);
    ImGui::ProgressBar(tDop_.frac_, size, tDop_.str_.c_str());

    ImGui::TextUnformatted("Vertical DOP");
    ImGui::SameLine(dataOffs);
    ImGui::ProgressBar(vDop_.frac_, size, vDop_.str_.c_str());

    ImGui::TextUnformatted("Horizontal DOP");
    ImGui::SameLine(dataOffs);
    ImGui::ProgressBar(hDop_.frac_, size, hDop_.str_.c_str());

    ImGui::TextUnformatted("Northing DOP");
    ImGui::SameLine(dataOffs);
    ImGui::ProgressBar(nDop_.frac_, size, nDop_.str_.c_str());

    ImGui::TextUnformatted("Easting DOP");
    ImGui::SameLine(dataOffs);
    ImGui::ProgressBar(eDop_.frac_, size, eDop_.str_.c_str());

    if (width > (charWidth * 30)) {
        constexpr float o005 = std::log10( 0.5 * (1.0 / LOG_SCALE)) / MAX_LOG;
        constexpr float o010 = std::log10( 1.0 * (1.0 / LOG_SCALE)) / MAX_LOG;
        constexpr float o050 = std::log10( 5.0 * (1.0 / LOG_SCALE)) / MAX_LOG;
        constexpr float o100 = std::log10(10.0 * (1.0 / LOG_SCALE)) / MAX_LOG;
        constexpr float o500 = std::log10(50.0 * (1.0 / LOG_SCALE)) / MAX_LOG;

        ImGui::NewLine();

        ImGui::SameLine(dataOffs - charWidth);
        ImGui::TextUnformatted("0.1");

        ImGui::SameLine(dataOffs + (o005 * width) - charWidth);
        ImGui::TextUnformatted("0.5");

        ImGui::SameLine(dataOffs + (o010 * width) - charWidth);
        ImGui::TextUnformatted("1.0");

        ImGui::SameLine(dataOffs + (o050 * width));
        ImGui::TextUnformatted("5");

        ImGui::SameLine(dataOffs + (o100 * width) - charWidth);
        ImGui::TextUnformatted("10");

        ImGui::SameLine(dataOffs + (o500 * width) - charWidth);
        ImGui::TextUnformatted("50");
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
