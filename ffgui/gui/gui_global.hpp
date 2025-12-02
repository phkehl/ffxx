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

#ifndef __GUI_GLOBAL_HPP__
#define __GUI_GLOBAL_HPP__

//
#include "ffgui_inc.hpp"
//
#include <list>
#include <map>
//
// #include "ff_epoch.h"
#include <nlohmann/json.hpp>
//
#include "mapparams.hpp"
//
#include "gui_global_colours.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiGlobal;

using HistoryEntries = std::list<std::string>;

struct GuiConfig
{
    // Minimal render framerate
    static constexpr int MIN_FRAME_RATE_MIN = 5;
    static constexpr int MIN_FRAME_RATE_DEF = 15;
    static constexpr int MIN_FRAME_RATE_MAX = 100;
    int minFrameRate = MIN_FRAME_RATE_DEF;

    enum class DragLagWorkaround : int { SYNC_ALWAYS = 0, SYNC_NEVER, NOSYNC_DRAG, NOSYNC_DRAG_LIMIT };
    static constexpr DragLagWorkaround DRAG_LAG_WORKAROUND_DEF = DragLagWorkaround::NOSYNC_DRAG_LIMIT;
    DragLagWorkaround dragLagWorkaround = DRAG_LAG_WORKAROUND_DEF;

    // Font
    static constexpr float FONT_SIZE_DEF = 16.0;
    static constexpr float FONT_SIZE_MIN = 10.0;
    static constexpr float FONT_SIZE_MAX = 30.0;
    static constexpr int FONT_IX_DEF = 0;
    int fontIx = FONT_IX_DEF;
    float fontSize = FONT_SIZE_DEF;           // Font size

    static constexpr uint32_t FT_LOADER_FLAGS_DEF = (1 << 2);  // ImGuiFreeTypeLoaderFlags_ForceAutoHint
    uint32_t /*ImGuiFreeTypeLoaderFlags*/ ftLoaderFlags = FT_LOADER_FLAGS_DEF;
    static constexpr float FT_RASTERIZER_MULTIPLY_DEF = 1.0f;
    static constexpr float FT_RASTERIZER_MULTIPLY_MIN = 0.2f;
    static constexpr float FT_RASTERIZER_MULTIPLY_MAX = 2.0f;
    float ftRasterizerMultiply = FT_RASTERIZER_MULTIPLY_DEF;

    // Maps
    std::map<std::string, bool> maps;

    // Shared histories
    HistoryEntries receiverSpecHistory;  // GuiWinInputReceiver -> GuiWidgetStreamSpec
    HistoryEntries logfileHistory;       // GuiWinInputLogfile -> GuiWidgetLog
    HistoryEntries receiverLogHistory;   // GuiWinDataInf -> GuiWidgetLog
    HistoryEntries recieverInfHistory;   // GuiWinDataLog -> GuiWidgetLog
    HistoryEntries corrSpecHistory;      // GuiWinCorr -> GuiWidgetStreamSpec

    // Colours
    // clang-format off
#   define _EVAL1(_str_, _enum_, _col_) _enum_,
#   define _EVAL2(_str_, _enum_, _col_) +1
#   define _EVAL3(_str_, _enum_, _col_) _col_,
    enum class ColourIndex : int { _DUMMY = -1,  GUI_GLOBAL_COLOURS(_EVAL1) };
    static constexpr std::size_t COLOUR_COUNT = GUI_GLOBAL_COLOURS(_EVAL2);
    std::array<ImU32, COLOUR_COUNT> colours = {{ GUI_GLOBAL_COLOURS(_EVAL3) }};
#   undef _EVAL1
#   undef _EVAL2
#   undef _EVAL3
    // clang-format on

    // Window
    std::array<int, 4> windowGeometry = { { 1270, 768, -1, -1 } };
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GuiConfig, minFrameRate, dragLagWorkaround, fontSize, ftLoaderFlags,
    ftRasterizerMultiply, maps, receiverSpecHistory, logfileHistory, receiverLogHistory, recieverInfHistory, corrSpecHistory,
    windowGeometry)  // "colours" is handled separately

struct GuiCache
{
    // Paths
    std::string configDir;
    std::string cacheDir;
    std::string tilesCacheDir;
    std::string homeDir;

    // ImGui and ImPlot styles
    ImGuiStyle* imguiStyle = nullptr;
    ImPlotStyle* implotStyle = nullptr;

    // Font
    ImFont* fontRegular = nullptr;
    ImFont* fontItalic = nullptr;
    ImFont* fontBold = nullptr;

    // Sizes
    Vec2f charSize = { 0.0f, 0.0f };  // Char size (of a fontRegular character)
    float charSizeX = 0.0f;
    float charSizeY = 0.0f;
    Vec2f iconSize = { 0.0f, 0.0f };  // Size for ImGui::Button() with icon
    float widgetOffs = 0.0f;
    float lineHeight = 0.0f;  // Current line hight (default font)
    float alphaDimVal = 0.4f;

    // Colours
    std::array<ImVec4, GuiConfig::COLOUR_COUNT> colours4;  // ImVec4 versions of GuiConfig::colours
    static constexpr int CNO_COLOUR_MAX = 55;
    std::array<ImU32, CNO_COLOUR_MAX + 1> cnoColours;

    // Stuff
    Time t0;
    Duration maxFrameDur;
};

class GuiGlobal
{
   public:
    static GuiConfig config_;
    static GuiCache cache_;

    // ----- Main control (ffgui.cpp) -----

    static bool EarlyInit();
    static bool Init(const std::string& configName, const bool reset_config);
    static bool Save();
    static std::string ConfigName(const std::string& configName = "");
    static bool UpdateSizes();
    static void UpdateFonts();
    static void RestoreFocusOrder();

    // ----- Settings editor (gui_win_app.cpp) -----

    static void DrawSettingsEditorFont();
    static void DrawSettingsEditorColours();
    static void DrawSettingsEditorMaps();
    static void DrawSettingsEditorMisc();
    static void DrawSettingsEditorTools();

    // ----- User functions -----

    static ImU32 FixColour(const FixType fix, const bool fixok = true);
    static const ImVec4& FixColour4(const FixType fix, const bool fixok = true);

    // Save a state/config/whatever object

    template <typename ObjT>
    static bool SaveObj(const std::string& name, const ObjT& obj) {
        persistentData_[name] = obj;
        TRACE("GuiGlobal::SaveObj %s %s", name.c_str(), persistentData_[name].dump().c_str());
        return true;
    }

    template <typename ObjT>
    static bool LoadObj(const std::string& name, ObjT& obj) {
        bool ok = false;
        if (persistentData_.contains(name) && persistentData_[name].is_object()) {
            TRACE("GuiGlobal::LoadObj %s %s", name.c_str(), persistentData_[name].dump().c_str());
            try {
                obj = persistentData_[name].template get<ObjT>();
            } catch (std::exception& ex) {
                WARNING("%s fail: %s", __PRETTY_FUNCTION__, ex.what());
            }
            ok = true;
        }
        return ok;
    }

   private:
    static std::string configName_;
    static bool clearConfOnSave_;
    static bool fontDirty_;
    static bool sizesDirty_;
    static nlohmann::json persistentData_;
};

// Shortcuts for config and cache
#define GUI_CFG GuiGlobal::config_
#define GUI_VAR GuiGlobal::cache_

// Shortcuts/helpers for colours
// - Colours: for example, C_GREEN(), C4_GREEN()
// clang-format off
static constexpr ImU32 COL_DEBUG_INFO() { return GUI_CFG.colours[EnumToVal(GuiConfig::ColourIndex::DEBUG_INFO)]; }
#define _EVAL1(_str_, _enum_, _col_) static constexpr ImU32& CONCAT(C_, _enum_)() { return GUI_CFG.colours[EnumToVal(GuiConfig::ColourIndex::_enum_)]; };
#define _EVAL2(_str_, _enum_, _col_) static constexpr ImVec4& CONCAT(C4_, _enum_)() { return GUI_VAR.colours4[EnumToVal(GuiConfig::ColourIndex::_enum_)]; };
#define _EVAL3(_str_, _enum_, _col_) static constexpr std::size_t CONCAT(CIX_, _enum_) = EnumToVal(GuiConfig::ColourIndex::_enum_);
GUI_GLOBAL_COLOURS(_EVAL1)
GUI_GLOBAL_COLOURS(_EVAL2)
GUI_GLOBAL_COLOURS(_EVAL3)
#undef _EVAL1
#undef _EVAL2
#undef _EVAL3
// Special uses
#define C_AUTO()       IM_COL32(0x00, 0x00, 0x00, 0x00)
static constexpr std::size_t CIX_AUTO = std::numeric_limits<std::size_t>::max();
static constexpr ImU32 CNO_COLOUR(const float cno) { return GuiGlobal::cache_.cnoColours[std::clamp((int)cno, 0, GuiCache::CNO_COLOUR_MAX)]; }
inline ImU32 FIX_COLOUR(const FixType fix, const bool fixok = true) { return GuiGlobal::FixColour(fix, fixok); }
inline const ImVec4& FIX_COLOUR4(const FixType fix, const bool fixok = true) { return GuiGlobal::FixColour4(fix, fixok); }
inline ImU32 IX_COLOUR(const std::size_t ix) { return ix <GUI_CFG.colours.size() ? GUI_CFG.colours[ix] : C_AUTO(); }
// clang-format on

// Helpers for fonts
// clang-format off
inline void GuiPushFontRegular() { ImGui::PushFont(GUI_VAR.fontRegular, GUI_CFG.fontSize); }
inline void GuiPushFontItalic()  { ImGui::PushFont(GUI_VAR.fontItalic,  GUI_CFG.fontSize); }
inline void GuiPushFontBold()    { ImGui::PushFont(GUI_VAR.fontBold,    GUI_CFG.fontSize); }
inline void GuiPopFont()         { ImGui::PopFont(); }
class GuiScopedFontRegular { public: GuiScopedFontRegular() { GuiPushFontRegular(); } ~GuiScopedFontRegular() { GuiPopFont(); } };
class GuiScopedFontItalic  { public: GuiScopedFontItalic()  { GuiPushFontItalic();  } ~GuiScopedFontItalic()  { GuiPopFont(); } };
class GuiScopedFontBold    { public: GuiScopedFontBold()    { GuiPushFontBold();    } ~GuiScopedFontBold()    { GuiPopFont(); } };
// clang-format on

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_GLOBAL_HPP__
