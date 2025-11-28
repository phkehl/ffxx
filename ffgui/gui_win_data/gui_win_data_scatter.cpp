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
#include <fpsdk_common/trafo.hpp>
//
#include "gui_win_data_scatter.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::trafo;

GuiWinDataScatter::GuiWinDataScatter(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 65, 35, 40, 10 }, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse, input)  // clang-format on
{
    DEBUG("GuiWinDataScatter(%s)", WinName().c_str());

    toolbarClearEna_ = false;

    GuiGlobal::LoadObj(WinName() + ".GuiWinDataScatter", cfg_);

    ClearData();
}

GuiWinDataScatter::~GuiWinDataScatter()
{
    DEBUG("~GuiWinDataScatter(%s)", WinName().c_str());
    GuiGlobal::SaveObj(WinName() + ".GuiWinDataScatter", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataScatter::_ProcessData(const DataPtr& /*data*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataScatter::_Loop(const Time& /*now*/)
{
    if (input_->database_->Changed(this)) {
        _Update();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataScatter::_ClearData()
{
    dbInfo_ = {};
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataScatter::_DrawToolbar()
{
    switch (input_->database_->GetRefPos()) {
        case Database::RefPos::MEAN:
            if (ImGui::Button(ICON_FK_DOT_CIRCLE_O "###RefPos", GUI_VAR.iconSize)) {
                input_->database_->SetRefPosLast();
            }
            Gui::ItemTooltip("Reference position is mean position");
            break;
        case Database::RefPos::LAST:
            if (ImGui::Button(ICON_FK_CHECK_CIRCLE_O "###RefPos", GUI_VAR.iconSize)) {
                input_->database_->SetRefPosMean();
            }
            Gui::ItemTooltip("Reference position is last position");
            break;
        case Database::RefPos::USER:
            if (ImGui::Button(ICON_FK_CIRCLE_O "###RefPos", GUI_VAR.iconSize)) {
                input_->database_->SetRefPosMean();
            }
            Gui::ItemTooltip("Reference position is user position");
            break;
    }

    ImGui::SameLine();

    // Show statistics?
    Gui::ToggleButton(ICON_FK_ARROWS_H, ICON_FK_ARROWS_V, &cfg_.sizingMax, "Sizing: max h/v",
        "Sizing: min h/v", GUI_VAR.iconSize);

    ImGui::SameLine();

    // Fit data
    if (ImGui::Button(ICON_FK_ARROWS_ALT "###Fit", GUI_VAR.iconSize)) {
        triggerSnapRadius_ = true;
    }
    Gui::ItemTooltip("Fit all points");

    ImGui::SameLine();

    // Error ellipses
    showingErrorEll_ = (cfg_.sigmaEna[0] || cfg_.sigmaEna[1] || cfg_.sigmaEna[2] || cfg_.sigmaEna[3] ||
                        cfg_.sigmaEna[4] || cfg_.sigmaEna[5]);
    if (Gui::ToggleButton(
            ICON_FK_PERCENT "###ErrEll", NULL, &showingErrorEll_, "Error ellipses", NULL, GUI_VAR.iconSize)) {
        ImGui::OpenPopup("ErrEllSigma");
    }
    if (ImGui::BeginPopup("ErrEllSigma")) {
        for (std::size_t ix = 0; ix < cfg_.sigmaEna.size(); ix++) {
            ImGui::Checkbox(SIGMA_LABELS[ix], &cfg_.sigmaEna[ix]);
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // Show statistics?
    Gui::ToggleButton(ICON_FK_CROP "###ShowStats", NULL, &cfg_.showStats, "Showing statistics",
        "Not showing statistics", GUI_VAR.iconSize);

    ImGui::SameLine();

    // Histogram
    Gui::ToggleButton(ICON_FK_BAR_CHART "##Histogram", NULL, &cfg_.showHistogram, "Showing histogram",
        "Not showing histogram", GUI_VAR.iconSize);

    ImGui::SameLine();

    // Accuracy estimate circle
    Gui::ToggleButton(ICON_FK_CIRCLE "###AccEst", NULL, &cfg_.showAccEst, "Showing accuracy estimate (2d)",
        "Not showing accuracy estimate (2d)", GUI_VAR.iconSize);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataScatter::_DrawContent()
{
    ImGui::BeginChild("##Plot", ImVec2(0.0f, 0.0f), false,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Canvas
    const Vec2f offs = ImGui::GetCursorScreenPos();
    const Vec2f size = ImGui::GetContentRegionAvail();
    const Vec2f cent = ImVec2(offs.x + std::floor(size.x * 0.5), offs.y + std::floor(size.y * 0.5));
    const Vec2f cursorOrigin = ImGui::GetCursorPos();
    const Vec2f cursorMax = cursorOrigin + ImGui::GetContentRegionAvail();

    constexpr float minRadiusM = 0.02;
    constexpr float maxRadiusM = 10000.0;

    Eigen::Vector3d refPosLlh;
    Eigen::Vector3d refPosXyz;
    const auto refPos = input_->database_->GetRefPos(refPosLlh, refPosXyz);

    const float lineHeight = ImGui::GetTextLineHeight();
    const float radiusM = cfg_.plotRadius;
    auto& io = ImGui::GetIO();

    // Draw scatter plot (draw list stuff is in screen coordinates)
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float radiusPx = ((cfg_.sizingMax ? std::max(size.x, size.y) : std::min(size.x, size.y)) / 2) - (3 * GUI_VAR.charSizeX);
    const float m2px = radiusM > FLT_EPSILON ? radiusPx / radiusM : 1.0f;
    const float px2m = radiusPx > FLT_EPSILON ? radiusM / radiusPx : 1.0f;
    const float top = offs.y;
    const float bot = offs.y + size.y;
    const float left = offs.x;
    const float right = offs.x + size.x;
    draw->PushClipRect(offs, offs + size);

    // Draw histogram (will lag one epoch, but we want to draw it below the points...)
    if (cfg_.showHistogram && (histNumPoints_ > 0)) {
        const float barSep = 2 * radiusPx / (float)NUM_HIST;
        const float barLen = 0.5 * radiusPx;
        const float norm = barLen / (float)histNumPoints_;
        const Vec2f pos0x = cent + Vec2f(-radiusPx, -radiusPx); // left -> right
        const Vec2f pos0y = cent + Vec2f(-radiusPx, radiusPx - barSep); // botton -> up
        for (std::size_t ix = 0; ix < NUM_HIST; ix++) {
            {
                const float dx = (float)ix * barSep;
                const float dy = 1.0 + ((float)histogramE_[ix] * norm);
                draw->AddRectFilled(pos0x + Vec2f(dx, 0), pos0x + Vec2f(dx + barSep, dy), C_PLOT_HISTOGRAM());
            }
            {
                const float dx = 1.0 + ((float)histogramN_[ix] * norm);
                const float dy = -(float)ix * barSep;
                draw->AddRectFilled(pos0y + Vec2f(0, dy), pos0y + Vec2f(dx, dy + barSep), C_PLOT_HISTOGRAM());
            }
        }
    }

    // Draw points (and update histogram)
    histogramN_.fill(0);
    histogramE_.fill(0);
    histNumPoints_ = 0;
    bool havePoints = false;
    input_->database_->ProcRows([&](const Database::Row& row) {
        if (row.pos_avail) {
            const float east = row.pos_enu_ref_east;
            const float north = row.pos_enu_ref_north;
            const float dx = std::floor((east * m2px) + 0.5);
            const float dy = -std::floor((north * m2px) + 0.5);
            draw->AddRectFilled(cent + ImVec2(dx - 2, dy - 2), cent + ImVec2(dx + 2, dy + 2), row.fix_colour);
            havePoints = true;
            histNumPoints_++;

            if (cfg_.showHistogram) {
                const int binE = (radiusM + east) / (2.0f * cfg_.plotRadius) * (float)NUM_HIST;
                histogramE_[binE < 0 ? 0 : (binE > (int)(NUM_HIST - 1) ? (NUM_HIST - 1) : binE)]++;
                const int binN = (radiusM + north) / (2.0f * cfg_.plotRadius) * (float)NUM_HIST;
                histogramN_[binN < 0 ? 0 : (binN > (int)(NUM_HIST - 1) ? (NUM_HIST - 1) : binN)]++;
            }
        }
        return true;
    });

    // Draw grid
    {
        draw->AddLine(ImVec2(offs.x, cent.y), ImVec2(offs.x + size.x, cent.y), C_PLOT_GRID_MAJOR());
        draw->AddLine(ImVec2(cent.x, offs.y), ImVec2(cent.x, offs.y + size.y), C_PLOT_GRID_MAJOR());
        draw->AddCircle(cent, radiusPx * 1.00f, C_PLOT_GRID_MAJOR(), 0);
        draw->AddCircle(cent, radiusPx * 0.75f, C_PLOT_GRID_MINOR(), 0);
        draw->AddCircle(cent, radiusPx * 0.50f, C_PLOT_GRID_MAJOR(), 0);
        draw->AddCircle(cent, radiusPx * 0.25f, C_PLOT_GRID_MINOR(), 0);
    }

    // Label grid
    const float pos[] = { -1.0, -0.5, 0.5, 1.0 };
    ImGui::PushStyleColor(ImGuiCol_Text, C_PLOT_GRID_LABEL());
    for (std::size_t ix = 0; ix < NumOf(pos); ix++) {
        char label[20];
        float labelOffs = -0.2;
        if (radiusM > 49.9) {
            std::snprintf(label, sizeof(label), "%+.0f", pos[ix] * radiusM);
        } else if (radiusM > 0.19) {
            std::snprintf(label, sizeof(label), "%+.1f", pos[ix] * radiusM);
            labelOffs = 1.5;
        } else {
            std::snprintf(label, sizeof(label), "%+.2f", pos[ix] * radiusM);
            labelOffs = 2.5;
        }

        // std::snprintf(label, sizeof(label), radiusM < 0.2 ? "%+.2f" : "%+.1f", pos[ix] * radiusM);
        const float xOffs = ((float)strlen(label) - labelOffs) * GUI_VAR.charSizeX;
        // x axis
        ImVec2 cursor = cent + ImVec2((radiusPx * pos[ix]) - xOffs, -GUI_VAR.charSizeY - 2);
        ImGui::SetCursorScreenPos(cursor);
        ImGui::TextUnformatted(label);
        // y axis
        cursor = cent + ImVec2(-xOffs, radiusPx * -pos[ix]);
        if (pos[ix] > 0) {
            cursor.y -= GUI_VAR.charSizeY + 2;
        } else {
            cursor.y += 2;
        }
        ImGui::SetCursorScreenPos(cursor);
        ImGui::TextUnformatted(label);
    }
    ImGui::PopStyleColor();

    // Draw min/max/mean
    if (cfg_.showStats && havePoints) {
        const float minEastX = cent.x + std::floor((dbInfo_.stats.pos_enu_ref_east.min * m2px) + 0.5);
        const float maxEastX = cent.x + std::floor((dbInfo_.stats.pos_enu_ref_east.max * m2px) + 0.5);
        draw->AddLine(ImVec2(minEastX, top), ImVec2(minEastX, bot), C_PLOT_STATS_MINMAX());
        draw->AddLine(ImVec2(maxEastX, top), ImVec2(maxEastX, bot), C_PLOT_STATS_MINMAX());
        const float minNorthY = cent.y - std::floor((dbInfo_.stats.pos_enu_ref_north.min * m2px) + 0.5);
        const float maxNorthY = cent.y - std::floor((dbInfo_.stats.pos_enu_ref_north.max * m2px) + 0.5);
        draw->AddLine(ImVec2(left, minNorthY), ImVec2(right, minNorthY), C_PLOT_STATS_MINMAX());
        draw->AddLine(ImVec2(left, maxNorthY), ImVec2(right, maxNorthY), C_PLOT_STATS_MINMAX());
        const float meanEastX = cent.x + std::floor((dbInfo_.stats.pos_enu_ref_east.mean * m2px) + 0.5);
        const float meanNorthY = cent.y - std::floor((dbInfo_.stats.pos_enu_ref_north.mean * m2px) + 0.5);
        if (refPos != Database::RefPos::MEAN) {
            draw->AddLine(ImVec2(meanEastX, top), ImVec2(meanEastX, bot), C_PLOT_STATS_MEAN());
            draw->AddLine(ImVec2(left, meanNorthY), ImVec2(right, meanNorthY), C_PLOT_STATS_MEAN());
        }

        ImGui::PushStyleColor(ImGuiCol_Text, C_PLOT_STATS_MINMAX());
        char label[20];

        std::snprintf(label, sizeof(label), "%+.3f", dbInfo_.stats.pos_enu_ref_east.min);
        ImGui::SetCursorScreenPos(ImVec2(minEastX - (GUI_VAR.charSizeX * strlen(label)) - 2, top));
        ImGui::TextUnformatted(label);

        std::snprintf(label, sizeof(label), "%+.3f", dbInfo_.stats.pos_enu_ref_east.max);
        ImGui::SetCursorScreenPos(ImVec2(maxEastX + 2, top));
        ImGui::TextUnformatted(label);

        std::snprintf(label, sizeof(label), "%+.3f", dbInfo_.stats.pos_enu_ref_north.min);
        ImGui::SetCursorScreenPos(ImVec2(left, minNorthY));
        ImGui::TextUnformatted(label);

        std::snprintf(label, sizeof(label), "%+.3f", dbInfo_.stats.pos_enu_ref_north.max);
        ImGui::SetCursorScreenPos(ImVec2(left, maxNorthY - lineHeight));
        ImGui::TextUnformatted(label);

        ImGui::PopStyleColor();

        // Show mean East-North in case centre of plot isn't the mean
        if (refPos != Database::RefPos::MEAN) {
            ImGui::PushStyleColor(ImGuiCol_Text, C_PLOT_STATS_MEAN());

            std::snprintf(label, sizeof(label), "%+.3f", dbInfo_.stats.pos_enu_ref_east.mean);
            ImGui::SetCursorScreenPos(ImVec2(meanEastX + 2, top + lineHeight));
            ImGui::TextUnformatted(label);

            std::snprintf(label, sizeof(label), "%+.3f", dbInfo_.stats.pos_enu_ref_north.mean);
            ImGui::SetCursorScreenPos(ImVec2(left + 2 + (GUI_VAR.charSizeX * 6), meanNorthY - lineHeight));
            ImGui::TextUnformatted(label);

            ImGui::PopStyleColor();
        }
    }

    // Draw error ellipses
    if (!std::isnan(dbInfo_.err_ell.a)) {
        for (std::size_t sIx = 0; sIx < cfg_.sigmaEna.size(); sIx++) {
            if (!cfg_.sigmaEna[sIx]) {
                continue;
            }

            ImVec2 points[100];
            const double cosOmega = std::cos(dbInfo_.err_ell.omega);
            const double sinOmega = std::sin(dbInfo_.err_ell.omega);
            const double x0 = dbInfo_.stats.pos_enu_ref_east.mean * m2px;
            const double y0 = dbInfo_.stats.pos_enu_ref_north.mean * m2px;
            const double scale = SIGMA_SCALES[sIx];
            const double a = scale * dbInfo_.err_ell.a;
            const double b = scale * dbInfo_.err_ell.b;
            for (std::size_t ix = 0; ix < NumOf(points); ix++) {
                const double t = (double)ix * (2 * M_PI / (double)NumOf(points));
                const double cosT = std::cos(t);
                const double sinT = std::sin(t);
                const double dx = (a * cosT * cosOmega) - (b * sinT * sinOmega);
                const double dy = (a * cosT * sinOmega) - (b * sinT * cosOmega);
                points[ix].x = cent.x + std::floor(x0 + (dx * m2px) + 0.5);
                points[ix].y = cent.y - std::floor(y0 + (dy * m2px) + 0.5);
            }
            draw->AddPolyline(points, NumOf(points), C_PLOT_ERR_ELL(), true, 3.0f);
        }
    }

    // Highlight last point, draw accuracy estimate circle
    input_->database_->ProcRows(
        [&](const Database::Row& row) {
            if (!std::isnan(row.pos_enu_ref_east)) {
                const float east = row.pos_enu_ref_east;
                const float north = row.pos_enu_ref_north;
                const float dx = std::floor((east * m2px) + 0.5);
                const float dy = -std::floor((north * m2px) + 0.5);
                const ImU32 c = row.fix_ok ? C_PLOT_FIX_HL_OK() : C_PLOT_FIX_HL_MASKED();
                const double age = (Time::FromClockRealtime() - row.time_in).GetSec();
                const float w = age < 0.1 ? 3.0 : 1.0;
                const float l = 20.0;  // age < 100 ? 40.0 : 20.0; //20 + ((100 - std::min(age, 100)) / 2);
                if (cfg_.showAccEst) {
                    draw->AddCircleFilled(
                        cent + ImVec2(dx, dy), (0.5 * row.pos_acc_horiz * m2px) + 0.5, C_PLOT_MAP_ACC_EST());
                }
                draw->AddLine(cent + ImVec2(dx - l, dy), cent + ImVec2(dx - 3, dy), c, w);
                draw->AddLine(cent + ImVec2(dx + 3, dy), cent + ImVec2(dx + l, dy), c, w);
                draw->AddLine(cent + ImVec2(dx, dy - l), cent + ImVec2(dx, dy - 3), c, w);
                draw->AddLine(cent + ImVec2(dx, dy + 3), cent + ImVec2(dx, dy + l), c, w);
                return false;
            }
            return true;
        },
        true);

    draw->PopClipRect();

    // Place an invisible button on top of everything to capture mouse events (and disable windows moving)
    // Note the real buttons above must are placed first so that they will get the mouse events first (before this
    // invisible button)
    ImGui::SetCursorPos(cursorOrigin);
    ImGui::InvisibleButton("canvas", size, ImGuiButtonFlags_MouseButtonLeft);
    const bool isHovered = ImGui::IsItemHovered();
    const bool isActive = ImGui::IsItemActive();

    // Dragging
    if (isActive && havePoints) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            refPosLlhDragStart_ = refPosLlh;
            refPosXyzDragStart_ = refPosXyz;
        } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            Vec2f totalDrag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            if ((totalDrag.x != 0.0) || (totalDrag.y != 0.0)) {
                Vec2f totalDragM = totalDrag * px2m;
                const Eigen::Vector3d newXyz = TfEcefEnu({ -totalDragM.x, totalDragM.y, 0 }, refPosLlhDragStart_);
                Eigen::Vector3d newLlh = TfWgs84LlhEcef(newXyz);  // xyz2llh
                newLlh[2] = refPosLlh[2];                         // keep height
                input_->database_->SetRefPosLlh(newLlh);
            }
        }
    }

    // Crosshairs
    if (isHovered) {
        draw->AddLine(ImVec2(io.MousePos.x, top), ImVec2(io.MousePos.x, bot), C_PLOT_FIX_CROSSHAIRS());
        draw->AddLine(ImVec2(left, io.MousePos.y), ImVec2(right, io.MousePos.y), C_PLOT_FIX_CROSSHAIRS());

        const auto delta = (cent - io.MousePos) * (Vec2f(-1.0f, 1.0f) * px2m);
        ImGui::PushStyleColor(ImGuiCol_Text, C_PLOT_FIX_CROSSHAIRS_LABEL());
        char label[20];

        std::snprintf(label, sizeof(label), "%+.3f", delta.x);
        ImGui::SetCursorScreenPos(ImVec2(io.MousePos.x + 2, top + (cfg_.showStats ? (2 * lineHeight) : 0)));
        ImGui::TextUnformatted(label);

        std::snprintf(label, sizeof(label), "%+.3f", delta.y);
        ImGui::SetCursorScreenPos(
            ImVec2(left + 2 + (cfg_.showStats ? (GUI_VAR.charSizeX * 13) : 0), io.MousePos.y - lineHeight));
        ImGui::TextUnformatted(label);

        ImGui::PopStyleColor();
    }

    // Print centre position
    ImGui::SetCursorPosY(cursorMax.y - lineHeight);
    if (havePoints) {
        ImGui::Text("%+.9f %+.9f %+.3f", RadToDeg(refPosLlh[0]), RadToDeg(refPosLlh[1]), refPosLlh[2]);
    } else {
        ImGui::TextUnformatted("no data");
    }

    if (triggerSnapRadius_) {
        cfg_.plotRadius = std::min(
            std::max(dbInfo_.stats.pos_enu_ref_east.max, dbInfo_.stats.pos_enu_ref_north.max), (double)maxRadiusM);
    }

    // Range control using mouse wheel (but not while dragging FIXME: wh not?)
    if ((isHovered && !isActive) || triggerSnapRadius_) {
        if ((io.MouseWheel != 0.0) || triggerSnapRadius_) {
            float delta = 0.0;
            // Zoom in
            if (io.MouseWheel > 0) {  // clang-format off
                if      (cfg_.plotRadius <  0.3)   { delta =  -0.02; }
                else if (cfg_.plotRadius <  1.1)   { delta =  -0.2;  }
                else if (cfg_.plotRadius < 20.1)   { delta =  -1.0;  }
                else if (cfg_.plotRadius < 50.1)   { delta =  -5.0;  }
                else if (cfg_.plotRadius < 1000.1) { delta =  -10.0;  }
                else                               { delta = -100.0;  }
            }  // clang-format on
            // Zoom out
            else {  // clang-format off
                if      (cfg_.plotRadius > 999.9)  { delta = 100.0;  }
                else if (cfg_.plotRadius >  49.9)  { delta =  10.0;  }
                else if (cfg_.plotRadius >  19.9)  { delta =   5.0;  }
                else if (cfg_.plotRadius >   0.9)  { delta =   1.0;  }
                else if (cfg_.plotRadius >   0.19) { delta =   0.2;  }
                else                           { delta =   0.02; }
            }  // clang-format on
            if (io.KeyCtrl) {
                delta *= 2.0;
            }
            if (triggerSnapRadius_) {
                cfg_.plotRadius = std::floor((cfg_.plotRadius + (1.0 * delta)) / delta) * delta;
                triggerSnapRadius_ = false;
            } else {
                cfg_.plotRadius = std::floor((cfg_.plotRadius + (1.5 * delta)) / delta) * delta;
            }
            if (cfg_.plotRadius < minRadiusM) {
                cfg_.plotRadius = minRadiusM;
            } else if (cfg_.plotRadius > maxRadiusM) {
                cfg_.plotRadius = maxRadiusM;
            }
        }
        if (havePoints) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_None /*ImGuiMouseCursor_ResizeAll*/);
        }
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataScatter::_Update()
{
    dbInfo_ = input_->database_->GetInfo();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
