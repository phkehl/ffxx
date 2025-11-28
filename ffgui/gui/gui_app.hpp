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

#ifndef __GUI_APP_HPP__
#define __GUI_APP_HPP__

//
#include "ffgui_inc.hpp"
//
#include <chrono>
//
#include <fpsdk_common/app.hpp>
using namespace fpsdk::common::app;
#include <nlohmann/json.hpp>
//
#include "glmatrix.hpp"
#include "opengl.hpp"
//
#include "gui_notify.hpp"
#include "gui_widget_log.hpp"
#include "gui_widget_tabbar.hpp"
#include "gui_win.hpp"
#include "gui_win_input_corr.hpp"
#include "gui_win_input_logfile.hpp"
#include "gui_win_input_receiver.hpp"
#include "gui_win_multimap.hpp"
#include "gui_win_streammux.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiApp
{
   public:
    GuiApp();
    ~GuiApp();

    bool Init(const std::vector<std::string>& versions);

    // Main application loop, do what needs to be done regularly
    void Loop(const Time& now);

    // Draw one app frame, call all windows drawing methods
    void DrawFrame();

    void ConfirmClose(bool& display, bool& done);
    ImVec4 BackgroundColour();

    // Performance monitor
    // clang-format off
    enum Perf_e { NEWFRAME = 0, LOOP, DRAW, RENDER_IM, RENDER_GL, RENDER_PL, TOTAL, CPU, RSS, _NUM_PERF };  // clang-format on
    void PerfTic(const enum Perf_e perf);
    void PerfToc(const enum Perf_e perf);

    void AddDebugLog(const LoggingLevel level, const char* str);

   private:
    void _MainMenu();

    // Application windows
    enum AppWin_e  // clang-format off
    {
        APP_WIN_ABOUT = 0, APP_WIN_SETTINGS, APP_WIN_HELP, APP_WIN_EXPERIMENT, APP_WIN_PLAY, APP_WIN_LEGEND, APP_WIN_HEXDECBIN,
        APP_WIN_IMGUI_DEMO,    APP_WIN_IMGUI_METRICS,    APP_WIN_IMGUI_STYLES,
        APP_WIN_IMPLOT_DEMO,   APP_WIN_IMPLOT_METRICS,   APP_WIN_IMPLOT_STYLES,
        // APP_WIN_IMPLOT3D_DEMO, APP_WIN_IMPLOT3D_METRICS, APP_WIN_IMPLOT3D_STYLES,
        _NUM_APP_WIN
    };  // clang-format on
    std::array<std::unique_ptr<GuiWin>, _NUM_APP_WIN> appWindows_;
    GuiNotify notifications_;

    // Persistent config data
    struct Config
    {
        bool debugWinOpen = false;
        bool h4xx0rMode = false;
        GlMatrix::Options matrixOpts;
        std::set<std::string> receiverWinNames;
        std::set<std::string> logfileWinNames;
        std::set<std::string> corrWinNames;
        std::set<std::string> multiWinNames;
        std::set<std::string> streammuxWinNames;
        HistoryEntries logHistory;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, debugWinOpen, h4xx0rMode, matrixOpts, receiverWinNames,
        logfileWinNames, corrWinNames, multiWinNames, streammuxWinNames, logHistory)
    Config cfg_;

    // Input windows
    std::list< std::unique_ptr<GuiWinInputReceiver> > receiverWins_;
    void _CreateReceiverWin(const std::string& winName = "");
    std::list< std::unique_ptr<GuiWinInputLogfile> > logfileWins_;
    void _CreateLogfileWin(const std::string& winName = "");
    std::list< std::unique_ptr<GuiWinInputCorr> > corrWins_;
    void _CreateCorrWin(const std::string &winName = "");

    // Tool windows
    std::list< std::unique_ptr<GuiWinMultimap> > multimapWins_;
    void _CreateMultiWin(const std::string &winName = "");
    std::list< std::unique_ptr<GuiWinStreammux> > streammuxWins_;
    void _CreateStreammuxWin(const std::string &winName = "");

    // clang-format off
    // Debugging
    fpsdk::common::logging::LoggingParams logging_params_orig_;
    PerfStats            perf_stats_;
    int                  perf_stats_cnt_ = 0;
    bool                 debugWinDim_ = false;
    GuiWidgetLog         debugLog_;
    float                menuBarHeight_ = 0.0f;
    GuiWidgetTabbar      debugTabbar_;
    void _DrawDebugWin();
    // H4xx0r mode
    GlMatrix::Matrix     matrix_;
    OpenGL::FrameBuffer  matrixFb_;
    double               matrixLastRender_;
    bool _ConfigMatrix(const bool enable);
    void _LoopMatrix(const double& now);
    void _DrawMatrix();
    void _DrawMatrixConfig();
    // clang-format on
    void _SlavaUkraini();

    struct PerfData
    {
        PerfData();
        std::array<float, 250> data;
        std::size_t ix = 0;
        std::chrono::steady_clock::time_point t0;
        void Tic();
        void Toc();
        void Add(const float value);
    };
    std::array<PerfData, _NUM_PERF> perfData_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_APP_HPP__
