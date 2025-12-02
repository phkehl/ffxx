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

#ifndef __GUI_MSG_UBX_TIM_TM2_HPP__
#define __GUI_MSG_UBX_TIM_TM2_HPP__

//
#include "ffgui_inc.hpp"
//
#include <deque>
//
#include "gui_msg.hpp"
#include "gui_widget_table.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsgUbxTimTm2 : public GuiMsg
{
   public:
    GuiMsgUbxTimTm2(const std::string& viewName, const InputPtr& input);
    ~GuiMsgUbxTimTm2();

    void Update(const DataMsgPtr& msg) final;
    void Update(const Time& now) final;
    void Clear() final;

    void DrawToolbar() final;
    void DrawMsg() final;

   private:
    enum FitMode
    {
        NONE,
        AUTOFIT,
        FOLLOW_X
    };

    struct Config
    {
        FitMode fitMode = FitMode::NONE;
    };
    Config cfg_;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, fitMode)

    struct PlotData
    {
        double tow_ = 0.0;
        bool signal_ = false;
        double pRising_ = 0.0;
        double pFalling_ = 0.0;
        double dutyCycle_ = 0.0;
    };
    static constexpr std::size_t MAX_PLOT_DATA = 10000;

    std::deque<PlotData> plotData_;
    double lastRisingEdge_ = NAN;
    double lastFallingEdge_ = NAN;
    double periodRisingEdge_ = NAN;
    double periodFallingEdge_ = NAN;
    double dutyCycle_ = NAN;
    ImPlotRange plotRangeX_;
    double lastTow_ = NAN;
    bool setRangeX_ = false;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_UBX_TIM_TM2_HPP__
