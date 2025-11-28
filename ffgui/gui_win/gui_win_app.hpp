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

#ifndef __GUI_WIN_APP_HPP__
#define __GUI_WIN_APP_HPP__

//
#include "ffgui_inc.hpp"
//
#include <cstdarg>
//
#include "gui_widget_tabbar.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinAppAbout : public GuiWin
{
   public:
    GuiWinAppAbout(const std::vector<std::string>& versions);
    void DrawWindow() override final;

   private:
    std::vector<std::string> _versions;
    void _DrawEntry(const char* name, const char* license, const char* link = nullptr, const char* link2 = nullptr);
};

// ---------------------------------------------------------------------------------------------------------------------

class GuiWinAppSettings : public GuiWin
{
   public:
    GuiWinAppSettings();
    void DrawWindow() override final;

   private:
    GuiWidgetTabbar tabbar_;
};

// ---------------------------------------------------------------------------------------------------------------------

class GuiWinAppHelp : public GuiWin
{
   public:
    GuiWinAppHelp();
    void DrawWindow() override final;

   private:
    GuiWidgetTabbar tabbar_;
    std::vector<std::string> streamHelp_;
};

// ---------------------------------------------------------------------------------------------------------------------

class GuiWinAppLegend : public GuiWin
{
   public:
    GuiWinAppLegend();
    void DrawWindow() override final;

   private:
    GuiWidgetTabbar tabbar_;
    void _DrawColourLegend(const ImU32 colour, const char* fmt, ...) PRINTF_ATTR(3);
};

// ---------------------------------------------------------------------------------------------------------------------

#ifndef IMGUI_DISABLE_DEMO_WINDOWS

class GuiWinAppImguiDemo : public GuiWin
{
   public:
    GuiWinAppImguiDemo();
    void DrawWindow() override final;
};

class GuiWinAppImplotDemo : public GuiWin
{
   public:
    GuiWinAppImplotDemo();
    void DrawWindow() override final;
};

// class GuiWinAppImplot3dDemo : public GuiWin
// {
//    public:
//    GuiWinAppImplot3dDemo();
//     void DrawWindow() override final;
// };

#endif

#ifndef IMGUI_DISABLE_DEBUG_TOOLS

class GuiWinAppImguiMetrics : public GuiWin
{
   public:
    GuiWinAppImguiMetrics();
    void DrawWindow() override final;
};

#endif

class GuiWinAppImplotMetrics : public GuiWin
{
   public:
    GuiWinAppImplotMetrics();
    void DrawWindow() override final;
};

// class GuiWinAppImplot3dMetrics : public GuiWin
// {
//    public:
//    GuiWinAppImplot3dMetrics();
//     void DrawWindow() override final;
// };

class GuiWinAppImguiStyles : public GuiWin
{
   public:
    GuiWinAppImguiStyles();
    void DrawWindow() override final;
};

class GuiWinAppImplotStyles : public GuiWin
{
   public:
    GuiWinAppImplotStyles();
    void DrawWindow() override final;
};

// class GuiWinAppImplot3dStyles : public GuiWin
// {
//    public:
//    GuiWinAppImplot3dStyles();
//     void DrawWindow() override final;
// };

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_APP_HPP__
