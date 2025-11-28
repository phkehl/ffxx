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
#include "gui_win_data_stats.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinDataStats::GuiWinDataStats(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 80, 25, 0, 0 }, ImGuiWindowFlags_None, input),
    tabbar_   { WinName(), ImGuiTabBarFlags_FittingPolicyShrink },
    table_    { WinName() }  // clang-format on
{
    DEBUG("GuiWinDataStats(%s)", WinName().c_str());
    toolbarEna_ = false;

    table_.AddColumn("Variable");
    table_.AddColumn("Count", 0.0f, GuiWidgetTable::ALIGN_RIGHT);
    table_.AddColumn("Mean", 0.0f, GuiWidgetTable::ALIGN_RIGHT);
    table_.AddColumn("Std", 0.0f, GuiWidgetTable::ALIGN_RIGHT);
    table_.AddColumn("Min", 0.0f, GuiWidgetTable::ALIGN_RIGHT);
    table_.AddColumn("Max", 0.0f, GuiWidgetTable::ALIGN_RIGHT);
    table_.AddColumn("Description");

}

GuiWinDataStats::~GuiWinDataStats()
{
    DEBUG("~GuiWinDataStats(%s)", WinName().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataStats::_ProcessData(const DataPtr& /*data*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

#define _SKIP(_field_, _type_, _init_, _fmt_, _label_) /* nothing */
#define _EVAL(_field_, _type_, _init_, _fmt_, _label_)           \
    {                                                            \
        const char* label = _label_;                             \
        const char* fmt = _fmt_;                                 \
        if (label) {                                             \
            const auto& stats = dbInfo_.stats._field_;           \
            table_.AddCellText(#_field_);                        \
            table_.SetRowUid((uint32_t)(uint64_t)(void*)&label); \
            table_.AddCellTextF("%d", stats.count);              \
            table_.AddCellTextF(fmt ? fmt : "%g", stats.mean);   \
            table_.AddCellTextF(fmt ? fmt : "%g", stats.std);    \
            table_.AddCellTextF(fmt ? fmt : "%g", stats.min);    \
            table_.AddCellTextF(fmt ? fmt : "%g", stats.max);    \
            table_.AddCellText(label);                           \
        }                                                        \
    }

void GuiWinDataStats::_Loop(const Time& /*now*/)
{
    if (input_->database_->Changed(this)) {
        dbInfo_ = input_->database_->GetInfo();
        table_.ClearRows();
        DATABASE_COLUMNS(_SKIP, _SKIP, _EVAL, _EVAL, _EVAL)
    }
}

#undef _SKIP
#undef _EVAL


// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataStats::_ClearData()
{
    dbInfo_ = {};
    table_.ClearRows();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataStats::_DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataStats::_DrawContent()
{
    bool drawVars = false;
    bool drawSignal = false;
    if (tabbar_.Begin()) {
        if (tabbar_.Item("Variables")) {
            drawVars = true;
        }
        if (tabbar_.Item("Signal levels")) {
            drawSignal = true;
        }
        tabbar_.End();
    }
    if (drawVars) {
        table_.DrawTable();
    }
    if (drawSignal) {
        _DrawSiglevelPlot();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataStats::_DrawSiglevelPlot()
{
    if (ImPlot::BeginPlot("##SignalLevels", ImVec2(-1, -1),
            ImPlotFlags_Crosshairs | ImPlotFlags_NoMenus | ImPlotFlags_NoFrame /*| ImPlotFlags_NoLegend*/)) {
        // Data
        const auto& cno_trk = dbInfo_.cno_trk;
        const auto& cno_nav = dbInfo_.cno_nav;
        constexpr float bin_width = 4.5f;

        // Configure plot
        const float maxTrk = *std::max_element(cno_trk.maxs.begin(), cno_trk.maxs.end());
        const float maxSig = (maxTrk > 30.0f ? 50.0f : 30.0f);
        ImPlot::SetupAxis(ImAxis_X1, "Signal level [dbHz]", ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxis(ImAxis_Y1, "Number of signals", ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0f, 55.0f, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0f, maxSig, ImGuiCond_Always);
        ImPlot::SetupFinish();

        constexpr const char* label_trk = "Tracked (max/mean/std)";
        constexpr const char* label_nav = "Used    (max/mean/std)";

        // Maxima
        ImPlot::SetNextLineStyle(C4_SIGNAL_UNUSED());
        ImPlot::PlotStairs(
            label_trk, Database::Info::CNO_BINS_LO.data(), cno_trk.maxs.data(), Database::Info::CNO_BINS_NUM);
        ImPlot::SetNextLineStyle(C4_SIGNAL_USED());
        ImPlot::PlotStairs(
            label_nav, Database::Info::CNO_BINS_LO.data(), cno_nav.maxs.data(), Database::Info::CNO_BINS_NUM);

        // On plot per bin, so that we can cycle through the colourmap. Per bin plot #tracked on top of that #used.
        for (std::size_t ix = 0; ix < Database::Info::CNO_BINS_NUM; ix++) {
            // Plot x for centre of bin
            const float x = Database::Info::CNO_BINS_MI[ix];

            // Grey bar for tracked signals
            ImPlot::SetNextFillStyle(C4_SIGNAL_UNUSED());
            ImPlot::PlotBars(label_trk, &x, &cno_trk.means[ix], 1, bin_width);

            // Coloured bar for used signals
            ImPlot::SetNextFillStyle(*(&C4_SIGNAL_00_05() + ix));
            ImPlot::PlotBars(label_nav, &x, &cno_nav.means[ix], 1, bin_width);

            if (cno_nav.means[ix] > 0.0f) {
                ImPlot::SetNextErrorBarStyle(C4_SIGNAL_USED(), 15.0f, 3.0f);
                ImPlot::PlotErrorBars(label_nav, &x, &cno_nav.means[ix], &cno_nav.stds[ix], 1);
            }

            if (cno_trk.means[ix] > 0.0f) {
                ImPlot::SetNextErrorBarStyle(C4_SIGNAL_UNUSED(), 10.0f, 2.0f);
                ImPlot::PlotErrorBars(label_trk, &x, &cno_trk.means[ix], &cno_trk.stds[ix], 1);
            }
        }

        // Last plot determines colour shown in the legend
        ImPlot::SetNextLineStyle(C4_SIGNAL_UNUSED());
        ImPlot::PlotDummy(label_trk);
        ImPlot::SetNextLineStyle(C4_SIGNAL_USED());
        ImPlot::PlotDummy(label_nav);

        ImPlot::EndPlot();
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
