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
#include <cmath>
#include <cstring>
//
#include <fpsdk_common/math.hpp>
using namespace fpsdk::common::math;
#include <nlohmann/json.hpp>
#include "gui_app.hpp"
#include "imgui_internal.h"
//
#include "gui_win.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

struct GuiWinCfg
{
    bool open = false;
};

GuiWin::GuiWin(const std::string& name, const ImVec4& size, const ImGuiWindowFlags flags) /* clang-format off */ :
    winName_          { name },
    winUid_           { reinterpret_cast<std::uintptr_t>(this) },
    winUidStr_        { Sprintf("%016" PRIxMAX, winUid_) },
    winFlags_         { flags },
    winSizeDef_       { std::max(size.x, 20.0f), std::max(size.y, 5.0f) },
    winSizeMin_       { std::max(size.z,  5.0f), std::max(size.w, 5.0f) },
    winClass_         { std::make_unique<ImGuiWindowClass>() }  // clang-format on
{
    WinSetTitle();  // by default the (internal) name (= ID) is also the displayed title

    // Restore states not restored automatically by ImGui (it handles collapsed, position, size)
    GuiGlobal::LoadObj(winName_ + ".GuiWin", cfg_);

    // Setup initial size and pos, applied in first call to _DrawWindowBegin()
    winInitSize_ = winSizeDef_;
    winInitSizePend_ = true;
    static constexpr std::array<std::array<float, 2>, 9> NEW_WIN_POS = { { { 20, 20 }, { 40, 40 }, { 60, 60 },
        { 80, 80 }, { 100, 100 }, { 120, 120 }, { 140, 140 }, { 160, 160 }, { 180, 180 } } };
    winInitPos_ = NEW_WIN_POS[winInitPosIx_++];
    winInitPosIx_ %= NEW_WIN_POS.size();
    winInitPosPend_ = true;

    DEBUG("GuiWin(%s) %s def %.1fx%.1f min %.1fx%.1f pos %+.1f%+.1f", winName_.c_str(),
        cfg_.winOpen ? "open" : "closed", winSizeDef_.x, winSizeDef_.y, winSizeMin_.x, winSizeMin_.y, winInitPos_.x,
        winInitPos_.y);
}

GuiWin::~GuiWin()
{
    DEBUG("~GuiWin(%s)", WinName().c_str());
    GuiGlobal::SaveObj(winName_ + ".GuiWin", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ int GuiWin::winInitPosIx_ = 0;

// ---------------------------------------------------------------------------------------------------------------------

void GuiWin::WinOpen()
{
    cfg_.winOpen = true;
    WinFocus();
    ImGui::SetWindowCollapsed(winNameImGui_.c_str(), false);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWin::WinClose()
{
    cfg_.winOpen = false;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWin::WinSetTitle(const std::string& title)
{
    if (title.empty()) {
        winTitle_ = winName_;
    } else {
        winTitle_ = title;
    }
    winNameImGui_ = winTitle_ + std::string("###") + winName_;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWin::WinFocus()
{
    ImGui::SetWindowFocus(winNameImGui_.c_str());
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWin::WinToBack()
{
    auto win = ImGui::FindWindowByName(winNameImGui_.c_str());
    ImGui::SetWindowFocus(NULL);
    ImGui::BringWindowToDisplayBack(win);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWin::WinMoveTo(const ImVec2& pos)
{
    winMoveTo_ = pos;
    winMoveToPend_ = true;
}

void GuiWin::WinResize(const ImVec2& size)
{
    if ((size.x != 0.0f) || (size.y != 0.0f)) {
        winResize_ = size;
    } else {
        winResize_ = winSizeDef_;
    }
    winResizePend_ = true;
}

ImVec2 GuiWin::_WinSizeToPx(ImVec2 size)
{
    auto& io = ImGui::GetIO();
    size.x *= size.x < 0.0 ? -io.DisplaySize.x : GUI_VAR.charSizeX;
    if (size.y != 0.0f) {
        size.y *= size.y < 0.0 ? -io.DisplaySize.y : GUI_VAR.charSizeY;
    } else {
        size.y = size.x;
    }
    return size;
}
// ---------------------------------------------------------------------------------------------------------------------

void GuiWin::WinToggleFlag(const ImGuiWindowFlags flag, const bool set)
{
    if (set) {
        SetBits(winFlags_, flag);
    } else {
        ClearBits(winFlags_, flag);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWin::Loop(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWin::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }
    ImGui::TextUnformatted("Hello!");
    _DrawWindowEnd();
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWin::_DrawWindowBegin()
{
    // Start drawing window

    // Not an automatically sized window. Otherwise ImGui deals with the auto resizing.
    if (!CheckBitsAll<int>(winFlags_, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Set initial size
        if (winInitSizePend_) {
            const auto size = _WinSizeToPx(winInitSize_);
            DEBUG("GuiWin(%s) init size %.1fx%.1f -> %.1fx%.1f", winName_.c_str(), winInitSize_.x, winInitSize_.y,
                size.x, size.y);
            ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
            winInitSizePend_ = false;
        }
        // Resize on request
        else if (winResizePend_) {
            const auto size = _WinSizeToPx(winResize_);
            DEBUG("GuiWin(%s) resize %.1fx%.1f -> %.1fx%.1f", winName_.c_str(), winResize_.x, winResize_.y, size.x,
                size.y);
            ImGui::SetNextWindowSize(size, ImGuiCond_Always);
            winResizePend_ = false;
        }
        if (winSizeMin_.x != 0.0f) {
            ImGui::SetNextWindowSizeConstraints(_WinSizeToPx(winSizeMin_), ImVec2(FLT_MAX, FLT_MAX));
        }
    }

    ImGui::SetNextWindowClass(winClass_.get());  // docking stuff

    // Set position?
    if (winInitPosPend_) {
        DEBUG("GuiWin(%s) init pos %.1fx%.1f", winName_.c_str(), winInitPos_.x, winInitPos_.y);
        ImGui::SetNextWindowPos(winInitPos_, ImGuiCond_FirstUseEver);
        winInitPosPend_ = false;
    } else if (winMoveToPend_) {
        DEBUG("GuiWin(%s) moveto %.1fx%.1f", winName_.c_str(), winMoveTo_.x, winMoveTo_.y);
        ImGui::SetNextWindowPos(winMoveTo_, ImGuiCond_Always);
        winMoveToPend_ = false;
    }

    winDrawn_ = ImGui::Begin(winNameImGui_.c_str(), &cfg_.winOpen, winFlags_);
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("To back")) { WinToBack(); }
        if (ImGui::MenuItem("Resize")) { WinResize(); }
        if (ImGui::MenuItem("Close")) { WinClose(); }
        ImGui::EndPopup();
    }

    winRegionPos_ = ImGui::GetCursorScreenPos();
    winRegionSize_ = ImGui::GetContentRegionAvail();

    if (!winDrawn_) {
        winIsFocused_ = false;
        ImGui::End();
    }

    return winDrawn_;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWin::_DrawWindowEnd()
{
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    winIsDocked_ = win->DockIsActive;
    winIsFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::End();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
