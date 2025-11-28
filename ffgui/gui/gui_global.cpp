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
#include <cerrno>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
//
#include <fpsdk_common/math.hpp>
#include <fpsdk_common/path.hpp>
using namespace fpsdk::common::math;
using namespace fpsdk::common::path;
//
#include "imgui_internal.h"
#include "implot_internal.h"
#include "imgui_freetype.h"
#include "platform_folders.hpp"
//
//#include "database.hpp"
//
#include "gui_fonts.hpp"
//
#include "gui_global.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

/*static*/ std::string GuiGlobal::configName_ = "default";
/*static*/ GuiConfig GuiGlobal::config_;
/*static*/ GuiCache GuiGlobal::cache_;
/*static*/ bool GuiGlobal::clearConfOnSave_ = false;
/*static*/ bool GuiGlobal::fontDirty_ = true;
/*static*/ bool GuiGlobal::sizesDirty_ = true;
/*static*/ nlohmann::json GuiGlobal::persistentData_ = nlohmann::json::object();

// clang-format off
#define _EVAL1(_str_, _enum_, _col_) _str_,
#define _EVAL2(_str_, _enum_, _col_) _col_,
#define _EVAL3(_str_, _enum_, _col_) STRINGIFY(_enum_),
static constexpr std::array<const char*, GuiConfig::COLOUR_COUNT> COLOUR_NAMES = {{ GUI_GLOBAL_COLOURS(_EVAL1) }};
static constexpr std::array<ImU32, GuiConfig::COLOUR_COUNT> COLOUR_DEFAULTS = {{ GUI_GLOBAL_COLOURS(_EVAL2) }};
static constexpr std::array<const char*, GuiConfig::COLOUR_COUNT> COLOUR_ENUMS = {{ GUI_GLOBAL_COLOURS(_EVAL3) }};
#undef _EVAL1
#undef _EVAL2
// clang-format on

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ std::string GuiGlobal::ConfigName(const std::string& configName)
{
    if (!configName.empty()) {
        configName_ = configName;
    }
    return configName_;
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ bool GuiGlobal::EarlyInit()
{
    DEBUG("GuiGlobal::EarlyInit()");
    cache_.t0.SetClockRealtime();

    // Files and directories
    try {
        cache_.configDir = sago::getConfigHome() + "/ffgui";  // ~/.config/ffgui
        cache_.cacheDir = sago::getCacheDir() + "/ffgui";     // ~/.cache/ffgui
        cache_.tilesCacheDir = cache_.cacheDir + "/tiles";     // ~/.cache/ffgui/tiles
        cache_.homeDir = sago::getUserHome();
        if (!std::filesystem::exists(cache_.configDir)) {
            std::filesystem::create_directories(cache_.configDir);
        }
        if (!std::filesystem::exists(cache_.tilesCacheDir)) {
            std::filesystem::create_directories(cache_.tilesCacheDir);
        }
        DEBUG("configDir:  %s", cache_.configDir.c_str());
        DEBUG("cacheDir:   %s", cache_.cacheDir.c_str());
        DEBUG("homeDir:    %s", cache_.homeDir.c_str());
    } catch (std::exception& ex) {
        WARNING("Could not work out platform directories: %s", ex.what());
        return false;
    }

    return true;
}

/*static*/ bool GuiGlobal::Init(const std::string& configName, const bool reset_config)
{

    configName_ = configName;
    DEBUG("GuiGlobal::Init(%s)", configName_.c_str());

    // Configure ImGui

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // io.UserData = NULL;
    io.IniFilename = NULL;  // Don't auto-load, we do it ourselves in LoadConf() below
    // io.FontScaleMain = 1.0; // TODO: expose as setting?
    // io.FontScaleDpi = 1.0; // TODO: expose as setting?
    io.ConfigDockingWithShift = true;
    // io.ConfigViewportsNoAutoMerge   = true;
    // io.ConfigViewportsNoTaskBarIcon = true;
    io.ConfigWindowsResizeFromEdges = true;
    // io.MouseDrawCursor = true;

    ImGui::SetColorEditOptions(ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayHSV |
                               ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar |
                               ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);

    // Select ImGui and ImPlot styles
    ImGui::StyleColorsDark();
    ImPlot::StyleColorsAuto();

    // Tune ImGui styles
    ImGuiStyle* imguiStyle = &(ImGui::GetCurrentContext()->Style);
    imguiStyle->Colors[ImGuiCol_PopupBg] = ImColor(IM_COL32(0x34, 0x34, 0x34, 0xf8));
    imguiStyle->Colors[ImGuiCol_TitleBg] = ImVec4(0.232f, 0.201f, 0.271f, 1.00f);
    imguiStyle->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.502f, 0.075f, 0.256f, 1.00f);
    imguiStyle->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.200f, 0.220f, 0.270f, 0.75f);
    imguiStyle->Colors[ImGuiCol_TabSelected] = ImVec4(0.502f, 0.075f, 0.256f, 1.00f);
    imguiStyle->FrameBorderSize = 1.0f;
    imguiStyle->TabBorderSize = 1.0f;
    imguiStyle->WindowTitleAlign.x = 0.0f;
    imguiStyle->WindowMenuButtonPosition = ImGuiDir_Left;
    imguiStyle->FrameRounding = 3.0f;
    // imguiStyle->WindowBorderSize = 5.0f;
    imguiStyle->WindowRounding = 5.0f;
    imguiStyle->WindowBorderHoverPadding = 8.0f;

    // Tune ImPlot styles
    ImPlotStyle* implotStyle = &(ImPlot::GetCurrentContext()->Style);
    implotStyle->LineWeight = 2.0;
    // implotStyle->AntiAliasedLines      = true; // TODO: enable MSAA (how?!), see ImGui FAQ
    implotStyle->UseLocalTime = false;
    implotStyle->UseISO8601 = true;
    implotStyle->Use24HourClock = true;
    implotStyle->FitPadding = ImVec2(0.1, 0.1);
    // implotStyle->PlotPadding             = ImVec2(0, 0);
    implotStyle->Colors[ImPlotCol_AxisBgHovered] = ImVec4(0.060f, 0.530f, 0.980f, 0.200f);
    implotStyle->Colors[ImPlotCol_AxisBgActive] = ImVec4(0.060f, 0.530f, 0.980f, 0.400f);

    // Store style shortcuts
    cache_.imguiStyle = imguiStyle;
    cache_.implotStyle = implotStyle;

    // Load config file
    const std::string configFile = cache_.configDir + "/" + configName_ + ".json";
    DEBUG("configFile: %s", configFile.c_str());
    if (!reset_config) {
        INFO("Loading config '%s'", configName_.c_str());
        std::vector<uint8_t> configData;
        if (PathExists(configFile) && FileSlurp(configFile, configData)) {
            try {
                persistentData_ = nlohmann::json::parse(configData);
                if (persistentData_.contains("DearImGuiIni") && persistentData_["DearImGuiIni"].is_array()) {
                    ImGui::LoadIniSettingsFromMemory(
                        StrJoin(persistentData_["DearImGuiIni"].get<std::vector<std::string>>(), "\n").c_str());
                }
            } catch (std::exception& ex) {
                WARNING("Failed loading %s: %s", configName_.c_str(), ex.what());
            }
        }
    } else {
        WARNING("Reset config '%s'", configName_.c_str());
        FileSpew(configFile, StrToBuf("{}"));
    }

    // Restore config data
    LoadObj("GuiConfig", config_);

    // Load colours
    if (persistentData_.contains("GuiConfig") && persistentData_["GuiConfig"].is_object() &&
        persistentData_["GuiConfig"].contains("colours") && persistentData_["GuiConfig"]["colours"].is_array()) {
        for (const auto& c : persistentData_["GuiConfig"]["colours"]) {
            if (c.is_object() && c.contains("name") && c["name"].is_string() && c.contains("rgba") &&
                c["rgba"].is_array() && (c["rgba"].size() == 4) && c["rgba"][0].is_number() &&
                c["rgba"][1].is_number() && c["rgba"][2].is_number() && c["rgba"][3].is_number()) {
                const std::string name = c["name"].get<std::string>();
                auto entry = std::find(COLOUR_ENUMS.begin(), COLOUR_ENUMS.end(), name);
                if (entry != COLOUR_ENUMS.end()) {
                    const auto ix = std::distance(COLOUR_ENUMS.begin(), entry);
                    config_.colours[ix] = ((c["rgba"][0].get<int>() & 0xff) << IM_COL32_R_SHIFT) |
                                          ((c["rgba"][1].get<int>() & 0xff) << IM_COL32_G_SHIFT) |
                                          ((c["rgba"][2].get<int>() & 0xff) << IM_COL32_B_SHIFT) |
                                          ((c["rgba"][3].get<int>() & 0xff) << IM_COL32_A_SHIFT);
                }
            }
        }
    }

    // Enforce config constraints
    config_.fontSize = std::clamp(config_.fontSize, config_.FONT_SIZE_MIN, config_.FONT_SIZE_MAX);
    config_.ftRasterizerMultiply = std::clamp(
        config_.ftRasterizerMultiply, config_.FT_RASTERIZER_MULTIPLY_MIN, config_.FT_RASTERIZER_MULTIPLY_MAX);
    config_.minFrameRate = std::clamp(config_.minFrameRate, config_.MIN_FRAME_RATE_MIN, config_.MIN_FRAME_RATE_MAX);

    // Update maps enable/disable config
    std::map<std::string, bool> maps;
    for (auto& map : MapParams::BUILTIN_MAPS) {
        const auto entry = config_.maps.find(map.name);
        maps.emplace(map.name, entry != config_.maps.end() ? entry->second : map.enabled);
    }
    config_.maps = maps;

    // Sync ImU32 colours to ImVec4 colours
    for (std::size_t ix = 0; ix < config_.colours.size(); ix++) {
        cache_.colours4[ix] = ImGui::ColorConvertU32ToFloat4(config_.colours[ix]);
    }

    // Calculate CNo colours

    const int cnoColoursCount =
        EnumToVal(GuiConfig::ColourIndex::SIGNAL_55_OO) - EnumToVal(GuiConfig::ColourIndex::SIGNAL_00_05) + 1;
    ImPlotColormap cnoColourMap = ImPlot::AddColormap(
        "CnoColours", &GuiGlobal::config_.colours[EnumToVal(GuiConfig::ColourIndex::SIGNAL_00_05)], cnoColoursCount, false);
    for (int cno = 0; cno <= GuiCache::CNO_COLOUR_MAX; cno++) {
        const float t = (float)cno / (float)GuiCache::CNO_COLOUR_MAX;
        cache_.cnoColours[cno] = ImGui::ColorConvertFloat4ToU32(ImPlot::SampleColormap(t, cnoColourMap));
    }

    cache_.maxFrameDur.SetSec(1.0 / config_.minFrameRate);

    return true;
}

/*static*/ void GuiGlobal::RestoreFocusOrder()
{
    // ImGuiContext& g = *GImGui;
    // for (const auto& win : g.WindowsFocusOrder) {
    //     DEBUG("FocusOrder %08" PRIx32 " %s", win->ID, win->Name);
    // }

    if (!persistentData_.contains("FocusOrder") || !persistentData_["FocusOrder"].is_array()) {
        return;
    }
    for (const auto& entry : persistentData_["FocusOrder"]) {
        if (!entry.is_number()) {
            continue;
        }
        const auto id = entry.get<ImGuiID>();
        auto win = ImGui::FindWindowByID(id);
        if (win) {
            DEBUG("Focus %" PRIu32 " %s", win->ID, win->Name);
            ImGui::FocusWindow(win);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ bool GuiGlobal::Save()
{
    const std::string configFile = cache_.configDir + "/" + configName_ + ".json";
    DEBUG("configFile: %s", configFile.c_str());

    if (clearConfOnSave_) {
        WARNING("Reset config '%s'", configName_.c_str());
        FileSpew(configFile, StrToBuf("{}"));
        return true;
    }

    INFO("Saving config '%s'", configName_.c_str());

    SaveObj("GuiConfig", config_);
    persistentData_["DearImGuiIni"] = StrSplit(ImGui::SaveIniSettingsToMemory(), "\n");

    // Translate colours into something more friendly
    auto colours = nlohmann::json::array();
    for (std::size_t ix = 0; ix < GuiConfig::COLOUR_COUNT; ix++) {
        auto c = nlohmann::json::array({
            (config_.colours[ix] >> IM_COL32_R_SHIFT) & 0xff,
            (config_.colours[ix] >> IM_COL32_G_SHIFT) & 0xff,
            (config_.colours[ix] >> IM_COL32_B_SHIFT) & 0xff,
            (config_.colours[ix] >> IM_COL32_A_SHIFT) & 0xff,
        });
        colours.push_back(nlohmann::json::object({ { "name", std::string(COLOUR_ENUMS[ix]) }, { "rgba", c } }));
    }
    persistentData_["GuiConfig"]["colours"] = colours;

    ImGuiContext& g = *GImGui;
    std::vector<ImGuiID> focusOrder;
    for (const auto& win : g.WindowsFocusOrder) {
        // DEBUG("FocusOrder %08" PRIx32 " %s", win->ID, win->Name);
        focusOrder.push_back(win->ID);
    }
    persistentData_["FocusOrder"] = focusOrder;

    return FileSpew(configFile, StrToBuf(persistentData_.dump(LoggingIsLevel(LoggingLevel::DEBUG) ? 4 : 0).c_str()));
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ bool GuiGlobal::UpdateSizes()
{
    if (!sizesDirty_) {
        return false;
    }
    DEBUG("GuiGlobal::UpdateSizes()");
    const float frameHeight = ImGui::GetFrameHeight();
    cache_.iconSize = ImVec2(frameHeight, frameHeight);
    cache_.charSize = ImGui::CalcTextSize("X");
    cache_.charSizeX = cache_.charSize.x;
    cache_.charSizeY = cache_.charSize.y;
    cache_.lineHeight = ImGui::GetTextLineHeightWithSpacing();
    cache_.widgetOffs = 25.0f * cache_.charSize.x;
    cache_.alphaDimVal = cache_.imguiStyle->Alpha * 0.4f;

    sizesDirty_ = false;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ void GuiGlobal::UpdateFonts()
{
    if (!fontDirty_) {
        return;
    }
    DEBUG("GuiGlobal::UpdateFonts()");

    ImGuiIO& io = ImGui::GetIO();

    // Clear all font data
    io.Fonts->Clear();

    FontData fontData = GetFontData(config_.fontIx);

    // Normal font
    {
        ImFontConfig fontConfig;
        std::snprintf(fontConfig.Name, sizeof(fontConfig.Name), "DejaVu Mono");
        fontConfig.FontLoaderFlags = config_.ftLoaderFlags;
        fontConfig.RasterizerMultiply = config_.ftRasterizerMultiply;
        cache_.fontRegular =
        // io.Fonts->AddFontFromMemoryCompressedBase85TTF(guiGetFontDejaVuSansMono(), config_.fontSize, &fontConfig);
            io.Fonts->AddFontFromMemoryCompressedBase85TTF(fontData.dataRegular, config_.fontSize, &fontConfig);

            fontConfig.MergeMode = true;
        // fontConfig.GlyphMinAdvanceX = 13.0f; // Use if you want to make the icon monospaced
        io.Fonts->AddFontFromMemoryCompressedBase85TTF(GetFontIcons(), 0.0f, &fontConfig);
    }

    // Italic font
    {
        ImFontConfig fontConfig;
        std::snprintf(fontConfig.Name, sizeof(fontConfig.Name), "DejaVu Sans");
        fontConfig.FontLoaderFlags = config_.ftLoaderFlags;
        fontConfig.RasterizerMultiply = config_.ftRasterizerMultiply;
        cache_.fontItalic = io.Fonts->AddFontFromMemoryCompressedBase85TTF(fontData.dataItalic, 0.0f, &fontConfig);
    }

    // Bold font
    {
        ImFontConfig fontConfig;
        std::snprintf(fontConfig.Name, sizeof(fontConfig.Name), "DejaVu Sans Bold");
        fontConfig.FontLoaderFlags = config_.ftLoaderFlags;
        fontConfig.RasterizerMultiply = config_.ftRasterizerMultiply;
        cache_.fontBold = io.Fonts->AddFontFromMemoryCompressedBase85TTF(fontData.dataBold, 0.0f, &fontConfig);
    }

    fontDirty_ = false;
    sizesDirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ ImU32 GuiGlobal::FixColour(const FixType fix, const bool fixok)
{
    if (!fixok) {
        return C_FIX_MASKED();
    }
    switch (fix) {  // clang-format off
        case FixType::UNKNOWN:
        case FixType::NOFIX:         return C_FIX_INVALID();
        case FixType::DRONLY:        return C_FIX_DRONLY();
        case FixType::TIME:          return C_FIX_S2D();
        case FixType::SPP_2D:        return C_FIX_S3D();
        case FixType::SPP_3D:        return C_FIX_S3D_DR();
        case FixType::SPP_3D_DR:     return C_FIX_TIME();
        case FixType::RTK_FLOAT:     return C_FIX_RTK_FLOAT();
        case FixType::RTK_FIXED:     return C_FIX_RTK_FIXED();
        case FixType::RTK_FLOAT_DR:  return C_FIX_RTK_FLOAT_DR();
        case FixType::RTK_FIXED_DR:  return C_FIX_RTK_FIXED_DR();
    }  // clang-format on
    return C_FIX_INVALID();
}

/*static*/ const ImVec4& GuiGlobal::FixColour4(const FixType fix, const bool fixok)
{
    if (!fixok) {
        return C4_FIX_MASKED();
    }
    switch (fix) {  // clang-format off
        case FixType::UNKNOWN:
        case FixType::NOFIX:         return C4_FIX_INVALID();
        case FixType::DRONLY:        return C4_FIX_DRONLY();
        case FixType::TIME:          return C4_FIX_S2D();
        case FixType::SPP_2D:        return C4_FIX_S3D();
        case FixType::SPP_3D:        return C4_FIX_S3D_DR();
        case FixType::SPP_3D_DR:     return C4_FIX_TIME();
        case FixType::RTK_FLOAT:     return C4_FIX_RTK_FLOAT();
        case FixType::RTK_FIXED:     return C4_FIX_RTK_FIXED();
        case FixType::RTK_FLOAT_DR:  return C4_FIX_RTK_FLOAT_DR();
        case FixType::RTK_FIXED_DR:  return C4_FIX_RTK_FIXED_DR();
    }  // clang-format on
    return C4_FIX_INVALID();
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ void GuiGlobal::DrawSettingsEditorFont()
{
    ImGui::AlignTextToFramePadding();

    ImGui::TextUnformatted("Font");
    ImGui::SameLine(cache_.widgetOffs);
    {
        ImGui::BeginDisabled(config_.fontIx == GuiConfig::FONT_IX_DEF);
        if (ImGui::Button(config_.fontIx != GuiConfig::FONT_IX_DEF ? ICON_FK_TIMES "##IxDef" : "##IxDef", cache_.iconSize)) {
            config_.fontIx = GuiConfig::FONT_IX_DEF;
            fontDirty_ = true;
        }
        Gui::ItemTooltip("Reset to default");
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(12.0f * cache_.charSize.x);
    if (ImGui::Combo("##font", &config_.fontIx, FONT_NAMES.data(), FONT_NAMES.size())) {
        fontDirty_ = true;
    }


    ImGui::TextUnformatted("Font size");
    ImGui::SameLine(cache_.widgetOffs);
    {
        ImGui::BeginDisabled(config_.fontSize == GuiConfig::FONT_SIZE_DEF);
        if (ImGui::Button(config_.fontSize != GuiConfig::FONT_SIZE_DEF ? ICON_FK_TIMES "##FsDef" : "##FsDef", cache_.iconSize)) {
            config_.fontSize = GuiConfig::FONT_SIZE_DEF;
            fontDirty_ = true;
        }
        Gui::ItemTooltip("Reset to default");
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(12.0f * cache_.charSize.x);
    float newFontSize = config_.fontSize;
    if (ImGui::InputFloat("##FontSize", &newFontSize, 1.0, 1.0, "%.1f" /*, ImGuiInputTextFlags_EnterReturnsTrue*/)) {
        if (std::fabs(newFontSize - config_.fontSize) > 0.05) {
            newFontSize = std::floor(newFontSize * 10.0f) / 10.0f;
            config_.fontSize = std::clamp(newFontSize, GuiConfig::FONT_SIZE_MIN, GuiConfig::FONT_SIZE_MAX);
            fontDirty_ = true;  // trigger change in main()
        }
    }

    ImGui::Separator();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Font renderer");
    ImGui::SameLine(cache_.widgetOffs);
    {
        ImGui::BeginDisabled(config_.ftLoaderFlags == GuiConfig::FT_LOADER_FLAGS_DEF);
        if (ImGui::Button(config_.ftLoaderFlags != GuiConfig::FT_LOADER_FLAGS_DEF ? ICON_FK_TIMES "##LF" : "##LF", cache_.iconSize)) {
            config_.ftLoaderFlags = GuiConfig::FT_LOADER_FLAGS_DEF;
            fontDirty_ = true;
        }
        Gui::ItemTooltip("Reset to default");
        ImGui::EndDisabled();
    }

    const float offs0 = cache_.iconSize.x + cache_.imguiStyle->ItemSpacing.x;
    const float offs1 = cache_.charSize.x * 16.0f;

    ImGui::SameLine(cache_.widgetOffs + offs0);
    if (ImGui::CheckboxFlags("NoHinting", &config_.ftLoaderFlags, ImGuiFreeTypeLoaderFlags_NoHinting)) {
        fontDirty_ = true;
    }
    ImGui::SameLine(cache_.widgetOffs + offs0 + offs1);
    if (ImGui::CheckboxFlags("NoAutoHint", &config_.ftLoaderFlags, ImGuiFreeTypeLoaderFlags_NoAutoHint)) {
        fontDirty_ = true;
    }
    ImGui::SameLine(cache_.widgetOffs + offs0 + offs1 + offs1);
    if (ImGui::CheckboxFlags("ForceAutoHint", &config_.ftLoaderFlags, ImGuiFreeTypeLoaderFlags_ForceAutoHint)) {
        fontDirty_ = true;
    }
    ImGui::NewLine();
    ImGui::SameLine(cache_.widgetOffs + offs0);
    if (ImGui::CheckboxFlags("LightHinting", &config_.ftLoaderFlags, ImGuiFreeTypeLoaderFlags_LightHinting)) {
        fontDirty_ = true;
    }
    ImGui::SameLine(cache_.widgetOffs + offs0 + offs1);
    if (ImGui::CheckboxFlags("MonoHinting", &config_.ftLoaderFlags, ImGuiFreeTypeLoaderFlags_MonoHinting)) {
        fontDirty_ = true;
    }
    // if (ImGui::CheckboxFlags("Bold", &config_.ftLoaderFlags, ImGuiFreeTypeLoaderFlags_Bold))
    // {
    //     fontDirty_ = true;
    // }
    // if (ImGui::CheckboxFlags("Oblique", &config_.ftLoaderFlags, ImGuiFreeTypeLoaderFlags_Oblique))
    // {
    //     fontDirty_ = true;
    // }
    ImGui::SameLine(cache_.widgetOffs + offs0 + offs1 + offs1);
    if (ImGui::CheckboxFlags("Monochrome", &config_.ftLoaderFlags, ImGuiFreeTypeLoaderFlags_Monochrome)) {
        fontDirty_ = true;
    }

    ImGui::Separator();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Rasterizer multiply");
    ImGui::SameLine(cache_.widgetOffs);
    {
        ImGui::BeginDisabled(config_.ftRasterizerMultiply == GuiConfig::FT_RASTERIZER_MULTIPLY_DEF);
        if (ImGui::Button(config_.ftRasterizerMultiply != GuiConfig::FT_RASTERIZER_MULTIPLY_DEF ? ICON_FK_TIMES "##RM" : "##RM", cache_.iconSize)) {
            config_.ftRasterizerMultiply = GuiConfig::FT_RASTERIZER_MULTIPLY_DEF;
            fontDirty_ = true;
        }
        Gui::ItemTooltip("Reset to default");
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(12.0f * cache_.charSize.x);
    float newMultiply = config_.ftRasterizerMultiply;
    if (ImGui::DragFloat("##RasterizerMultiply", &newMultiply, 0.01f, GuiConfig::FT_RASTERIZER_MULTIPLY_MIN,
            GuiConfig::FT_RASTERIZER_MULTIPLY_MAX, "%.2f")) {
        if (std::fabs(newMultiply - config_.ftRasterizerMultiply) > 0.01) {
            newMultiply = std::floor(newMultiply * 100.0f) / 100.0f;
            config_.ftRasterizerMultiply =
                std::clamp(newMultiply, GuiConfig::FT_RASTERIZER_MULTIPLY_MIN, GuiConfig::FT_RASTERIZER_MULTIPLY_MAX);
            fontDirty_ = true;
        }
    }

    ImGui::Separator();

    ImGui::TextUnformatted("Regular");
    ImGui::SameLine(cache_.widgetOffs);
    ImGui::TextUnformatted(cache_.fontRegular->GetDebugName());
    ImGui::NewLine();
    ImGui::SameLine(cache_.widgetOffs);
    ImGui::TextUnformatted("abcdefghijklmnopqrstuvwxyz");
    ImGui::NewLine();
    ImGui::SameLine(cache_.widgetOffs);
    ImGui::TextUnformatted("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    ImGui::NewLine();
    ImGui::SameLine(cache_.widgetOffs);
    ImGui::TextUnformatted("0123456789+*%&/()=?{}[];,:.-_<>");

    ImGui::TextUnformatted("Italic");
    ImGui::SameLine(cache_.widgetOffs);
    {
        GuiScopedFontItalic font;
        ImGui::TextUnformatted(cache_.fontItalic->GetDebugName());
        ImGui::NewLine();
        ImGui::SameLine(cache_.widgetOffs);
        ImGui::TextUnformatted("abcdefghijklmnopqrstuvwxyz");
        ImGui::NewLine();
        ImGui::SameLine(cache_.widgetOffs);
        ImGui::TextUnformatted("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        ImGui::NewLine();
        ImGui::SameLine(cache_.widgetOffs);
        ImGui::TextUnformatted("0123456789+*%&/()=?{}[];,:.-_<>");
    }

    ImGui::TextUnformatted("Bold font");
    ImGui::SameLine(cache_.widgetOffs);
    {
        GuiScopedFontBold font;
        ImGui::TextUnformatted(cache_.fontBold->GetDebugName());
        ImGui::NewLine();
        ImGui::SameLine(cache_.widgetOffs);
        ImGui::TextUnformatted("abcdefghijklmnopqrstuvwxyz");
        ImGui::NewLine();
        ImGui::SameLine(cache_.widgetOffs);
        ImGui::TextUnformatted("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        ImGui::NewLine();
        ImGui::SameLine(cache_.widgetOffs);
        ImGui::TextUnformatted("0123456789+*%&/()=?{}[];,:.-_<>");
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Comparison");
    ImGui::SameLine(cache_.widgetOffs);
    ImGui::TextUnformatted("ABCDE");
    ImGui::SameLine(0.0f);
    {
        GuiScopedFontItalic font;
        ImGui::TextUnformatted("ABCDE");
    }
    ImGui::SameLine(0.0f);
    {
        GuiScopedFontBold font;
        ImGui::TextUnformatted("ABCDE");
    }
    ImGui::Separator();
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ void GuiGlobal::DrawSettingsEditorColours()
{
    if (ImGui::BeginChild("Colours")) {
        for (std::size_t ix = 0; ix < GuiConfig::COLOUR_COUNT; ix++) {
            if (ix != 0) {
                ImGui::Separator();
            }

            ImGui::PushID(COLOUR_NAMES[ix]);
            {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(COLOUR_NAMES[ix]);
            }
            ImGui::SameLine(cache_.widgetOffs);
            {
                ImGui::BeginDisabled(config_.colours[ix] == COLOUR_DEFAULTS[ix]);
                if (ImGui::Button(config_.colours[ix] != COLOUR_DEFAULTS[ix] ? ICON_FK_TIMES : " ", cache_.iconSize)) {
                    config_.colours[ix] = COLOUR_DEFAULTS[ix];
                    cache_.colours4[ix] = ImGui::ColorConvertU32ToFloat4(COLOUR_DEFAULTS[ix]);
                }
                Gui::ItemTooltip("Reset to default");
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            {
                ImGui::SetNextItemWidth(cache_.charSize.x * 40.0f);
                if (ImGui::ColorEdit4("##ColourEdit", &cache_.colours4[ix].x, ImGuiColorEditFlags_AlphaPreviewHalf)) {
                    config_.colours[ix] = ImGui::ColorConvertFloat4ToU32(cache_.colours4[ix]);
                }
            }

            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ void GuiGlobal::DrawSettingsEditorMaps()
{
    if (ImGui::BeginChild("Maps")) {
        for (auto& map : MapParams::BUILTIN_MAPS) {
            auto entry = config_.maps.find(map.name);
            if (entry != config_.maps.end()) {
                char str[100];
                std::snprintf(str, sizeof(str), "%s (%s)##%p", map.title.c_str(), map.name.c_str(), &map);
                ImGui::Checkbox(str, &entry->second);
                ImGui::Separator();
            }
        }
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ void GuiGlobal::DrawSettingsEditorMisc()
{
    ImGui::TextUnformatted("Min framerate");
    ImGui::SameLine(cache_.widgetOffs);
    ImGui::BeginDisabled(config_.minFrameRate == GuiConfig::MIN_FRAME_RATE_DEF);
    if (ImGui::Button(config_.minFrameRate != GuiConfig::MIN_FRAME_RATE_DEF ? ICON_FK_TIMES "###MinFramerate": " ###MinFramerate", cache_.iconSize)) {
        config_.minFrameRate = GuiConfig::MIN_FRAME_RATE_DEF;
        cache_.maxFrameDur.SetSec(1.0 / config_.minFrameRate);
    }
    Gui::ItemTooltip("Reset to default");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(cache_.widgetOffs);
    if (ImGui::SliderInt(
            "##minFrameRate", &config_.minFrameRate, GuiConfig::MIN_FRAME_RATE_MIN, GuiConfig::MIN_FRAME_RATE_MAX)) {
        config_.minFrameRate =
            std::clamp(config_.minFrameRate, GuiConfig::MIN_FRAME_RATE_MIN, GuiConfig::MIN_FRAME_RATE_MAX);
            cache_.maxFrameDur.SetSec(1.0 / config_.minFrameRate);
    }

    ImGui::TextUnformatted("Drag lag workaround");
    ImGui::SameLine();
    ImGui::SameLine(cache_.widgetOffs);
    ImGui::BeginDisabled(config_.dragLagWorkaround == GuiConfig::DRAG_LAG_WORKAROUND_DEF);  // clang-format off
    if (ImGui::Button(config_.dragLagWorkaround != GuiConfig::DRAG_LAG_WORKAROUND_DEF ? ICON_FK_TIMES "###DragLagWorkaround" : " ###DragLagWorkaround", cache_.iconSize)) {  // clang-format on
        config_.dragLagWorkaround = GuiConfig::DRAG_LAG_WORKAROUND_DEF;
    }
    Gui::ItemTooltip("Reset to default");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(cache_.widgetOffs);
    static constexpr std::array<const char*, 4> items = { "SYNC_ALWAYS", "SYNC_NEVER", "NOSYNC_DRAG", "NOSYNC_DRAG_LIMIT" };
    int dragLagWorkaround = EnumToVal(config_.dragLagWorkaround);
    if (ImGui::Combo("##dragLagWorkaround", &dragLagWorkaround, items.data(), items.size())) {
        switch ((GuiConfig::DragLagWorkaround)dragLagWorkaround) { // clang-format off
            case GuiConfig::DragLagWorkaround::SYNC_ALWAYS:        config_.dragLagWorkaround = GuiConfig::DragLagWorkaround::SYNC_ALWAYS;       break;
            case GuiConfig::DragLagWorkaround::SYNC_NEVER:         config_.dragLagWorkaround = GuiConfig::DragLagWorkaround::SYNC_NEVER;        break;
            case GuiConfig::DragLagWorkaround::NOSYNC_DRAG:        config_.dragLagWorkaround = GuiConfig::DragLagWorkaround::NOSYNC_DRAG;       break;
            case GuiConfig::DragLagWorkaround::NOSYNC_DRAG_LIMIT:  config_.dragLagWorkaround = GuiConfig::DragLagWorkaround::NOSYNC_DRAG_LIMIT; break;
        }  // clang-format on
    }
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ void GuiGlobal::DrawSettingsEditorTools()
{
    ImGui::TextUnformatted("Config name");
    ImGui::SameLine(cache_.widgetOffs);
    ImGui::SetNextItemWidth(cache_.widgetOffs);
    ImGui::InputText("##ConfigName", &configName_);

    ImGui::TextUnformatted("Clear config on exit");
    ImGui::SameLine(cache_.widgetOffs);
    ImGui::Checkbox("##ClearConfig", &clearConfOnSave_);
    if (clearConfOnSave_) {
        ImGui::SameLine();
        ImGui::Text("Config '%s' will be wiped on exit!", configName_.c_str());
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
