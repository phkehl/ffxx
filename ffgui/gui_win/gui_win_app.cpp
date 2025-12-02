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
#include <cstring>
//
#include <ffxx/stream.hpp>
#include <ubloxcfg/ubloxcfg.h>
using namespace ffxx;
//
#include "gui_win_app.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinAppAbout::GuiWinAppAbout(const std::vector<std::string>& versions) /* clang-format off */ :
    GuiWin("About", {}, ImGuiWindowFlags_AlwaysAutoResize),
    _versions       { versions }  // clang-format on
{
}

void GuiWinAppAbout::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }

    GuiPushFontBold();
    Gui::TextTitle("ffgui " FF_VERSION_STRING IF_DEBUG(" -- debug", ""));
    GuiPopFont();

    // clang-format off
    GuiPushFontItalic();
    ImGui::TextUnformatted( "Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)");
    GuiPopFont();
    ImGui::TextLinkOpenURL( "https://oinkzwurgl.org/projaeggd/ffxx/");
    ImGui::TextUnformatted("Licenses: See the LICENSE files included in the source distribution");
    ImGui::Separator();  // -----------------------------------------------------------------------------
    ImGui::TextUnformatted("This consists of the following parts:");
    _DrawEntry("'ffgui' application",         "GPLv3 license");
    _DrawEntry("'ffxx' library",              "GPLv3 license");
    _DrawEntry("'ubloxcfg' library",          "LGPLv3 license");
    ImGui::Separator();  // -----------------------------------------------------------------------------
    ImGui::TextUnformatted( "Third-party code (see source code and the links for details):");
    _DrawEntry("Dear Imgui",                  "MIT license",          "https://github.com/ocornut/imgui");
    _DrawEntry("ImPlot",                      "MIT license",          "https://github.com/epezent/implot");
    _DrawEntry("PlatformFolders",             "MIT license",          "https://github.com/sago007/PlatformFolders");
    _DrawEntry("DejaVu fonts",                "various licenses",     "https://dejavu-fonts.github.io");
    _DrawEntry("ForkAwesome font",            "MIT license",          "https://forkaweso.me", "https://github.com/ForkAwesome/Fork-Awesome");
    _DrawEntry("Tetris",                      "Revised BSD license",  "https://brennan.io/2015/06/12/tetris-reimplementation/");
    _DrawEntry("zfstream",                    "unnamed license",      "https://github.com/madler/zlib/tree/master/contrib/iostream3");
    _DrawEntry("Base64",                      "MIT license",          "https://gist.github.com/tomykaira/f0fd86b6c73063283afe550bc5d77594");
    _DrawEntry("glm",                         "Happy Bunny license",  "https://github.com/g-truc/glm");
    ImGui::Separator();  // -----------------------------------------------------------------------------
    ImGui::TextUnformatted("Third-party libraries (first level dependencies, use ldd to see all):");
    _DrawEntry("json",                        "MIT license",          "https://github.com/nlohmann/json", "https://json.nlohmann.me/");    _DrawEntry("Fixposition SDK",             "MIT license",          "https://github.com/fixposition/fixposition-sdk", "https://fixposition.com");
    _DrawEntry("GLFW",                        "zlib/libpng license",  "https://www.glfw.org", "https://github.com/glfw/glfw");
    _DrawEntry("libcurl",                     "unnamed license",      "https://curl.se/", "https://github.com/curl/curl");
    _DrawEntry("FreeType",                    "FreeType license",     "https://freetype.org/", "https://gitlab.freedesktop.org/freetype/freetype");
    ImGui::Separator();  // -----------------------------------------------------------------------------
    // clang-format on
    ImGui::TextUnformatted("Third-party data from the following public sources:");
    int nSources = 0;
    const char** sources = ubloxcfg_getSources(&nSources);
    for (int ix = 0; ix < nSources; ix++) {
        ImGui::Text("- %s", sources[ix]);
    }
    ImGui::Separator();  // -----------------------------------------------------------------------------
    ImGui::TextUnformatted("Versions:");
    for (const auto& version : _versions) {
        ImGui::TextWrapped("- %s", version.c_str());
    }
    ImGui::Separator();  // -----------------------------------------------------------------------------
    ImGui::TextUnformatted("Data:");
    ImGui::TextUnformatted("- Config directory:");
    ImGui::SameLine();
    ImGui::TextLinkOpenURL(GuiGlobal::cache_.configDir.c_str());
    ImGui::TextUnformatted("- Cache directory:");
    ImGui::SameLine();
    ImGui::TextLinkOpenURL(GuiGlobal::cache_.cacheDir.c_str());

    _DrawWindowEnd();
}

void GuiWinAppAbout::_DrawEntry(const char* name, const char* license, const char* link, const char* link2)
{
    ImGui::TextUnformatted("- ");
    ImGui::SameLine(0, 0);
    ImGui::PushFont(GUI_VAR.fontItalic, 0.0f);
    ImGui::TextUnformatted(name);
    ImGui::PopFont();

    if (license) {
        ImGui::SameLine(GUI_VAR.charSizeX * 30);
        ImGui::TextUnformatted(license);
    }

    if (link) {
        ImGui::SameLine(GUI_VAR.charSizeX * 51);
        ImGui::TextLinkOpenURL(link);
        // Gui::TextLink(link);
    }

    if (link2) {
        ImGui::SameLine();
        ImGui::TextLinkOpenURL(link2);
        // Gui::TextLink(link2);
    }
}

/* ****************************************************************************************************************** */

GuiWinAppSettings::GuiWinAppSettings() /* clang-format off */ :
    GuiWin("Settings", { 80, 35, 75, 15 }),
    tabbar_ { WinName() }  // clang-format on
{
}

void GuiWinAppSettings::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }

    if (tabbar_.Begin()) {
        tabbar_.Item("Font", GuiGlobal::DrawSettingsEditorFont);
        tabbar_.Item("Colours", GuiGlobal::DrawSettingsEditorColours);
        tabbar_.Item("Maps", GuiGlobal::DrawSettingsEditorMaps);
        tabbar_.Item("Misc", GuiGlobal::DrawSettingsEditorMisc);
        tabbar_.Item("Tools", GuiGlobal::DrawSettingsEditorTools);
        tabbar_.End();
    }

    _DrawWindowEnd();
}

/* ****************************************************************************************************************** */

GuiWinAppHelp::GuiWinAppHelp() /* clang-format off */ :
    GuiWin("Help", { 70, 25, 0, 0 }),
    tabbar_ { WinName() }  // clang-format on
{
}

void GuiWinAppHelp::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }

    if (tabbar_.Begin()) {
        if (tabbar_.Item("Help")) {
            ImGui::BeginChild("ffguiHelp", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted("In addition to the ImGui controls:");
            ImGui::BulletText("CTRL-F4 or CTRL-W closes focused window");
            ImGui::BulletText(
                "Hold SHIFT while moving window to dock into other windows.\n"
                "(VERY experimental. WILL crash sometimes!)");
            ImGui::TextUnformatted("In (some) tables:");
            ImGui::BulletText("Click to mark a line, CTRL-click to mark multiple lines");
            ImGui::BulletText("SHIFT-click colum headers for multi-sort");
            ImGui::EndChild();
        }
        if (tabbar_.Item("ImGui")) {
            ImGui::BeginChild("ImGuiHelp", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::ShowUserGuide();
            ImGui::EndChild();
        }
        if (tabbar_.Item("ImPlot")) {
            ImGui::BeginChild("ImPlotHelp", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            ImPlot::ShowUserGuide();
            ImGui::EndChild();
        }
        if (tabbar_.Item("Streams")) {
            if (streamHelp_.empty()) {
                streamHelp_ = StrSplit(StreamHelpScreen(), "\n");
            }
            for (const auto& line : streamHelp_) {
                ImGui::TextUnformatted(line.c_str());
            }
        }
        tabbar_.End();
    }

    _DrawWindowEnd();
}

/* ****************************************************************************************************************** */

GuiWinAppLegend::GuiWinAppLegend() /* clang-format off */ :
    GuiWin("Legend", { }, ImGuiWindowFlags_AlwaysAutoResize),
    tabbar_ { WinName() }  // clang-format on
{
}

void GuiWinAppLegend::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }

    bool doFixColours = false;
    bool doSignalLevels = false;
    bool doMessages = false;
    if (tabbar_.Begin()) {
        if (tabbar_.Item("Fix colours")) {
            doFixColours = true;
        }
        if (tabbar_.Item("Signal levels")) {
            doSignalLevels = true;
        }
        if (tabbar_.Item("Messages")) {
            doMessages = true;
        }
        tabbar_.End();
    }
    if (doFixColours) {  // clang-format off
        _DrawColourLegend(C_FIX_MASKED(),       " - Masked");
        _DrawColourLegend(C_FIX_INVALID(),      "%2" PRIu8 " Invalid",                    EnumToVal(FixType::NOFIX));
        _DrawColourLegend(C_FIX_DRONLY(),       "%2" PRIu8 " Dead-reckoning only",        EnumToVal(FixType::DRONLY));
        _DrawColourLegend(C_FIX_S2D(),          "%2" PRIu8 " Single 2D",                  EnumToVal(FixType::TIME));
        _DrawColourLegend(C_FIX_S3D(),          "%2" PRIu8 " Single 3D",                  EnumToVal(FixType::SPP_2D));
        _DrawColourLegend(C_FIX_S3D_DR(),       "%2" PRIu8 " Single 3D + dead-reckoning", EnumToVal(FixType::SPP_3D));
        _DrawColourLegend(C_FIX_TIME(),         "%2" PRIu8 " Single time only",           EnumToVal(FixType::SPP_3D_DR));
        _DrawColourLegend(C_FIX_RTK_FLOAT(),    "%2" PRIu8 " RTK float",                  EnumToVal(FixType::RTK_FLOAT));
        _DrawColourLegend(C_FIX_RTK_FIXED(),    "%2" PRIu8 " RTK fixed",                  EnumToVal(FixType::RTK_FIXED));
        _DrawColourLegend(C_FIX_RTK_FLOAT_DR(), "%2" PRIu8 " RTK float + dead-reckoning", EnumToVal(FixType::RTK_FLOAT_DR));
        _DrawColourLegend(C_FIX_RTK_FIXED_DR(), "%2" PRIu8 " RTK fixed + dead-reckoning", EnumToVal(FixType::RTK_FIXED_DR));  // clang-format on
    }
    if (doSignalLevels) {  // clang-format off
        _DrawColourLegend(C_SIGNAL_00_05(), "Signal  0 -  5 dBHz");
        _DrawColourLegend(C_SIGNAL_05_10(), "Signal  5 - 10 dBHz");
        _DrawColourLegend(C_SIGNAL_10_15(), "Signal 10 - 15 dBHz");
        _DrawColourLegend(C_SIGNAL_15_20(), "Signal 15 - 20 dBHz");
        _DrawColourLegend(C_SIGNAL_20_25(), "Signal 20 - 25 dBHz");
        _DrawColourLegend(C_SIGNAL_25_30(), "Signal 25 - 30 dBHz");
        _DrawColourLegend(C_SIGNAL_30_35(), "Signal 30 - 35 dBHz");
        _DrawColourLegend(C_SIGNAL_35_40(), "Signal 35 - 40 dBHz");
        _DrawColourLegend(C_SIGNAL_40_45(), "Signal 40 - 45 dBHz");
        _DrawColourLegend(C_SIGNAL_45_50(), "Signal 45 - 50 dBHz");
        _DrawColourLegend(C_SIGNAL_50_55(), "Signal 50 - 55 dBHz");
        _DrawColourLegend(C_SIGNAL_55_OO(), "Signal 55 - oo dBHz");  // clang-format on
    }
    if (doMessages) {  // clang-format off
        _DrawColourLegend(C_LOG_MSG_UBX(),    "Log UBX messages");
        _DrawColourLegend(C_LOG_MSG_NMEA(),   "Log NMEA messages");
        _DrawColourLegend(C_LOG_MSG_FP_A(),   "Log FP_A messages");
        _DrawColourLegend(C_LOG_MSG_FP_B(),   "Log FP_B messages");
        _DrawColourLegend(C_LOG_MSG_RTCM3(),  "Log RTCM3 messages");
        _DrawColourLegend(C_LOG_MSG_NOV_B(),  "Log NOV_B messages");
        _DrawColourLegend(C_LOG_MSG_UNI_B(),  "Log UNI_B messages");
        _DrawColourLegend(C_LOG_MSG_SPARTN(), "Log SPARTN messages");
        _DrawColourLegend(C_LOG_MSG_OTHER(),  "Log OTHER messages");
        _DrawColourLegend(C_LOG_EPOCH(),      "Log Epochs");  // clang-format on
    }

    _DrawWindowEnd();
}

void GuiWinAppLegend::_DrawColourLegend(const ImU32 colour, const char* fmt, ...)
{
    ImDrawList* draw = ImGui::GetWindowDrawList();
    Vec2f offs = ImGui::GetCursorScreenPos();
    const Vec2f size{ GUI_VAR.charSizeX * 1.5f, GUI_VAR.charSizeY };
    const float textOffs = (2 * size.x) + (2 * GUI_VAR.imguiStyle->ItemSpacing.x);
    draw->AddRectFilled(offs, offs + size, colour | ((ImU32)(0xff) << IM_COL32_A_SHIFT));
    offs.x += size.x;
    draw->AddRectFilled(offs, offs + size, colour);
    ImGui::SetCursorPosX(textOffs);

    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

/* ****************************************************************************************************************** */

#ifndef IMGUI_DISABLE_DEMO_WINDOWS

GuiWinAppImguiDemo::GuiWinAppImguiDemo() /* clang-format off */ :
    GuiWin("DearImGuiDemo")  // clang-format on
{
    // SetTitle("Dear ImGui demo"); // not used, ImGui sets it
}

void GuiWinAppImguiDemo::DrawWindow()
{
    bool open = WinIsOpen();
    ImGui::ShowDemoWindow(&open);
    if (!open) {
        WinClose();
    }
}

GuiWinAppImplotDemo::GuiWinAppImplotDemo() /* clang-format off */ :
    GuiWin("ImPlotDemo")  // clang-format on
{
    // SetTitle("ImPlot demo"); // not used, ImPlot sets it
};

void GuiWinAppImplotDemo::DrawWindow()
{
    bool open = WinIsOpen();
    ImPlot::ShowDemoWindow(&open);
    if (!open) {
        WinClose();
    }
}

// GuiWinAppImplot3dDemo::GuiWinAppImplot3dDemo() /* clang-format off */ :
//     GuiWin("ImPlot3dDemo")  // clang-format on
// {
//     // SetTitle("ImPlot demo"); // not used, ImPlot sets it
// };

// void GuiWinAppImplot3dDemo::DrawWindow()
// {
//     bool open = WinIsOpen();
//     ImPlot3D::ShowDemoWindow(&open);
//     if (!open) {
//         WinClose();
//     }
// }

#endif

/* ****************************************************************************************************************** */

#ifndef IMGUI_DISABLE_DEBUG_TOOLS

GuiWinAppImguiMetrics::GuiWinAppImguiMetrics() /* clang-format off */ :
    GuiWin("DearImGuiMetrics", { 70, 40, 0, 0 })  // clang-format on
{
    // SetTitle("Dear ImGui metrics"); // not used, ImGui sets it
}

void GuiWinAppImguiMetrics::DrawWindow()
{
    bool open = WinIsOpen();
    ImGui::ShowMetricsWindow(&open);
    if (!open) {
        WinClose();
    }
}

GuiWinAppImplotMetrics::GuiWinAppImplotMetrics() /* clang-format off */ :
    GuiWin("ImPlotMetrics", { 70, 40, 0, 0 })  // clang-format on
{
    // SetTitle("ImPlot metrics"); // not used, ImPlot sets it
}

void GuiWinAppImplotMetrics::DrawWindow()
{
    bool open = WinIsOpen();
    ImPlot::ShowMetricsWindow(&open);
    if (!open) {
        WinClose();
    }
}

// GuiWinAppImplot3dMetrics::GuiWinAppImplot3dMetrics() /* clang-format off */ :
//     GuiWin("ImPlot3dMetrics", { 70, 40, 0, 0 })  // clang-format on
// {
//     // SetTitle("ImPlot metrics"); // not used, ImPlot sets it
// }

// void GuiWinAppImplot3dMetrics::DrawWindow()
// {
//     bool open = WinIsOpen();
//     ImPlot3D::ShowMetricsWindow(&open);
//     if (!open) {
//         WinClose();
//     }
// }

#endif

/* ****************************************************************************************************************** */

GuiWinAppImguiStyles::GuiWinAppImguiStyles() /* clang-format off */ :
    GuiWin("DearImGuiStyles", { 70, 40, 0, 0 })  // clang-format on
{
    WinSetTitle("Dear ImGui styles");
}

void GuiWinAppImguiStyles::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }

    ImGui::ShowStyleEditor();

    _DrawWindowEnd();
}

/* ****************************************************************************************************************** */

GuiWinAppImplotStyles::GuiWinAppImplotStyles() /* clang-format off */ :
    GuiWin("ImPlotStyles", { 70, 40, 0, 0 })  // clang-format on
{
    WinSetTitle("ImPlot styles");
}

void GuiWinAppImplotStyles::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }

    ImPlot::ShowStyleEditor();

    _DrawWindowEnd();
}

/* ****************************************************************************************************************** */

// GuiWinAppImplot3dStyles::GuiWinAppImplot3dStyles() /* clang-format off */ :
//     GuiWin("ImPlot3dStyles", { 70, 40, 0, 0 })  // clang-format on
// {
//     WinSetTitle("ImPlot styles");
// }

// void GuiWinAppImplot3dStyles::DrawWindow()
// {
//     if (!_DrawWindowBegin()) {
//         return;
//     }

//     ImPlot3D::ShowStyleEditor();

//     _DrawWindowEnd();
// }

/* ****************************************************************************************************************** */
}  // namespace ffgui
