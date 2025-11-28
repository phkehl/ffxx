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

#ifndef __GUI_WIN_HPP__
#define __GUI_WIN_HPP__

//
#include "ffgui_inc.hpp"
//

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWin
{
   public:
    GuiWin(const std::string& name,  // Must be unique among all windows
        const ImVec4& size = {},  // Initial size and minimal size, see Resize() for the units (default: see ctor def)
        const ImGuiWindowFlags flags = ImGuiWindowFlags_None);
    virtual ~GuiWin();

    // clang-format off
    const std::string&   WinName() const                             { return winName_; }
    const std::string&   WinTitle() const                            { return winTitle_; }
    void                 WinSetTitle(const std::string &title = "");
    uint64_t             WinUid() const                              { return winUid_; }
    const std::string&   WinUidStr() const                           { return winUidStr_; }

    void                 WinOpen();
    void                 WinClose();
    void                 WinFocus();
    void                 WinToBack();
    virtual bool         WinIsOpen()                                 { return cfg_.winOpen; }
    bool                 WinIsDrawn() const                          { return winDrawn_; }
    bool                 WinIsFocused() const                        { return winIsFocused_; }
    bool                 WinIsDocked() const                         { return winIsDocked_; }

    void                 WinMoveTo(const ImVec2& pos);         // Position in [px]
    void                 WinResize(const ImVec2& size = {});   // 0 for default, > 0 for units of font size, < 0 for fraction of screen width/height

    const Vec2f&         WinRegionPos() const                       { return winRegionPos_; }
    const Vec2f&         WinRegionSize() const                      { return winRegionSize_; }

    void                 WinToggleFlag(const ImGuiWindowFlags flag, const bool set);
    // clang-format on

    virtual void Loop(const Time& now);
    virtual void DrawWindow();

   protected:
    bool _DrawWindowBegin();
    void _DrawWindowEnd();

   private:
    // clang-format off
    std::string       winName_;      // Unique window name as passed to ctor, won't change
    std::string       winTitle_;     // Displayed title, defaults to winName_
    std::string       winNameImGui_; // Title + constant ID (=winName_) to pass to ImGui window functions
    uint64_t          winUid_;       // Numeric UID for the instance
    std::string       winUidStr_;    // Stringification of the UID
    ImGuiWindowFlags  winFlags_;
    ImVec2            winSizeDef_;   // Initial/default size [px]
    ImVec2            winSizeMin_;   // Minimal size [px]
    Vec2f             winRegionPos_;   // Window region origin (with _DrawWindowBegin())
    Vec2f             winRegionSize_;  // Window region size   (with _DrawWindowBegin())
    // clang-format on

    struct Config
    {
        bool winOpen = false;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, winOpen)
    Config cfg_;

    // clang-format off
    bool              winDrawn_     = false;       // updated by draw
    bool              winIsFocused_ = false;       // updated by draw
    bool              winIsDocked_  = false;       // updated by draw

    ImVec2            winResize_;                  // data for resize in next draw
    bool              winResizePend_ = false;      // resize is pending
    ImVec2            winMoveTo_;                  // data for move in next draw
    bool              winMoveToPend_ = false;      // move is pending
    ImVec2            _WinSizeToPx(ImVec2 size);
    // clang-format on

    std::unique_ptr<ImGuiWindowClass> winClass_;  // docking stuff

    // Initial size and pos when window is created the very first time. Otherwise ImGui restores the previous size and
    // pos from  its saved config. We pick from these intial position (screen coordinates) in turn.
    ImVec2 winInitSize_;
    bool winInitSizePend_ = false;
    ImVec2 winInitPos_;
    bool winInitPosPend_ = false;
    static int winInitPosIx_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_HPP__
