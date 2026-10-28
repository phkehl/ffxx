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
#include "gui_win_data_epoch.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinDataEpoch::GuiWinDataEpoch(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 100, 25, 0, 0 }, ImGuiWindowFlags_None, input),
    table_   { WinName() },
    tabbar_   { WinName() }  // clang-format on
{
    DEBUG("GuiWinDataEpoch(%s)", WinName().c_str());
    table_.AddColumn("Variable");
    table_.AddColumn("Value");
    table_.AddColumn("Description");

    _ClearData();
}

GuiWinDataEpoch::~GuiWinDataEpoch()
{
    DEBUG("~GuiWinDataEpoch(%s)", WinName().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataEpoch::_ProcessData(const InputDataPtr& data)
{
    // New epoch means a new database row is available
    if (data->type_ == InputDataType::EPOCH)
    {
        table_.ClearRows();

        const auto& row = input_->database_->LatestRow();

        table_.AddCellText("seq");
        table_.AddCellTextF("%" PRIuMAX, row.seq_);
        table_.AddCellText("Sequence");
        table_.SetRowUid(0x12345678);

        for (const auto &def: input_->database_->FIELDS) {
            if (def.label) {
                table_.AddCellText(def.name);
                if (def.field == Database::FieldIx::ix_fix_type_val) {
                    table_.AddCellTextF("%g %s", row[def.field], row.epoch_.fixTypeStr);
                } else {
                    table_.AddCellTextF(def.fmt ? def.fmt : "%g", row[def.field]);
                }
                table_.SetRowUid((uint32_t)(uint64_t)(void *)def.label);
                table_.AddCellText(def.label);
            }

            if ((def.field == Database::FieldIx::ix_time_tai) && !row.time_fix.IsZero()) {
                const uint32_t rowUid = 0xdeadbeef;
                table_.AddCellText("-> Time UTC");
                table_.AddCellText(row.time_fix.StrUtcTime());
                table_.AddCellEmpty();
                table_.SetRowUid(rowUid);

                table_.AddCellText("-> Time GPS");
                table_.AddCellText(row.time_fix.StrWnoTow(WnoTow::Sys::GPS));
                table_.AddCellEmpty();
                table_.SetRowUid(rowUid + 1);

                table_.AddCellText("-> Time GAL");
                table_.AddCellText(row.time_fix.StrWnoTow(WnoTow::Sys::GAL));
                table_.AddCellEmpty();
                table_.SetRowUid(rowUid + 2);

                table_.AddCellText("-> Time BDS");
                table_.AddCellText(row.time_fix.StrWnoTow(WnoTow::Sys::BDS));
                table_.AddCellEmpty();
                table_.SetRowUid(rowUid + 3);

                table_.AddCellText("-> Time GLO");
                const auto glo = row.time_fix.GetGloTime(3);
                table_.AddCellTextF("%d:%04d:%09.3f", glo.N4_, glo.Nt_, glo.TOD_);
                table_.AddCellEmpty();
                table_.SetRowUid(rowUid + 4);
            }
        }

        cno_trk_ = { { row.cno_trk_00, row.cno_trk_05, row.cno_trk_10, row.cno_trk_15, row.cno_trk_20, row.cno_trk_25,
            row.cno_trk_30, row.cno_trk_35, row.cno_trk_40, row.cno_trk_45, row.cno_trk_50, row.cno_trk_55 } };
        cno_nav_ = { { row.cno_nav_00, row.cno_nav_05, row.cno_nav_10, row.cno_nav_15, row.cno_nav_20, row.cno_nav_25,
            row.cno_nav_30, row.cno_nav_35, row.cno_nav_40, row.cno_nav_45, row.cno_nav_50, row.cno_nav_55 } };
        cno_trk_max_ = *std::max_element(cno_trk_.begin(), cno_trk_.end());
        cno_top5_ = { { row.cno_avg_top5_l1, row.cno_avg_top5_e6, row.cno_avg_top5_l2, row.cno_avg_top5_l5 } };
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataEpoch::_Loop(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataEpoch::_ClearData()
{
    table_.ClearRows();
    cno_trk_.fill(0.0f);
    cno_nav_.fill(0.0f);
    cno_trk_max_ = 0.0f;
    cno_top5_.fill(0.0f);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataEpoch::_DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataEpoch::_DrawContent()
{
    if (tabbar_.Begin()) {
        if (tabbar_.Item("Variables").isSelected_) { table_.DrawTable(); }
        if (tabbar_.Item("Signal levels").isSelected_) { _DrawSiglevelPlot(); }
        tabbar_.End();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataEpoch::_DrawSiglevelPlot()
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

            for (std::size_t ix = 0; ix < cno_top5_.size(); ix++) {
                const float y = (float)ix + 1;
                const float x = cno_top5_[ix];
                ImPlot::PlotBars("Top5", &x, &y, 1, 0.5, { ImPlotProp_Flags, ImPlotBarsFlags_Horizontal,
                    ImPlotProp_FillColor, ImGui::ColorConvertU32ToFloat4(CNO_COLOUR(x)) });
            }
            ImPlot::EndPlot();
        }

        // Histogram
        if (ImPlot::BeginPlot("##Histogram", {0, 0}, flags)) {

            const float maxSig = (cno_trk_max_ > 30.0f ? 50.0f : 30.0f);
            ImPlot::SetupAxis(ImAxis_X1, "Signal level [dbHz]", ImPlotAxisFlags_NoHighlight);
            ImPlot::SetupAxis(ImAxis_Y1, "Number of signals", ImPlotAxisFlags_NoHighlight);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0.0f, 55.0f, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0f, maxSig, ImGuiCond_Always);
            ImPlot::SetupFinish();

            constexpr const char* label_trk = "Tracked";
            constexpr const char* label_nav = "Used";
            constexpr float bin_width = 4.5f;

            // On plot per bin, so that we can cycle through the colourmap. Per bin plot #tracked on top of that #used.
            for (std::size_t ix = 0; ix < Database::Info::CNO_BINS_NUM; ix++) {
                // Plot x for centre of bin
                const float x = Database::Info::CNO_BINS_MI[ix];

                // Grey bar for tracked signals
                ImPlot::PlotBars(label_trk, &x, &cno_trk_[ix], 1, bin_width, { ImPlotProp_FillColor, C4_SIGNAL_UNUSED() });

                // Coloured bar for used signals
                ImPlot::PlotBars(label_nav, &x, &cno_nav_[ix], 1, bin_width, { ImPlotProp_FillColor, *(&C4_SIGNAL_00_05() + ix) });
            }

            // Last plot determines colour shown in the legend
            ImPlot::PlotDummy(label_trk, { ImPlotProp_FillColor, C4_SIGNAL_UNUSED() });
            ImPlot::PlotDummy(label_nav, { ImPlotProp_FillColor, C4_SIGNAL_USED() });

            ImPlot::EndPlot();
        }
        ImPlot::EndSubplots();
    }
}

/* ****************************************************************************************************************** */
} // ffgui
