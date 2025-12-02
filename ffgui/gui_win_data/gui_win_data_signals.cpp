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
#include "gui_win_data_signals.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinDataSignals::GuiWinDataSignals(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 115, 40, 0, 0 }, ImGuiWindowFlags_None, input),
    tabbarMode_     { WinName() + "Mode" },
    tabbarFilter_   { WinName() + "Filter", ImGuiTabBarFlags_FittingPolicyShrink | ImGuiTabBarFlags_TabListPopupButton },
    table_          { WinName() + "List" }  // clang-format on
{
    DEBUG("GuiWinDataSignals(%s)", WinName().c_str());
    GuiGlobal::LoadObj(WinName() + ".GuiWinDataSignals", cfg_);

    ClearData();

    table_.AddColumn("SV", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE | GuiWidgetTable::ColumnFlags::HIDE_SAME);
    table_.AddColumn("Signal  ", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE);
    table_.AddColumn("Bd.", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE);
    table_.AddColumn("Level", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE);
    table_.AddColumn("Use", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE);
    table_.AddColumn("Iono");
    table_.AddColumn("Health");
    table_.AddColumn("Used");
    table_.AddColumn("PR res.", 0.0f, GuiWidgetTable::ColumnFlags::SORTABLE);
    table_.AddColumn("Corrections");
}

GuiWinDataSignals::~GuiWinDataSignals()
{
    DEBUG("~GuiWinDataSignals(%s)", WinName().c_str());
    GuiGlobal::SaveObj(WinName() + ".GuiWinDataSignals", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSignals::_ProcessData(const DataPtr& data)
{
    if (data->type_ == DataType::EPOCH) {
        epoch_ = DataPtrToDataEpochPtr(data);
        newEpochAvail_ = true;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSignals::_Loop(const Time& /*now*/)
{
    if (newEpochAvail_) {
        _UpdateSignals();
        newEpochAvail_ = false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSignals::_ClearData()
{
    epoch_.reset();
    cnoSky_.Clear();
    _UpdateSignals();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSignals::_DrawToolbar()
{
    Gui::VerticalSeparator();

    if (Gui::ToggleButton(ICON_FK_CIRCLE_O "###OnlyTracked", ICON_FK_DOT_CIRCLE_O "###OnlyTracked", &cfg_.onlyTracked,
        "Showing only tracked signals", "Showing all signals", GUI_VAR.iconSize)) {
            // _UpdateSignals();
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(!doSky_);
    Gui::ToggleButton(ICON_FK_ARROWS_H, ICON_FK_ARROWS_V, &cfg_.sizingMax, "Sizing: max h/v",
        "Sizing: min h/v", GUI_VAR.iconSize);
    ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSignals::_DrawContent()
{
    doSky_ = false;
    doList_ = false;

    if (tabbarMode_.Begin()) {
        if (tabbarMode_.Item("List")) {
            doList_ = true;
        };
        if (tabbarMode_.Item("Sky")) {
            doSky_ = true;
        };
        tabbarMode_.End();
    }

    ImGui::SameLine();

    const int filterIx = filterIx_;

    if (tabbarFilter_.Begin()) {
        for (auto& f : FILTERS) {
            if (tabbarFilter_.Item(counts_[f.ix].label_)) {
                filterIx_ = f.ix;
            }
        }
        tabbarFilter_.End();
    }

    if (filterIx != filterIx_) {
        _UpdateSignals();
    }

    if (doSky_) {
        _DrawSky();
    } else if (doList_) {
        table_.DrawTable();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ void GuiWinDataSignals::DrawSignalLevelBar(const SigInfo& sig)
{
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float barMaxLen = 100.0f;  // [px]
    const float barMaxCno = 55.0f;   // [dbHz]
    const Vec2f bar0 = ImGui::GetCursorScreenPos();
    const Vec2f barSize = ImVec2(barMaxLen, GUI_VAR.charSizeY);
    ImGui::Dummy(barSize);
    const float barLen = sig.cno * barMaxLen / barMaxCno;
    const Vec2f bar1 = bar0 + Vec2f(barLen, barSize.y);
    draw->AddRectFilled(bar0, bar1, sig.anyUsed ? CNO_COLOUR(sig.cno) : C_SIGNAL_UNUSED());
    const float offs42 = 42.0 * barMaxLen / barMaxCno;
    draw->AddLine(bar0 + Vec2f(offs42, 0), bar0 + Vec2f(offs42, barSize.y), C_C_GREY());
}

/*static*/ void GuiWinDataSignals::_DrawSignalLevelCb(const void* arg)
{
    const SigInfo* sig = (const SigInfo*)arg;
    ImGui::Text("%4.1f", sig->cno);
    ImGui::SameLine();
    DrawSignalLevelBar(*sig);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSignals::_DrawSky()
{
    ImGui::BeginChild("##Plot", ImVec2(0.0f, 0.0f), false,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Canvas
    const Vec2f canvasOffs = ImGui::GetCursorScreenPos();
    const Vec2f canvasSize = ImGui::GetContentRegionAvail();
    const Vec2f canvasCent{ canvasOffs.x + std::floor(canvasSize.x * 0.5f),
        canvasOffs.y + std::floor(canvasSize.y * 0.5f) };

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(canvasOffs, canvasOffs + canvasSize);

    const float radiusPx = ((cfg_.sizingMax ? std::max(canvasSize.x, canvasSize.y) : std::min(canvasSize.x, canvasSize.y)) / 2.0f) - (2 * GUI_VAR.charSizeX);

    // Draw grid
    {
        // Elevation circles
        int ix = 0;
        const int rm = cnoSky_.rs_.size() / 3;
        for (const float r : cnoSky_.rs_) {
            draw->AddCircle(canvasCent, radiusPx * r, (ix++ % rm) == 0 ? C_PLOT_GRID_MAJOR() : C_PLOT_GRID_MINOR());
        }
        // Azimuth lines
        ix = 0;
        const int ra = cnoSky_.axys_.size() / 4;
        for (const auto& xy : cnoSky_.axys_) {
            draw->AddLine(
                canvasCent, canvasCent + (xy * radiusPx), (ix++ % ra) == 0 ? C_PLOT_GRID_MAJOR() : C_PLOT_GRID_MINOR());
        }
    }

    // Draw bins
    for (auto& bin : cnoSky_.bins_) {
        if (std::isfinite(bin.mean_[filterIx_])) {
            draw->PathClear();
            draw->PathArcTo(canvasCent, radiusPx * bin.r0_, bin.a0_, bin.a1_);
            draw->PathArcTo(canvasCent, radiusPx * bin.r1_, bin.a1_, bin.a0_);
            draw->PathFillConvex(CNO_COLOUR(bin.mean_[filterIx_]));
        }
    }

    // Tooltips and bin highlight
    if (ImGui::IsWindowHovered()) {
        // Is circle hovered?
        const Vec2f mouse = ImGui::GetMousePos();
        const Vec2f dxy = mouse - canvasCent;
        const float dxy2 = (dxy.x * dxy.x) + (dxy.y * dxy.y);
        const float r2 = radiusPx * radiusPx;
        if (dxy2 < r2) {
            // Determine which bin is hovered
            const float a = std::atan2(dxy.y, dxy.x);
            const int azim = ((int)std::floor(a * (180.0f / M_PI) + 0.5f) + (a < 0.0f ? 360 : 0) + 90) % 360;
            const int elev = 90 - (int)std::floor((90.0f * std::sqrt(dxy2) / std::sqrt(r2)) + 0.5f);
            const auto binIx = AZEL_TO_BIN(azim, elev);
            if (binIx < cnoSky_.bins_.size()) {
                auto& bin = cnoSky_.bins_[binIx];
                if (bin.count_[filterIx_] > 0) {
                    ImGui::SetCursorScreenPos(mouse);
                    ImGui::Dummy(ImVec2(1, 1));
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(bin.Tooltip(filterIx_).c_str());
                    ImGui::EndTooltip();

                    draw->PathClear();
                    draw->PathArcTo(canvasCent, radiusPx * bin.r0_, bin.a0_, bin.a1_);
                    draw->PathArcTo(canvasCent, radiusPx * bin.r1_, bin.a1_, bin.a0_);
                    draw->PathStroke(C_C_GREY(), ImDrawFlags_Closed, 3);
                }
            }
        }
    }

    draw->PopClipRect();

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSignals::_UpdateSignals()
{
    counts_.fill({});

    if (epoch_) {
        table_.ClearRows();

        // Update counts, populate list of signals considering filters
        for (auto& sig : epoch_->epoch_.sigs) {
            // Skip untracked signals in table
            if (cfg_.onlyTracked && (sig.use < SigUse::ACQUIRED)) {
                continue;
            }

            // Update counts
            for (auto& f : FILTERS) {
                if ((f.ix == 0) || (sig.gnss == f.gnss) || (sig.band == f.band)) {
                    counts_[f.ix].tot_++;
                    if (sig.use >= SigUse::ACQUIRED) {
                        counts_[f.ix].trk_++;
                    }
                    if (sig.anyUsed) {
                        counts_[f.ix].nav_++;
                    }
                }
            }

            // Update sky
            if ((sig.cno > 0.0f) && (sig.satIx >= 0)) {
                const auto& sat = epoch_->epoch_.sats[sig.satIx];
                const auto binIx = AZEL_TO_BIN(sat.azim, sat.elev);
                if (binIx < cnoSky_.bins_.size()) {
                    cnoSky_.bins_[binIx].Add(sig);
                }
            }

            // Apply filter for table
            const auto filterGnss = FILTERS[filterIx_].gnss;
            const auto filterBand = FILTERS[filterIx_].band;
            if (((filterGnss != Gnss::UNKNOWN) && (sig.gnss != filterGnss)) ||
                ((filterBand != Band::UNKNOWN) && (sig.band != filterBand))) {
                continue;
            }

            // Populate table
            // const uint32_t thisSat = (sig.gnss << 16) | (sig.sv << 8); // <use><gnss><sat><sig>
            const uint32_t rowUid = sig.satSig.id_;
            // default sort key: sort by gnss/sat/sig, and used signals first, searching signals at the end
            const uint32_t rowSort = rowUid | (sig.use > SigUse::SEARCH ? 0x40000000 : 0x80000000);

            table_.AddCellText(sig.satStr, sig.sat.id_);
            table_.AddCellText(sig.signalStr, EnumToVal(sig.signal));
            table_.AddCellText(sig.bandStr, EnumToVal(sig.band));
            if (sig.use > SigUse::SEARCH) {
                table_.AddCellCb(&GuiWinDataSignals::_DrawSignalLevelCb, &sig);

            } else {
                table_.AddCellText("-");
            }
            table_.SetCellSort(sig.cno > 0.0f ? 1 + (sig.cno * 100) : 0xffffffff);

            table_.AddCellText(sig.useStr, EnumToVal(sig.use));
            table_.AddCellText(sig.ionoStr);
            table_.AddCellText(sig.healthStr);
            table_.AddCellTextF(
                "%s %s %s", sig.prUsed ? "PR" : "--", sig.crUsed ? "CR" : "--", sig.doUsed ? "DO" : "--");

            const uint32_t prResSort = (sig.prUsed ? std::abs(sig.prRes) * 1000.0f : 9999999);
            if (sig.prUsed /*sig->prRes != 0.0*/) {
                table_.AddCellTextF("%6.1f", sig.prRes);
                table_.SetCellSort(prResSort);
            } else {
                table_.AddCellText("-", prResSort);
            }
            table_.AddCellTextF("%-9s %s %s %s", sig.corrStr, sig.prCorrUsed ? "PR" : "--",
                sig.crCorrUsed ? "CR" : "--", sig.doCorrUsed ? "DO" : "--");

            if (sig.use <= SigUse::SEARCH) {
                table_.SetRowColour(C_SIGNAL_UNUSED_TEXT());
            } else if (sig.anyUsed) {
                table_.SetRowColour(C_SIGNAL_USED_TEXT());
            }

            table_.SetRowSort(rowSort);
            table_.SetRowUid(rowUid);
        }
    }

    // Update tab labels
    for (auto& f : FILTERS) {  // clang-format off
        counts_[f.ix].label_ = (counts_[f.ix].tot_ > 0 ?
            Sprintf("%s (%d/%d)###%s", f.label, counts_[f.ix].nav_, counts_[f.ix].tot_, f.label) : Sprintf("%s###%s", f.label, f.label));
    }  // clang-format on
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataSignals::CnoBin::Add(const SigInfo& sig)
{
    for (auto& f : FILTERS) {
        if ((f.ix == 0) || (sig.gnss == f.gnss) || (sig.band == f.band)) {
            if (std::isnan(max_[f.ix]) || (sig.cno > max_[f.ix])) {
                max_[f.ix] = sig.cno;
            }
            if (std::isnan(min_[f.ix]) || (sig.cno < min_[f.ix])) {
                min_[f.ix] = sig.cno;
            }
            count_[f.ix]++;
            if (std::isnan(mean_[f.ix])) {
                mean_[f.ix] = sig.cno;
            } else {
                mean_[f.ix] += (sig.cno - mean_[f.ix]) / (float)count_[f.ix];
            }
            tooltips_[f.ix].clear();
        }
    }
}

const std::string& GuiWinDataSignals::CnoBin::Tooltip(const int ix)
{
    if (tooltips_[ix].empty()) {
        tooltips_[ix] = Sprintf("%.1f dBHz (min %.1f, max %.1f, n=%u)", mean_[ix], min_[ix], max_[ix], count_[ix]);
    }
    return tooltips_[ix];
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWinDataSignals::CnoSky::CnoSky()
{
    // Bins arrangement:
    // - clockwise (0 deg = east, 90 deg = south, 180 deg = west, 270 deg = north
    // - from out (0 deg elevation) to in (90 deg elevation)
    //                    (270)                         .
    //                                                  .
    //                , - ~ ~ ~ - ,                     .
    //            , '   \ __4__ /    ' ,                .
    //          ,      , \     / ',  5  ,               .
    //         ,   3 ,    \ 10/    '     ,              .
    //        ,     ,   9  \ / 11   r1    r0            .
    // (180)  ,-------------o--------,----,--- a0   (0) .
    //        ,     ,   8  / \  6   ,#####,             .
    //         ,     ',   / 7 \    ,##0##,              .
    //          ,   2  ' /,___,\ ,######,               .
    //            ,     /   1   \####, '                .
    //              ' - , _ _ _ ,\ '         a = angle  .
    //                            \          r = radius .
    //                             a1                   .
    //                    (90)                          .

    // Calculate bins and grid
    // - Elevation circles
    // clang-format off
    for (std::size_t el = 0; el < SKY_NUM_BINS_EL; el++)
    {
        const float r0 = (1.0f - ((el       * (90.0f / (float)SKY_NUM_BINS_EL) / 90.0f)));
        const float r1 = (1.0f - (((el + 1) * (90.0f / (float)SKY_NUM_BINS_EL) / 90.0f)));
        rs_[el] = r0; // Radii for grid circles

        // - Azimuth lines
        for (std::size_t az = 0; az < SKY_NUM_BINS_AZ; az++)
        {
            const float a0 = (float)az       * (360.0f / (float)SKY_NUM_BINS_AZ) * (M_PI / 180.f);
            const float a1 = (float)(az + 1) * (360.0f / (float)SKY_NUM_BINS_AZ) * (M_PI / 180.f);
            if (el == 0) { axys_[az] = { std::cos(a0), std::sin(a0) }; } // x/y (from angles) for grid lines

            // Add bin
            auto& bin = bins_[(el * SKY_NUM_BINS_AZ) + az];
            bin.r0_ = r0;
            bin.r1_ = r1;
            bin.a0_ = a0;
            bin.a1_ = a1;
            const float ar = 0.5f * (r0 + r1);
            const float aa = 0.5f * (a0 + a1);
            bin.xy_.x = ar * std::cos(aa);
            bin.xy_.y = ar * std::sin(aa);
            // bin.uid_ = Sprintf("%016" PRIxMAX, reinterpret_cast<std::uintptr_t>(&bin));
        }
    }
    // clang-format on
}

void GuiWinDataSignals::CnoSky::Clear()
{
    for (auto& bin : bins_) {
        bin.count_.fill(0);
        bin.min_.fill(NAN);
        bin.max_.fill(NAN);
        bin.mean_.fill(NAN);
        bin.tooltips_.fill("");
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
