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
#include <mutex>
//
#include <sys/resource.h>
#include <sys/time.h>
//
#include "gui_win_app.hpp"
#include "gui_win_experiment.hpp"
#include "gui_win_play.hpp"
#include "gui_win_hexdecbin.hpp"
#include "input.hpp"
//
#include "gui_app.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

static void sAddLog(const LoggingParams& params, const LoggingLevel level, const char* str)
{
    GuiApp* app = static_cast<GuiApp*>(params.user_);
    app->AddDebugLog(level, str);
    LoggingDefaultWriteFn(params, level, str);
}

// ---------------------------------------------------------------------------------------------------------------------

GuiApp::GuiApp() /* clang-format off */ :
    debugLog_      { "GuiAppDebug", cfg_.logHistory },
    debugTabbar_   { "GuiAppDebug" }  // clang-format on
{
    DEBUG("GuiApp()");

    logging_params_orig_ = LoggingGetParams();
    LoggingParams logging_params = logging_params_orig_;
    logging_params.fn_ = sAddLog;
    logging_params.user_ = this;
    LoggingSetParams(logging_params);

    GuiGlobal::LoadObj("GuiApp", cfg_);
}

GuiApp::~GuiApp()
{
    DEBUG("GuiApp::~GuiApp()");

    cfg_.receiverWinNames.clear();
    for (const auto& win : receiverWins_) {
        cfg_.receiverWinNames.emplace(win->WinName());
    }
    cfg_.logfileWinNames.clear();
    for (const auto& win : logfileWins_) {
        cfg_.logfileWinNames.emplace(win->WinName());
    }
    cfg_.corrWinNames.clear();
    for (const auto& win : corrWins_) {
        cfg_.corrWinNames.emplace(win->WinName());
    }
    cfg_.multiWinNames.clear();
    for (const auto& win : multimapWins_) {
        cfg_.multiWinNames.emplace(win->WinName());
    }
    cfg_.streammuxWinNames.clear();
    for (const auto& win : streammuxWins_) {
        cfg_.streammuxWinNames.emplace(win->WinName());
    }

    GuiGlobal::SaveObj("GuiApp", cfg_);

    // Reconfigure debug output back to terminal only
    LoggingSetParams(logging_params_orig_);
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiApp::Init(const std::vector<std::string>& versions)
{
    DEBUG("GuiApp::Init()");

    // Say hello
    auto str = Sprintf(
        "ffgui (%s) " FF_VERSION_STRING " (" IF_DEBUG("debug", "release") ")", GuiGlobal::ConfigName().c_str());
    NOTICE("%s", str.c_str());
    GuiNotify::Notice("Hello, hello. Good morning, good evening", str);

    // Create application windows
    appWindows_[APP_WIN_ABOUT] = std::make_unique<GuiWinAppAbout>(versions);
    appWindows_[APP_WIN_SETTINGS] = std::make_unique<GuiWinAppSettings>();
    appWindows_[APP_WIN_HELP] = std::make_unique<GuiWinAppHelp>();
    appWindows_[APP_WIN_EXPERIMENT] = std::make_unique<GuiWinExperiment>();
    appWindows_[APP_WIN_PLAY] = std::make_unique<GuiWinPlay>();
    appWindows_[APP_WIN_HEXDECBIN] = std::make_unique<GuiWinHexDecBin>();
    appWindows_[APP_WIN_LEGEND] = std::make_unique<GuiWinAppLegend>();
#ifndef IMGUI_DISABLE_DEMO_WINDOWS
    appWindows_[APP_WIN_IMGUI_DEMO] = std::make_unique<GuiWinAppImguiDemo>();
    appWindows_[APP_WIN_IMPLOT_DEMO] = std::make_unique<GuiWinAppImplotDemo>();
    // appWindows_[APP_WIN_IMPLOT3D_DEMO] = std::make_unique<GuiWinAppImplot3dDemo>();
#endif
#ifndef IMGUI_DISABLE_DEBUG_TOOLS
    appWindows_[APP_WIN_IMGUI_METRICS] = std::make_unique<GuiWinAppImguiMetrics>();
    appWindows_[APP_WIN_IMPLOT_METRICS] = std::make_unique<GuiWinAppImplotMetrics>();
    // appWindows_[APP_WIN_IMPLOT3D_METRICS] = std::make_unique<GuiWinAppImplot3dMetrics>();
#endif
    appWindows_[APP_WIN_IMGUI_STYLES] = std::make_unique<GuiWinAppImguiStyles>();
    appWindows_[APP_WIN_IMPLOT_STYLES] = std::make_unique<GuiWinAppImplotStyles>();
    // appWindows_[APP_WIN_IMPLOT3D_STYLES] = std::make_unique<GuiWinAppImplot3dStyles>();

    // Restore previous windows
    for (const auto& winName : cfg_.receiverWinNames) {
        _CreateReceiverWin(winName);
    }
    for (const auto& winName : cfg_.logfileWinNames) {
        _CreateLogfileWin(winName);
    }
    for (const auto& winName : cfg_.corrWinNames) {
        _CreateCorrWin(winName);
    }
    for (const auto& winName : cfg_.multiWinNames) {
        _CreateMultiWin(winName);
    }
    for (const auto& winName : cfg_.streammuxWinNames) {
        _CreateStreammuxWin(winName);
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

ImVec4 GuiApp::BackgroundColour()
{
    if (cfg_.h4xx0rMode) {
        return ImVec4(0.0f, 0.0f, 0.0f, C4_APP_BACKGROUND().w);
    } else {
        return C4_APP_BACKGROUND();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiApp::Loop(const Time& now)
{
    // Update (some) performance info, throttle it a bit or it's too jittery
    if ((perf_stats_cnt_++ % 10) == 0) {
        perf_stats_.Update();
    }

    // Dispatch input data collected since last call to this
    DispatchData();

    // Run windows loops
    for (auto& win : appWindows_) {
        if (win) {
            win->Loop(now);
        }
    }
    for (auto& win : receiverWins_) {
        win->Loop(now);
    }
    for (auto& win : logfileWins_) {
        win->Loop(now);
    }
    for (auto &win: corrWins_)
    {
        win->Loop(now);
    }
    for (auto &win: multimapWins_)
    {
        win->Loop(now);
    }
    for (auto &win: streammuxWins_)
    {
        win->Loop(now);
    }

    notifications_.Loop(ImGui::GetTime());

    if (cfg_.h4xx0rMode) {
        _LoopMatrix(ImGui::GetTime());
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiApp::DrawFrame()
{
    // CTRL-F4 or CTRL-w to close currently focused window
    bool closeWindow = false;
    if (ImGui::IsKeyPressed(ImGuiKey_F4, false) || ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        ImGuiIO& io = ImGui::GetIO();
        closeWindow = io.KeyCtrl;
    }

    _MainMenu();

    _SlavaUkraini();

    // Draw app windows
    for (auto& appWin : appWindows_) {
        if (appWin && appWin->WinIsOpen()) {
            appWin->DrawWindow();
            if (closeWindow && appWin->WinIsFocused()) {
                appWin->WinClose();
            }
        }
    }

    // Draw receiver windows, remove and destroy closed ones
    for (auto iter = receiverWins_.begin(); iter != receiverWins_.end();) {
        auto& win = *iter;
        if (win->WinIsOpen()) {
            win->DrawWindow();
            win->DrawDataWins(closeWindow);
            if (closeWindow && win->WinIsFocused()) {
                win->WinClose();
            }
            iter++;
        } else {
            iter = receiverWins_.erase(iter);
        }
    }

    // Draw logfile windows, remove and destroy closed ones
    for (auto iter = logfileWins_.begin(); iter != logfileWins_.end();) {
        auto& win = *iter;
        if (win->WinIsOpen()) {
            win->DrawWindow();
            win->DrawDataWins(closeWindow);
            if (closeWindow && win->WinIsFocused()) {
                win->WinClose();
            }
            iter++;
        } else {
            iter = logfileWins_.erase(iter);
        }
    }

    // Draw corrections windows, remove and destroy closed ones
    for (auto iter = corrWins_.begin(); iter != corrWins_.end();) {
        auto& win = *iter;
        if (win->WinIsOpen()) {
            win->DrawWindow();
            win->DrawDataWins(closeWindow);
            if (closeWindow && win->WinIsFocused()) {
                win->WinClose();
            }
            iter++;
        } else {
            iter = corrWins_.erase(iter);
        }
    }

    // Draw multimap windows, remove and destroy closed ones
    for (auto iter = multimapWins_.begin(); iter != multimapWins_.end();) {
        auto& win = *iter;
        if (win->WinIsOpen()) {
            win->DrawWindow();
            if (closeWindow && win->WinIsFocused()) {
                win->WinClose();
            }
            iter++;
        } else {
            iter = multimapWins_.erase(iter);
        }
    }

    // Draw multimap windows, remove and destroy closed ones
    for (auto iter = streammuxWins_.begin(); iter != streammuxWins_.end();) {
        auto& win = *iter;
        if (win->WinIsOpen()) {
            win->DrawWindow();
            if (closeWindow && win->WinIsFocused()) {
                win->WinClose();
            }
            iter++;
        } else {
            iter = streammuxWins_.erase(iter);
        }
    }

    perfData_[Perf_e::CPU].Add(perf_stats_.cpu_curr_);
    perfData_[Perf_e::RSS].Add(perf_stats_.mem_curr_);

    // Debug window
    if (cfg_.debugWinOpen) {
        _DrawDebugWin();
    }

    if (cfg_.h4xx0rMode) {
        _DrawMatrix();
    }

    // Notifications
    notifications_.Draw();
}

// ---------------------------------------------------------------------------------------------------------------------

static constexpr int MAX_INPUT_WINS = 20;

template <typename WinT>
static void CreateWinHelper(
    std::list<std::unique_ptr<WinT>>& wins, const std::string& baseName, const std::string& prevWinName)
{
    // Restore a previously open window.
    if (!prevWinName.empty()) {
        wins.emplace_back(std::make_unique<WinT>(prevWinName));
    }
    // Create a new window
    else {
        for (int n = 1; n <= MAX_INPUT_WINS; n++) {
            const std::string winName = baseName + "_" + std::to_string(n);
            if (std::find_if(wins.begin(), wins.end(),
                    [&winName](const auto& win) { return win->WinName() == winName; }) == wins.end()) {
                auto win = std::make_unique<WinT>(winName);
                win->WinOpen();
                wins.push_back(std::move(win));
                break;
            }
        }
    }

    // Keep them ordered (for the menu)
    wins.sort([](const auto& a, const auto& b) { return a->WinName() < b->WinName(); });
}

void GuiApp::_CreateReceiverWin(const std::string &winName)
{
    CreateWinHelper(receiverWins_, "Receiver", winName);
}

void GuiApp::_CreateLogfileWin(const std::string &winName)
{
    CreateWinHelper(logfileWins_, "Logfile", winName);
}

void GuiApp::_CreateCorrWin(const std::string &winName)
{
    CreateWinHelper(corrWins_, "Corrections", winName);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiApp::_CreateMultiWin(const std::string &winName)
{
    CreateWinHelper(multimapWins_, "MultiMap", winName);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiApp::_CreateStreammuxWin(const std::string &winName)
{
    CreateWinHelper(streammuxWins_, "StreamMux", winName);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiApp::_MainMenu()
{
    if (ImGui::BeginMainMenuBar()) {
        // Store menu bar height FIXME: is there a better way than doing this here for every frame?
        menuBarHeight_ = ImGui::GetWindowSize().y;

        // ImGui::PushStyleColor(ImGuiCol_PopupBg, GUI_VAR.imguiStyle->Colors[ImGuiCol_MenuBarBg]);

        if (ImGui::BeginMenu(ICON_FK_HEART)) {
            for (auto& win : receiverWins_) {
                win->DataWinMenuSwitcher();
            }

            if (!receiverWins_.empty()) {
                ImGui::Separator();
            }

            for (auto& win : logfileWins_) {
                win->DataWinMenuSwitcher();
            }

            if (!logfileWins_.empty()) {
                ImGui::Separator();
            }

            for (auto& win : corrWins_) {
                win->DataWinMenuSwitcher();
            }

            if (!corrWins_.empty()) {
                ImGui::Separator();
            }

            for (auto& win : multimapWins_) {
                if (ImGui::MenuItem(win->WinTitle().c_str())) {
                    win->WinFocus();
                }
            }

            if (!multimapWins_.empty()) {
                ImGui::Separator();
            }


            for (auto& win : streammuxWins_) {
                if (ImGui::MenuItem(win->WinTitle().c_str())) {
                    win->WinFocus();
                }
            }

            if (!streammuxWins_.empty()) {
                ImGui::Separator();
            }

            for (AppWin_e e : { APP_WIN_ABOUT, APP_WIN_SETTINGS, APP_WIN_HELP, APP_WIN_EXPERIMENT, APP_WIN_PLAY,
                     APP_WIN_LEGEND, APP_WIN_HEXDECBIN, APP_WIN_IMGUI_DEMO, APP_WIN_IMGUI_METRICS, APP_WIN_IMGUI_STYLES,
                     APP_WIN_IMPLOT_DEMO, APP_WIN_IMPLOT_METRICS, APP_WIN_IMPLOT_STYLES,
                     // APP_WIN_IMPLOT3D_DEMO, APP_WIN_IMPLOT3D_METRICS, APP_WIN_IMPLOT3D_STYLES
                 }) {
                auto& win = appWindows_[e];
                if (win && win->WinIsOpen() && ImGui::MenuItem(win->WinTitle().c_str())) {
                    win->WinFocus();
                }
            }

            ImGui::EndMenu();
        }


        if (ImGui::BeginMenu(ICON_FK_TTY " Receiver")) {
            if (ImGui::MenuItem("New receiver")) {
                _CreateReceiverWin();
            }
            if (!receiverWins_.empty()) {
                ImGui::Separator();
                for (auto& win : receiverWins_) {
                    if (ImGui::BeginMenu(win->WinTitle().c_str())) {
                        win->DataWinMenu();
                        ImGui::EndMenu();
                    }
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(ICON_FK_FOLDER " Logfile")) {
            if (ImGui::MenuItem("New logfile")) {
                _CreateLogfileWin();
            }
            if (!logfileWins_.empty()) {
                ImGui::Separator();
                for (auto& win : logfileWins_) {
                    if (ImGui::BeginMenu(win->WinTitle().c_str())) {
                        win->DataWinMenu();
                        ImGui::EndMenu();
                    }
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(ICON_FK_PODCAST " Corrections")) {
            if (ImGui::MenuItem("New corrections")) {
                _CreateCorrWin();
            }
            if (!corrWins_.empty()) {
                ImGui::Separator();
                for (auto& win : corrWins_) {
                    if (ImGui::BeginMenu(win->WinTitle().c_str())) {
                        win->DataWinMenu();
                        ImGui::EndMenu();
                    }
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(ICON_FK_COFFEE " Tools")) {
            if (ImGui::BeginMenu("MultiMap")) {
                if (ImGui::MenuItem("New multimap")) {
                    _CreateMultiWin();
                }
                if (!multimapWins_.empty()) {
                    ImGui::Separator();
                    for (auto& win : multimapWins_) {
                        if (ImGui::MenuItem(win->WinTitle().c_str())) {
                            win->WinOpen();
                        }
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("StreamMux")) {
                if (ImGui::MenuItem("New streammux")) {
                    _CreateStreammuxWin();
                }
                if (!streammuxWins_.empty()) {
                    ImGui::Separator();
                    for (auto& win : streammuxWins_) {
                        if (ImGui::MenuItem(win->WinTitle().c_str())) {
                            win->WinOpen();
                        }
                    }
                }
                ImGui::EndMenu();
            }

            for (AppWin_e e : { APP_WIN_SETTINGS, APP_WIN_PLAY, APP_WIN_HEXDECBIN }) {
                auto& win = appWindows_[e];
                if (win && ImGui::MenuItem(win->WinName().c_str())) {
                    win->WinOpen();
                }
            }

            ImGui::EndMenu();
        }

        // Some app windows are shown in the tools menu, others are buttons in the debug tools window/tab
        if (ImGui::BeginMenu(ICON_FK_UMBRELLA " Help")) {
            for (AppWin_e e : { APP_WIN_ABOUT, APP_WIN_HELP, APP_WIN_LEGEND }) {
                auto& win = appWindows_[e];
                if (win && ImGui::MenuItem(win->WinName().c_str())) {
                    win->WinOpen();
                }
            }

            ImGui::Separator();

            ImGui::MenuItem("Debug", NULL, &cfg_.debugWinOpen);

            if (ImGui::BeginMenu("H4xx0r")) {
                if (ImGui::MenuItem(cfg_.h4xx0rMode ? "Leave matrix" : "Enter matrix")) {
                    cfg_.h4xx0rMode = _ConfigMatrix(!cfg_.h4xx0rMode);
                }
                if (ImGui::TreeNode("Config")) {
                    ImGui::BeginDisabled(cfg_.h4xx0rMode);
                    _DrawMatrixConfig();
                    ImGui::EndDisabled();
                    ImGui::TreePop();
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        // ImGui::PopStyleColor();

        // Draw performance info
        char text[200];
        ImGuiIO& io = ImGui::GetIO();
        snprintf(text, sizeof(text),
            ICON_FK_TACHOMETER " %2.0f | " ICON_FK_CLOCK_O " %.1f | " ICON_FK_FILM " %d | " ICON_FK_THERMOMETER_HALF
                               " %.0f | " ICON_FK_BUG " %.0f ",
            io.Framerate,  // io.Framerate > FLT_EPSILON ? 1000.0f / io.Framerate : 0.0f,
            // io.DeltaTime > FLT_EPSILON ? 1.0 / io.DeltaTime : 0.0, io.DeltaTime * 1e3,
            ImGui::GetTime(), ImGui::GetFrameCount(), perf_stats_.mem_curr_, perf_stats_.cpu_curr_);
        const float w = ImGui::CalcTextSize(text).x + GUI_VAR.imguiStyle->WindowPadding.x;
        ImGui::SameLine(ImGui::GetWindowWidth() - w);
        ImGui::PushStyleColor(ImGuiCol_Text,
            io.DeltaTime < (1e-3 * ((1000 - 100) / GUI_CFG.minFrameRate)) ? C_C_BRIGHTWHITE() : C_C_WHITE());
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(ICON_FK_TACHOMETER " = FPS [Hz], " ICON_FK_CLOCK_O " = time [s], " ICON_FK_FILM
                                                      " = frames [-], " ICON_FK_THERMOMETER_HALF
                                                      " = RSS [MiB], " ICON_FK_BUG " = CPU [%s]");
            ImGui::EndTooltip();
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            cfg_.debugWinOpen = !cfg_.debugWinOpen;
        }
        ImGui::EndMainMenuBar();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiApp::ConfirmClose(bool& display, bool& done)
{
    const char* const popupName = "Really Close?###ConfirmClose";
    ImGui::OpenPopup(popupName);

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(popupName, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("All connections will be shut down.\nAll data will be lost.\n");

        ImGui::Separator();

        const auto size = GUI_VAR.iconSize * Vec2f(6, 1);
        if (ImGui::Button(ICON_FK_CHECK " OK", size)) {
            ImGui::CloseCurrentPopup();
            done = true;
        }

        // Make the OK button the selected button, activate keyboard navigation. Like this user can C-F4 and ENTER to exit
        ImGui::SetItemDefaultFocus();
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetNavCursorVisible(true);
        }

        ImGui::SameLine();
        if (ImGui::Button(ICON_FK_TIMES " Cancel", size)) {
            ImGui::CloseCurrentPopup();
            display = false;
        }
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiApp::_DrawDebugWin()
{
    // Always position window at NE corner, constrain min/max size
    auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x, menuBarHeight_), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(500, 200), ImVec2(io.DisplaySize.x, io.DisplaySize.y - menuBarHeight_));

    // Style window (no border, no round corners, no alpha if focused/active, bg colour same as menubar)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, debugWinDim_ ? 0.75f * GUI_VAR.imguiStyle->Alpha : 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, GUI_VAR.imguiStyle->Colors[ImGuiCol_MenuBarBg]);

    // Start window
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
    const bool isOpen = ImGui::Begin("Debug##FfGuiDebugWin", &cfg_.debugWinOpen, flags);

    // Pop style stack
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();

    // Should it be dimmed, i.e. is it neither focussed nor active?
    debugWinDim_ = !(ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) ||
                     ImGui::IsWindowHovered(ImGuiFocusedFlags_ChildWindows));

    // Draw window
    if (isOpen) {
        if (debugTabbar_.Begin()) {
            debugTabbar_.Item("Log", [&]() { debugLog_.DrawWidget(); });

            debugTabbar_.Item("Tools", [&]() {
                const ImVec2 buttonSize{ GUI_VAR.charSizeX * 20, 0 };

                const struct
                {
                    const char* label;
                    int win;
                    bool newline;
                } buttons[] = {
                    { "ImGui demo", APP_WIN_IMGUI_DEMO, true },
                    { "ImGui metrics", APP_WIN_IMGUI_METRICS, false },
                    { "ImGui styles", APP_WIN_IMGUI_STYLES, false },
                    { "ImPlot demo", APP_WIN_IMPLOT_DEMO, true },
                    { "ImPlot metrics", APP_WIN_IMPLOT_METRICS, false },
                    { "ImPlot styles", APP_WIN_IMPLOT_STYLES, false },
                    // { "ImPlot3d demo", APP_WIN_IMPLOT3D_DEMO, true },
                    // { "ImPlot3d metrics", APP_WIN_IMPLOT3D_METRICS, false },
                    // { "ImPlot3d styles", APP_WIN_IMPLOT3D_STYLES, false },
                    { "Experiments", APP_WIN_EXPERIMENT, true },
                };
                for (std::size_t ix = 0; ix < NumOf(buttons); ix++) {
                    if (!buttons[ix].newline) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(buttons[ix].label, buttonSize) && appWindows_[buttons[ix].win]) {
                        if (!appWindows_[buttons[ix].win]->WinIsOpen()) {
                            appWindows_[buttons[ix].win]->WinOpen();
                        } else {
                            appWindows_[buttons[ix].win]->WinFocus();
                        }
                    }
                }
            });

            debugTabbar_.Item("Perf", [&]() {
                const ImVec2 sizeAvail = ImGui::GetContentRegionAvail();
                const float bottom = GUI_VAR.lineHeight + GUI_VAR.imguiStyle->ItemSpacing.y;
                const ImVec2 plotSize1( sizeAvail.x, (sizeAvail.y * 2 / 3) - GUI_VAR.imguiStyle->ItemSpacing.y );
                const ImVec2 plotSize2( ((sizeAvail.x - ((_NUM_PERF - 1) * GUI_VAR.imguiStyle->ItemSpacing.x)) / _NUM_PERF),
                    sizeAvail.y - plotSize1.y - GUI_VAR.imguiStyle->ItemSpacing.y - bottom );

                const struct
                {
                    const char* title;
                    const char* label;
                    enum Perf_e perf;
                } plots[] = {
                    { "##Newframe", "Newframe", Perf_e::NEWFRAME },
                    { "##Loop", "Loop", Perf_e::LOOP },
                    { "##Draw", "Draw", Perf_e::DRAW },
                    { "##RenderImGui", "R ImGui", Perf_e::RENDER_IM },
                    { "##RenderOpenGL", "R OpenGL", Perf_e::RENDER_IM },
                    { "##RenderWin", "R Wins", Perf_e::RENDER_PL },
                    { "##Total", "Total", Perf_e::TOTAL },
                    { "##CPU", "CPU", Perf_e::CPU },
                    { "##RSS", "RSS", Perf_e::RSS },
                };

                if (ImPlot::BeginPlot("##Performance", plotSize1,
                        ImPlotFlags_Crosshairs | ImPlotFlags_NoMenus | ImPlotFlags_NoFrame)) {
                    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0005, 1000.0, ImGuiCond_Once);
                    ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, perfData_[0].data.size(), ImGuiCond_Always);
                    ImPlot::SetupLegend(ImPlotLocation_South, ImPlotLegendFlags_Horizontal);
                    ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);
                    ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_LockMin);
                    ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
                    ImPlot::SetupFinish();
                    for (std::size_t plotIx = 0; plotIx < NumOf(plots); plotIx++) {
                        PerfData* pd = &perfData_[plots[plotIx].perf];
                        // PlotLineG, PlotStairsG
                        ImPlot::PlotLineG(
                            plots[plotIx].label,
                            [](int ix, void* arg) {
                                const PerfData* perf = (PerfData*)arg;
                                return ImPlotPoint(ix, perf->data[(perf->ix + ix) % perf->data.size()]);
                            },
                            pd, pd->data.size());
                    }
                    ImPlot::EndPlot();
                }

                const ImPlotRange range;  // (0.0, 2.0);
                for (std::size_t plotIx = 0; plotIx < NumOf(plots); plotIx++) {
                    if (ImPlot::BeginPlot(plots[plotIx].title, plotSize2,
                            ImPlotFlags_Crosshairs | ImPlotFlags_NoMenus | ImPlotFlags_NoLegend |
                                ImPlotFlags_NoFrame)) {
                        ImPlot::SetupAxis(ImAxis_X1, nullptr,
                            ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoGridLines |
                                ImPlotAxisFlags_NoTickMarks);
                        ImPlot::SetupAxis(ImAxis_Y1, nullptr,
                            ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoGridLines |
                                ImPlotAxisFlags_NoTickMarks);
                        ImPlot::SetupFinish();
                        PerfData& pd = perfData_[plots[plotIx].perf];
                        ImPlot::SetNextFillStyle(ImPlot::GetColormapColor(plotIx));
                        ImPlot::PlotHistogram(
                            plots[plotIx].label, pd.data.data(), pd.data.size(), ImPlotBin_Sturges, 0.8, range);
                        ImPlot::EndPlot();
                    }
                    if (plotIx < (NumOf(plots) - 1)) {
                        ImGui::SameLine();
                    }
                }

                const auto mem_usage = GetMemUsage();
                ImGui::Text("Memory usage [MB]: Total %.1f, RSS %.1f, shared %.1f, text %.1f, data %.1f",
                    mem_usage.size_, mem_usage.resident_, mem_usage.shared_, mem_usage.text_, mem_usage.data_);
            });

            debugTabbar_.Item("Data", [&]() {

                DrawDataDebug();
                DrawInputDebug();
#if 0
                Gui::TextTitle("Receivers");
                for (auto &receiver: GuiData::Receivers())
                {
                    ImGui::Text("%s (%s)", receiver->GetName().c_str(), receiver->GetRxVer().c_str());
                }
                ImGui::Spacing();
                Gui::TextTitle("Logfiles");
                for (auto &logfile: GuiData::Logfiles())
                {
                    ImGui::Text("%s (%s)", logfile->GetName().c_str(), logfile->GetRxVer().c_str());
                }
                ImGui::Spacing();
                Gui::TextTitle("Databases");
                for (auto &database: GuiData::Databases())
                {
                    ImGui::Text("%s (%d/%d)", database->GetName().c_str(), database->Size(), database->MaxSize());
                }

#endif
            });

            debugTabbar_.End();
        }
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiApp::AddDebugLog(const LoggingLevel level, const char* str)
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    debugLog_.AddLine(str, level);
}

// ---------------------------------------------------------------------------------------------------------------------

GuiApp::PerfData::PerfData()
{
    data.fill(0.0f);
    // std::fill(data.begin(), data.end(), 0);
}

void GuiApp::PerfData::Tic()
{
    t0 = std::chrono::steady_clock::now();
}

void GuiApp::PerfData::Toc()
{
    const auto t1 = std::chrono::steady_clock::now();
    const std::chrono::duration<float, std::milli> dt = t1 - t0;
    Add(dt.count());
}

void GuiApp::PerfData::Add(const float value)
{
    data[ix] = value;
    ix++;
    ix %= data.size();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiApp::PerfTic(const enum Perf_e perf)
{
    if (perf < _NUM_PERF) {
        perfData_[perf].Tic();
    }
}

void GuiApp::PerfToc(const enum Perf_e perf)
{
    if (perf < _NUM_PERF) {
        perfData_[perf].Toc();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiApp::_ConfigMatrix(const bool enable)
{
    DEBUG("H4xx0r mode: %s", enable ? "enable" : "disable");
    if (enable) {
        if (!matrix_.Init(cfg_.matrixOpts)) {
            return false;
        }
        matrixLastRender_ = ImGui::GetTime();
    } else {
        matrix_.Destroy();
    }
    return enable;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiApp::_LoopMatrix(const double& now)
{
    const double rate = 50.0;  // 1/s
    int nIter = (now - matrixLastRender_ + (0.5 / rate)) * rate;
    matrixLastRender_ = now;
    nIter = std::clamp(nIter, 1, 10);
    while (nIter > 0) {
        matrix_.Animate();
        nIter--;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiApp::_DrawMatrix()
{
    ImGuiViewport* viewPort = ImGui::GetMainViewport();
    if (matrixFb_.Begin(viewPort->Size.x, viewPort->Size.y)) {
        matrix_.Render(viewPort->Size.x, viewPort->Size.y);
        matrixFb_.End();
        void* tex = matrixFb_.GetTexture();
        if (tex) {
            const Vec2f pos = viewPort->Pos;
            const Vec2f size = viewPort->Size;
            ImDrawList* draw = ImGui::GetBackgroundDrawList();
            draw->AddImage({ (ImTextureID)tex }, viewPort->Pos, pos + size, ImVec2(0, 1), ImVec2(1, 0));
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiApp::_DrawMatrixConfig()
{
    // clang-format off
    const struct { enum GlMatrix::Options::Mode_e mode; const char* label; } modes[] = {
        { GlMatrix::Options::MATRIX,      "MATRIX" },
        { GlMatrix::Options::DNA,         "DNA" },
        { GlMatrix::Options::BINARY,      "BINARY" },
        { GlMatrix::Options::HEXADECIMAL, "HEXADECIMAL" },
        { GlMatrix::Options::DECIMAL,     "DECIMAL" }, };  // clang-format on
    const char* modeStr = "?";
    for (std::size_t ix = 0; ix < NumOf(modes); ix++) {
        if (modes[ix].mode == cfg_.matrixOpts.mode) {
            modeStr = modes[ix].label;
            break;
        }
    }
    if (ImGui::BeginCombo("Mode", modeStr)) {
        for (std::size_t ix = 0; ix < NumOf(modes); ix++) {
            if (ImGui::Selectable(modes[ix].label, modes[ix].mode == cfg_.matrixOpts.mode)) {
                cfg_.matrixOpts.mode = modes[ix].mode;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SliderFloat("Speed", &cfg_.matrixOpts.speed, 0.1f, 5.0f);
    ImGui::SliderFloat("Density", &cfg_.matrixOpts.density, 1.0f, 200.0f, "%.0f");
    ImGui::Checkbox("Clock", &cfg_.matrixOpts.doClock);
    ImGui::Checkbox("Fog", &cfg_.matrixOpts.doFog);
    ImGui::Checkbox("Waves", &cfg_.matrixOpts.doWaves);
    ImGui::Checkbox("Rotate", &cfg_.matrixOpts.doRotate);
    ImGui::Checkbox("Debug grid", &cfg_.matrixOpts.debugGrid);
    ImGui::Checkbox("Debug frames", &cfg_.matrixOpts.debugFrames);
    ImGui::PushItemWidth(GUI_VAR.charSizeX * 40);
    ImColor colour((int)cfg_.matrixOpts.colour[0], (int)cfg_.matrixOpts.colour[1], (int)cfg_.matrixOpts.colour[2]);
    if (ImGui::ColorEdit4("Colour", &colour.Value.x,
            ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoInputs)) {
        cfg_.matrixOpts.colour[0] = colour.Value.x * 255.0f;
        cfg_.matrixOpts.colour[1] = colour.Value.y * 255.0f;
        cfg_.matrixOpts.colour[2] = colour.Value.z * 255.0f;
    }
    ImGui::PopItemWidth();

    if (ImGui::Button("Defaults")) {
        cfg_.matrixOpts = {};
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiApp::_SlavaUkraini()
{
    ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                                ImGuiWindowFlags_NoBackground;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float wx = viewport->WorkPos.x;
    const float wy = viewport->WorkPos.y + viewport->WorkSize.y;
    ImGui::SetNextWindowPos({ wx, wy }, ImGuiCond_Always, { 0.0f, 1.0f });
    ImGui::SetNextWindowViewport(viewport->ID);
    // ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("SlavaUkraini", NULL, winFlags)) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const auto p0 = ImGui::GetCursorScreenPos();
        const float h = GUI_VAR.charSizeY;
        const float w = h * (3.0f / 2.0f);
        const float h2 = h / 2.0f;
        draw->AddRectFilled({ p0.x, p0.y }, { p0.x + w, p0.y + h2 }, IM_COL32(0x00, 0x57, 0xb7, 0xff));
        draw->AddRectFilled({ p0.x, p0.y + h2 }, { p0.x + w, p0.y + h }, IM_COL32(0xff, 0xd7, 0x00, 0xff));
        // ImGui::Indent(w); ImGui::TextUnformatted(" Слава Україні!");
        // ImGui::Dummy({w, h });
        if (ImGui::InvisibleButton("SlavaUkraini", { w, h })) {
            std::system("xdg-open https://stand-with-ukraine.pp.ua/");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        Gui::ItemTooltip("Слава Україні!");
    }

    ImGui::End();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
