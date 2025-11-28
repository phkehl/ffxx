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

#ifndef __GUI_WIN_EXPERIMENT_HPP__
#define __GUI_WIN_EXPERIMENT_HPP__

#include "ffgui_inc.hpp"
//
#include "glmatrix.hpp"
#include "opengl.hpp"
#include "utils.hpp"
//
#include "gui_notify.hpp"
#include "gui_widget_log.hpp"
#include "gui_widget_map.hpp"
#include "gui_widget_tabbar.hpp"
#include "gui_win.hpp"
#include "gui_win_filedialog.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinExperiment : public GuiWin
{
   public:
    GuiWinExperiment();
    ~GuiWinExperiment();

    void DrawWindow() final;

   protected:
    GuiWidgetTabbar tabbar_;

    // File dialogues
    GuiWinFileDialog openFileDialog_;
    std::string openFilePath_;
    GuiWinFileDialog saveFileDialog_;
    std::string saveFilePath_;
    void _DrawGuiWinFileDialog();

    // Notifications
    void _DrawGuiNotify();

    // Maps
    std::unique_ptr<GuiWidgetMap> map_;
    void _DrawGuiWidgetMap();

    // Matrix
    void _DrawMatrix();
    OpenGL::FrameBuffer framebuffer_;
    GlMatrix::Matrix matrix_;
    bool matrixInit_ = false;

    // Input data
    void _DrawData();
    void _DataObserver(const DataPtr& data);
#if 0
    DataSrcDst inDataSrc_ = { "ExperimentSrc" };
    DataSrcDst inputDataObs_ = { "ExperimentObs" };
    GuiWidgetLog inDataLog_;
    std::size_t inputDataNotifyMsgCount_ = 0;
    DataPtr inputDataLatest_;
    DataMsgPtr inputDataLatestMsg_;
#endif

    // Icons
    void _DrawIcons();
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_EXPERIMENT_HPP__
