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
#include "gui_win_multimap.hpp"
#include "input.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinMultimap::GuiWinMultimap(const std::string& name) /* clang-format off */ :
    GuiWin(name, { 80, 25, 0, 0 }, ImGuiWindowFlags_None | ImGuiWindowFlags_NoDocking),
    tabbar_   { WinName() },
    map_      { WinName() }  // clang-format on
{
    DEBUG("GuiWinMultimap(%s)", WinName().c_str());
    GuiGlobal::LoadObj(WinName() + ".GuiWinMultimap", cfg_);

    auto title = WinName();
    StrReplace(title, "_", " ");
    WinSetTitle(title);

    for (const auto& [ n, c ] : cfg_.inputs) {
        states_.emplace(n, c);
    }
}

GuiWinMultimap::~GuiWinMultimap()
{
    DEBUG("~GuiWinMultimap(%s)", WinName().c_str());

    cfg_.inputs.clear();
    for (auto& [ name, state ] : states_) {
        cfg_.inputs.emplace(name, state.cfg_);
    }

    GuiGlobal::SaveObj(WinName() + ".GuiWinMultimap", cfg_);

    RemoveDataObserver(0, this);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinMultimap::_Clear()
{
    points_.clear();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinMultimap::Loop(const Time& /*now*/)
{
    // Maintain list of available inputs, and data observers to their DBs
    for (auto& [name, input] : GetInputs()) {
        if (!input->database_) {
            continue;
        }

        // Get state, create if necessary
        auto entry = states_.find(name);
        if (entry == states_.end()) {
            entry = states_.emplace(name, ConfigInput()).first;
        }

        auto& state = entry->second;
        auto& database = *input->database_;

        // Subscribe to database updates
        if (state.dbSrcId_ != database.GetDataSrcDst()) {
            AddDataObserver(database.GetDataSrcDst(), this, [pState = &entry->second](const DataPtr& data) {
                if (data->type_ == DataType::EVENT) {
                    auto& event = DataPtrToDataEvent(data);
                    if (event.event_ == DataEvent::DBUPDATE) {
                        pState->dbDirty_ = true;
                    }
                }
            });
            state.dbSrcId_ = database.GetDataSrcDst();
        }

        // We have a new point
        if (state.dbDirty_) {
            // DEBUG("%s: db %s update", WinName().c_str(), name.c_str());
            auto& row = database.LatestRow();
            if ((row.fix_type > FixType::NOFIX) && !row.time_fix.IsZero()) {
                static constexpr uint64_t snap = 1000000; // 10ms
                const uint64_t ts = ((row.time_fix.GetNSec() + (snap/2)) / snap) * snap;

                auto pointsEntry = points_.find(ts);
                if (pointsEntry == points_.end()) {
                    pointsEntry = points_.emplace(ts, PointMap()).first;
                }

                pointsEntry->second.emplace(&state, Point(row));
            }

            state.dbDirty_ = false;

            while ((int)points_.size() >= cfg_.numPoints) {
                points_.erase(points_.begin());
            }

        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinMultimap::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }


    if (ImGui::Button(ICON_FK_ERASER, GUI_VAR.iconSize)) {
        _Clear();
    }
    Gui::ItemTooltip("Clear data");

    Gui::VerticalSeparator();

    Gui::ToggleButton(ICON_FK_MINUS_SQUARE_O, ICON_FK_SQUARE_O, &cfg_.baseline, "Showing baselines",
        "Not showing baselines", GUI_VAR.iconSize);

    ImGui::SameLine();

    ImGui::SetNextItemWidth(GUI_VAR.charSizeX * 15.0f);
    ImGui::SliderInt("##NumPoints", &cfg_.numPoints, 1, 10000);
    Gui::ItemTooltip("Number of points shown");

    ImGui::Separator();

    if (ImGui::BeginChild("##Controls", ImVec2(GUI_VAR.charSizeX * 23.0f, 0))) {

        bool haveLogfiles = false;
        bool canPlayAny   = false;
        bool canPauseAny  = false;
        bool canStepAny   = false;

        for (auto& [name, input] : GetInputs()) {
            if (!input->database_) {
                continue;
            }

            ImGui::PushID(input->srcId_);

            auto entry = states_.find(name);
            if (entry == states_.end()) {
                entry = states_.emplace(name, ConfigInput()).first;
            }

            auto& state = entry->second;
            auto& database = *input->database_;

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0,0}); // smaller radio and checkbox

            // Checkbox
            ImGui::Checkbox("##Used", &state.cfg_.used);
            Gui::ItemTooltip(state.cfg_.used ? "Showing points" : "Not showing points");

            ImGui::SameLine();

            ImGui::BeginDisabled(!state.cfg_.used);

            if (ImGui::Button(ICON_FK_DOT_CIRCLE_O)) {
                for (auto it = points_.rbegin(); it != points_.rend(); it++) {
                    auto point = it->second.find(&state);
                    if (point != it->second.end()) {
                        map_.SetPos(point->second.lat_, point->second.lon_);
                        break;
                    }
                }
            }
            Gui::ItemTooltip("Centre map on last point");

            ImGui::SameLine();

            // Colour
            if (ImGui::BeginCombo("##Colour", NULL, ImGuiComboFlags_HeightLarge | ImGuiComboFlags_NoPreview)) {
                for (std::size_t ix = 0; ix < COLOURS.size(); ix++) {
                    if (COLOURS[ix] != CIX_AUTO) { ImGui::PushStyleColor(ImGuiCol_Text, IX_COLOUR(COLOURS[ix])); }
                    if (ImGui::Selectable(Sprintf("%s##%" PRIuMAX, input->name_.c_str(), COLOURS[ix]).c_str())) {
                        state.cfg_.colour = ix;
                    }
                    if (COLOURS[ix] != CIX_AUTO) { ImGui::PopStyleColor(); }
                }
                ImGui::EndCombo();
            }
            Gui::ItemTooltip("Colour for position markers");

            ImGui::PopStyleVar();
            ImGui::SameLine();

            // Name
            const auto colour = state.cfg_.colour;
            if (COLOURS[state.cfg_.colour] != CIX_AUTO) { ImGui::PushStyleColor(ImGuiCol_Text, IX_COLOUR(COLOURS[colour])); }
            ImGui::TextUnformatted(name.c_str());
            if (COLOURS[state.cfg_.colour] != CIX_AUTO) { ImGui::PopStyleColor(); }

            // Fix status
            if (state.cfg_.used) {
                const auto& row = database.LatestRow();
                ImGui::TextColored(FIX_COLOUR4(row.fix_type, row.fix_ok), "Fix: %s", row.fix_str);
                ImGui::TextUnformatted(row.time_str.c_str());
            }

            // Logfile controls
            if (state.cfg_.used && input->logfile_) {
                auto& logfile = *input->logfile_;
                const bool canPlay  = logfile.CanPlay();
                const bool canPause = logfile.CanPause();
                const bool canStep  = logfile.CanStep();

                ImGui::BeginDisabled(!canPlay && !canPause);
                if (canPause) {
                    if (ImGui::Button(ICON_FK_PAUSE "##PlayPause", GUI_VAR.iconSize)) {
                        logfile.Pause();
                    }
                } else {
                    if (ImGui::Button(ICON_FK_PLAY "##PlayPause", GUI_VAR.iconSize)) {
                        logfile.Play();
                    }
                }
                Gui::ItemTooltip(canPause ? "Pause" : "Play");
                ImGui::EndDisabled();

                ImGui::SameLine();

                ImGui::BeginDisabled(!canStep);
                ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
                if (ImGui::Button(ICON_FK_FORWARD "##StepEpoch", GUI_VAR.iconSize)) {
                    logfile.StepEpoch();
                }
                ImGui::PopItemFlag();
                Gui::ItemTooltip("Step epoch");
                ImGui::EndDisabled();

                ImGui::SameLine();

                ImGui::BeginDisabled(!canStep);
                ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
                if (ImGui::Button(ICON_FK_STEP_FORWARD "##StepMsg", GUI_VAR.iconSize)) {
                    logfile.StepMsg();
                }
                ImGui::PopItemFlag();
                Gui::ItemTooltip("Step message");
                ImGui::EndDisabled();


                haveLogfiles = true;
                if (canPlay)  { canPlayAny  = true; }
                if (canPause) { canPauseAny = true; }
                if (canStep)  { canStepAny  = true; }
            }

            ImGui::EndDisabled();

            ImGui::PopID();
            ImGui::Separator();
        }

        if (haveLogfiles) {

            ImGui::BeginDisabled(!canPlayAny && !canPauseAny);
            if (canPauseAny) {
                if (ImGui::Button(ICON_FK_PAUSE "##PlayPauseAll", GUI_VAR.iconSize)) {
                    for (auto& [name, input] : GetInputs()) {
                        if (input->logfile_ && (cfg_.inputs.count(name) > 0)) {
                            input->logfile_->Pause();
                        }
                    }
                }
            } else {
                if (ImGui::Button(ICON_FK_PLAY "##PlayPauseAll", GUI_VAR.iconSize)) {
                    for (auto& [name, input] : GetInputs()) {
                        if (input->logfile_ && (cfg_.inputs.count(name) > 0)) {
                            input->logfile_->Play();
                        }
                    }
                }
            }
            Gui::ItemTooltip(canPauseAny ? "Pause" : "Play");
            ImGui::EndDisabled();

            ImGui::SameLine();

            ImGui::BeginDisabled(!canStepAny);
            ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
            if (ImGui::Button(ICON_FK_FORWARD "##StepEpochAll", GUI_VAR.iconSize)) {
                for (auto& [name, input] : GetInputs()) {
                    if (input->logfile_ && (cfg_.inputs.count(name) > 0)) {
                        input->logfile_->StepEpoch();
                    }
                }
            }
            ImGui::PopItemFlag();
            Gui::ItemTooltip("Step epoch");
            ImGui::EndDisabled();

            ImGui::SameLine();

            ImGui::BeginDisabled(!canStepAny);
            ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
            if (ImGui::Button(ICON_FK_STEP_FORWARD "##StepMsgAll", GUI_VAR.iconSize)) {
                for (auto& [name, input] : GetInputs()) {
                    if (input->logfile_ && (cfg_.inputs.count(name) > 0)) {
                        input->logfile_->StepMsg();
                    }
                }
            }
            ImGui::PopItemFlag();
            Gui::ItemTooltip("Step message");
            ImGui::EndDisabled();

        }
    }
    ImGui::EndChild();

    Gui::VerticalSeparator();

    if (ImGui::BeginChild("##Map")) {
        if (map_.BeginDraw()) {
            ImDrawList *draw = ImGui::GetWindowDrawList();
            for (const auto& [ ts, points ] : points_) {
                Vec2f pxy;
                for (const auto& [ state, point ] : points) {
                    if (state->cfg_.used) {
                        const Vec2f xy = map_.LatLonToScreen(point.lat_, point.lon_);
                        const ImU32 colour = (state->cfg_.colour > 0 ? IX_COLOUR(COLOURS[state->cfg_.colour]) : FIX_COLOUR(point.fixType_, point.fixOk_));
                        draw->AddRectFilled(xy - Vec2f(2,2), xy + Vec2f(2,2), colour);
                        if (pxy.x != 0.0f) {
                            draw->AddLine(pxy, xy, C_PLOT_MAP_BASELINE());
                        }
                        pxy = xy;
                    }
                }
            }
            map_.EndDraw();
        }
    }

    ImGui::EndChild();

    _DrawWindowEnd();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
