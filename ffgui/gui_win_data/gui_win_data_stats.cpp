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
    GuiWinData(name, { 100, 25, 0, 0 }, ImGuiWindowFlags_None, input),
    tabbar_   { WinName() },
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

void GuiWinDataStats::_ProcessData(const InputDataPtr& /*data*/)
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
    if (tabbar_.Begin()) {
        if (tabbar_.Item("Variables").isSelected_) {
            table_.DrawTable();
        }
        if (tabbar_.Item("Signal levels").isSelected_) {
            _DrawSiglevelPlot();
        }
        tabbar_.End();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataStats::_DrawSiglevelPlot()
{
    static float rowRatios[2] = { 1.0f, 3.0f };

    if (ImPlot::BeginSubplots("##SignalLevels", 2, 1, { -1, -1 }, ImPlotSubplotFlags_LinkAllX, rowRatios)) {
        static constexpr ImPlotFlags flags = ImPlotFlags_Crosshairs | ImPlotFlags_NoMenus | ImPlotFlags_NoFrame /*| ImPlotFlags_NoLegend*/;

        // Top 5
        if (ImPlot::BeginPlot("##Top5", {0, 0}, flags | ImPlotFlags_NoLegend)) {
            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels);
            ImPlot::SetupAxis(ImAxis_Y1, "Avg. top 5");
            ImPlot::SetupAxisLimits(ImAxis_X1, 0.0f, 55.0f, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.25f, 4.75f, ImGuiCond_Always);
            static constexpr std::array<const char*, 4> yLabels = {{ "L1", "E6", "L2", "L5" }};
            static constexpr std::array<double, 4> yTics = {{ 1.0, 2.0, 3.0, 4.0 }};
            ImPlot::SetupAxisTicks(ImAxis_Y1, yTics.data(), 4, yLabels.data(), false);
            ImPlot::SetupFinish();

            // clang-format off
            const std::array<double, 12> maxX = {{
                dbInfo_.stats.cno_avg_top5_l1.max, dbInfo_.stats.cno_avg_top5_l1.max, NAN,
                dbInfo_.stats.cno_avg_top5_e6.max, dbInfo_.stats.cno_avg_top5_e6.max, NAN,
                dbInfo_.stats.cno_avg_top5_l2.max, dbInfo_.stats.cno_avg_top5_l2.max, NAN,
                dbInfo_.stats.cno_avg_top5_l5.max, dbInfo_.stats.cno_avg_top5_l5.max, NAN }};
            const std::array<double, 12> maxY = {{ 0.6f, 1.4f, NAN, 1.6f, 2.4f, NAN, 2.6f, 3.5f, NAN, 3.6f, 4.4f, NAN, }};
            // clang-format on

            ImPlot::PlotLine("Max", maxX.data(), maxY.data(), maxX.size(), { ImPlotProp_LineColor, C4_SIGNAL_UNUSED(), ImPlotProp_LineWeight, 2.0f });

            const float l1x = dbInfo_.stats.cno_avg_top5_l1.mean;
            const float l1y = 1.0f;
            ImPlot::PlotBars("Top5", &l1x, &l1y, 1, 0.5, { ImPlotProp_Flags, ImPlotBarsFlags_Horizontal, ImPlotProp_FillColor, ImGui::ColorConvertU32ToFloat4(CNO_COLOUR(l1x)) });
            const float e6x = dbInfo_.stats.cno_avg_top5_e6.mean;
            const float e6y = 2.0f;
            ImPlot::PlotBars("Top5", &e6x, &e6y, 1, 0.5, { ImPlotProp_Flags, ImPlotBarsFlags_Horizontal, ImPlotProp_FillColor, ImGui::ColorConvertU32ToFloat4(CNO_COLOUR(e6x)) });
            const float l2x = dbInfo_.stats.cno_avg_top5_l2.mean;
            const float l2y = 3.0f;
            ImPlot::PlotBars("Top5", &l2x, &l2y, 1, 0.5, { ImPlotProp_Flags, ImPlotBarsFlags_Horizontal, ImPlotProp_FillColor, ImGui::ColorConvertU32ToFloat4(CNO_COLOUR(l2x)) });
            const float l5x = dbInfo_.stats.cno_avg_top5_l5.mean;
            const float l5y = 4.0f;
            ImPlot::PlotBars("Top5", &l5x, &l5y, 1, 0.5, { ImPlotProp_Flags, ImPlotBarsFlags_Horizontal, ImPlotProp_FillColor, ImGui::ColorConvertU32ToFloat4(CNO_COLOUR(l5x)) });

            // clang-format off
            const std::array<double, 4> stdX = {{ dbInfo_.stats.cno_avg_top5_l1.mean, dbInfo_.stats.cno_avg_top5_e6.mean, dbInfo_.stats.cno_avg_top5_l2.mean, dbInfo_.stats.cno_avg_top5_l5.mean }};
            const std::array<double, 4> stdS = {{ dbInfo_.stats.cno_avg_top5_l1.std,  dbInfo_.stats.cno_avg_top5_e6.std,  dbInfo_.stats.cno_avg_top5_l2.std,  dbInfo_.stats.cno_avg_top5_l5.std  }};
            const std::array<double, 4> stdY = {{ 1.0f, 2.0f, 3.0f, 4.0f }};
            // clang-format on

            ImPlot::PlotErrorBars("Stds", stdX.data(), stdY.data(), stdS.data(), stdX.size(),
                { ImPlotProp_LineColor, C4_SIGNAL_USED(), ImPlotProp_Size, 15.0f, ImPlotProp_LineWeight, 3.0f, ImPlotProp_Flags,ImPlotErrorBarsFlags_Horizontal } );

            ImPlot::EndPlot();
        }

        // Histogram
        if (ImPlot::BeginPlot("##Histogram", {0, 0}, flags)) {

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
            ImPlot::PlotStairs(
                label_trk, Database::Info::CNO_BINS_LO.data(), cno_trk.maxs.data(), Database::Info::CNO_BINS_NUM, { ImPlotProp_LineColor, C4_SIGNAL_UNUSED(), ImPlotProp_LineWeight, 2.0f });
            ImPlot::PlotStairs(
                label_nav, Database::Info::CNO_BINS_LO.data(), cno_nav.maxs.data(), Database::Info::CNO_BINS_NUM, { ImPlotProp_LineColor, C4_SIGNAL_USED(), ImPlotProp_LineWeight, 2.0f });

            // On plot per bin, so that we can cycle through the colourmap. Per bin plot #tracked on top of that #used.
            for (std::size_t ix = 0; ix < Database::Info::CNO_BINS_NUM; ix++) {
                // Plot x for centre of bin
                const float x = Database::Info::CNO_BINS_MI[ix];

                // Grey bar for tracked signals
                ImPlot::PlotBars(label_trk, &x, &cno_trk.means[ix], 1, bin_width, { ImPlotProp_FillColor, C4_SIGNAL_UNUSED() });

                // Coloured bar for used signals
                ImPlot::PlotBars(label_nav, &x, &cno_nav.means[ix], 1, bin_width, { ImPlotProp_FillColor, *(&C4_SIGNAL_00_05() + ix) });

                if (cno_nav.means[ix] > 0.0f) {
                    ImPlot::PlotErrorBars(label_nav, &x, &cno_nav.means[ix], &cno_nav.stds[ix], 1,
                        { ImPlotProp_LineColor, C4_SIGNAL_USED(), ImPlotProp_LineWeight, 3.0f, ImPlotProp_Size, 15.0f });
                }

                if (cno_trk.means[ix] > 0.0f) {
                    ImPlot::PlotErrorBars(label_trk, &x, &cno_trk.means[ix], &cno_trk.stds[ix], 1,
                        { ImPlotProp_LineColor, C4_SIGNAL_UNUSED(), ImPlotProp_LineWeight, 2.0f, ImPlotProp_Size, 10.0f });
                }
            }

            // Last plot determines colour shown in the legend
            ImPlot::PlotDummy(label_trk, { ImPlotProp_LineColor, C4_SIGNAL_UNUSED() });
            ImPlot::PlotDummy(label_nav, { ImPlotProp_LineColor, C4_SIGNAL_USED() });

            ImPlot::EndPlot();
        }
        ImPlot::EndSubplots();
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
