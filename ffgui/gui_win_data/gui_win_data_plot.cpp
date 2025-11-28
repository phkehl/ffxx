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
//
#include "gui_win_data_plot.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinDataPlot::GuiWinDataPlot(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 80, 25, 0, 0 }, ImGuiWindowFlags_None, input)  // clang-format on
{
    DEBUG("GuiWinDataPlot(%s)", WinName().c_str());

    toolbarClearEna_ = false;
    std::snprintf(dndType_, sizeof(dndType_), "PlotVar_%016" PRIx64, WinUid());

    GuiGlobal::LoadObj(WinName() + ".GuiWinDataPlot", cfg_);
    GuiGlobal::LoadObj("GuiWinDataPlot", savedCfg_); // static!

    _SetConfig(cfg_);
}

GuiWinDataPlot::~GuiWinDataPlot()
{
    DEBUG("~GuiWinDataPlot(%s)", WinName().c_str());
    GuiGlobal::SaveObj(WinName() + ".GuiWinDataPlot", cfg_);
    GuiGlobal::SaveObj("GuiWinDataPlot", savedCfg_); // static!
}

/*static*/ GuiWinDataPlot::SavedConfig GuiWinDataPlot::savedCfg_;

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataPlot::_ProcessData(const DataPtr& /*data*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataPlot::_Loop(const Time& /*now*/)
{
    if (!input_->database_->Changed(this) || !xVar_.valid) {
        return;
    }

    // Find most recent x value
    double xMax = NAN;
    const Database::FieldIx fieldIx = xVar_.field.field;
    input_->database_->ProcRows(
        [&](const Database::Row& row) {
            const double xValue = row[fieldIx];
            if (!std::isnan(xValue)) {
                xMax = xValue;
                return false;
            }
            return true;
        },
        true);

    if (!std::isnan(xMax)) {
        // Update x axis range
        if ((cfg_.fitMode == FitMode::FOLLOW_X) && xVar_.valid) {
            if (!std::isnan(followXmax_)) {
                const double delta = xMax - followXmax_;
                if (delta != 0.0) {
                    plotLimitsY1_.X.Max += delta;
                    plotLimitsY1_.X.Min += delta;
                    setLimitsX_ = true;
                }
            }
            setLimitsX_ = true;
        }

        followXmax_ = xMax;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataPlot::_ClearData()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataPlot::_DrawToolbar()
{
    // Reset plot
    ImGui::BeginDisabled(!xVar_.valid && yVars_.empty());
    if (ImGui::Button(ICON_FK_TRASH_O "##ResetPlot", GUI_VAR.iconSize)) {
        _ResetPlot();
    }
    ImGui::EndDisabled();

    Gui::VerticalSeparator();

    // Add variables to plot
    if (ImGui::Button(ICON_FK_PLUS "##AddVar", GUI_VAR.iconSize)) {
        ImGui::OpenPopup("AddVarMenu");
    }
    Gui::ItemTooltip("Add variables to plot.\nDouble-click items in this list or drag them to the plot or axes.");
    if (ImGui::IsPopupOpen("AddVarMenu")) {
        ImGui::SetNextWindowSize(ImVec2(-1, GUI_VAR.lineHeight * 25));
        if (ImGui::BeginPopup("AddVarMenu")) {
            Gui::BeginMenuPersist();
            for (auto& field : Database::FIELDS) {
                // Skip entries without a title, they are not intended to be plotted
                if (!field.label) {
                    continue;
                }

                // Plot variable entry with tooltip
                const bool used = usedFields_[field.name];
                if (used) {
                    ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM());
                }
                if (ImGui::Selectable(field.label, false, ImGuiSelectableFlags_AllowDoubleClick)) {
                    // On double click, act like it was dragged onto the plot
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        _HandleDrop({ field, DragDrop::LIST, DragDrop::PLOT });
                    }
                }
                if (used) {
                    ImGui::PopStyleColor();
                }
                Gui::ItemTooltip(field.label);

                // Entry is a drag and drop source
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    DragDrop dragdrop(field, DragDrop::LIST);
                    ImGui::SetDragDropPayload(dndType_, &dragdrop, sizeof(dragdrop));
                    ImGui::TextUnformatted(field.label);
                    ImGui::EndDragDropSource();
                }
            }
            Gui::EndMenuPersist();
            ImGui::EndPopup();
        }
    }

    ImGui::SameLine();

    // Colormap
    if (ImGui::Button(ICON_FK_PAINT_BRUSH "###Colormap", GUI_VAR.iconSize)) {
        ImGui::OpenPopup("ChooseColourmap");
    }
    Gui::ItemTooltip("Colours");
    if (ImGui::IsPopupOpen("ChooseColourmap")) {
        if (ImGui::BeginPopup("ChooseColourmap")) {
            Gui::BeginMenuPersist();
            for (ImPlotColormap map = 0; map < ImPlot::GetColormapCount(); map++) {
                if (ImPlot::ColormapButton(ImPlot::GetColormapName(map), ImVec2(225, 0), map)) {
                    cfg_.colormap = map;
                    ImPlot::BustColorCache(/* "##plot" */);  // FIXME busting only our plot doesn't work
                }
            }

            Gui::EndMenuPersist();
            ImGui::EndPopup();
        }
    }

    ImGui::SameLine();

    // Fit mode
    const bool fitFollowX = (cfg_.fitMode == FitMode::FOLLOW_X);
    const bool fitAutofit = (cfg_.fitMode == FitMode::AUTOFIT);
    // - Follow X
    if (!fitFollowX) {
        ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM());
    }
    if (ImGui::Button(ICON_FK_ARROW_RIGHT "##FollowX", GUI_VAR.iconSize)) {
        cfg_.fitMode = (fitFollowX ? FitMode::NONE : FitMode::FOLLOW_X);
    }
    if (!fitFollowX) {
        ImGui::PopStyleColor();
    }
    Gui::ItemTooltip("Follow x (automatically move x axis)");
    ImGui::SameLine();
    // - Autofit
    if (!fitAutofit) {
        ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM());
    }
    if (ImGui::Button(ICON_FK_ARROWS_ALT "##Autofit", GUI_VAR.iconSize)) {
        cfg_.fitMode = (fitAutofit ? FitMode::NONE : FitMode::AUTOFIT);
    }
    if (!fitAutofit) {
        ImGui::PopStyleColor();
    }
    Gui::ItemTooltip("Automatically fit axes ranges to data");

    ImGui::SameLine();

    // Toggle legend
    Gui::ToggleButton(ICON_FK_LIST "##Legend", nullptr, &cfg_.showLegend, "Toggle legend", nullptr, GUI_VAR.iconSize);

    // Note
    Gui::VerticalSeparator();
    ImGui::SetNextItemWidth(-(2 * GUI_VAR.iconSize.x) - GUI_VAR.imguiStyle->ItemInnerSpacing.x - GUI_VAR.imguiStyle->ItemSpacing.x);
    ImGui::InputTextWithHint("##Note", "Note", &cfg_.note);

    auto saved = std::find_if(savedCfg_.plots.begin(), savedCfg_.plots.end(), [note = cfg_.note](const auto& cand) { return note == cand.note; });
    const bool canSave = !cfg_.note.empty() && ((saved == savedCfg_.plots.end()) || (cfg_ != *saved));
    WinToggleFlag(ImGuiWindowFlags_UnsavedDocument, canSave);

    // Save
    ImGui::SameLine(0, GUI_VAR.imguiStyle->ItemInnerSpacing.x);
    ImGui::BeginDisabled(!canSave);
    if (ImGui::Button(ICON_FK_FLOPPY_O)) { // clang-format off
        if (saved != savedCfg_.plots.end()) {
            *saved = cfg_;
        } else {
            savedCfg_.plots.push_back(cfg_);
            std::sort(savedCfg_.plots.begin(), savedCfg_.plots.end());
        }
    }
    Gui::ItemTooltip("Save plot");
    ImGui::EndDisabled();

    // Load
    ImGui::BeginDisabled(savedCfg_.plots.empty());
    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_FOLDER_O)) {
        ImGui::OpenPopup("Load");
    }
    Gui::ItemTooltip("Load plot");
    ImGui::EndDisabled();
    if (ImGui::BeginPopup("Load")) {

        const ImVec2 dummySize(3 * GUI_VAR.charSizeX, GUI_VAR.lineHeight);
        const auto a0 = ImGui::GetContentRegionAvail();
        const float trashOffs = a0.x - (2 * GUI_VAR.charSizeX);
        std::size_t id = 1;

        for (auto it = savedCfg_.plots.begin(); it != savedCfg_.plots.end(); ) {
            ImGui::PushID(id++);
            ImGui::SetNextItemAllowOverlap();
            if (ImGui::Selectable(
                    Sprintf("%.30s%s", it->note.c_str(), it->note.size() > 30 ? "..." : "").c_str())) {
                _SetConfig(*it);
            }
            ImGui::SameLine();
            ImGui::Dummy(dummySize);
            ImGui::SameLine(trashOffs);
            bool remove = ImGui::SmallButton(ICON_FK_TRASH);
            Gui::ItemTooltip("Remove plot");
            ImGui::PopID();
            if (remove) {
                it = savedCfg_.plots.erase(it);
            } else {
                it++;
            }
        }
        ImGui::EndPopup();
    }

    // ImGui::SameLine();
    // ImGui::Text("x1: %.1f %.1f, y1: %.1f %.1f, y2: %.1f %.1f, y3: %.1f %.1f, xMax %.1f, color %d",
    // plotLimitsY1_.X.Min, plotLimitsY1_.X.Max,
    //     plotLimitsY1_.Y.Min, plotLimitsY1_.Y.Max, plotLimitsY2_.Y.Min, plotLimitsY2_.Y.Max, plotLimitsY3_.Y.Min,
    //     plotLimitsY3_.Y.Max, followXmax_, cfg_.colormap);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataPlot::_DrawContent()
{
    std::unique_ptr<DragDrop> dragdrop;

    // Wrapper so that we can detect drop target on the entire content
    ImGui::BeginChild("PlotWrapper");

    // Plot
    ImPlot::PushColormap(cfg_.colormap);
    switch (cfg_.fitMode) {
        case FitMode::NONE:
            break;
        case FitMode::AUTOFIT:
            ImPlot::SetNextAxesToFit();
            break;
        case FitMode::FOLLOW_X:
            break;
    }
    ImPlotFlags plotFlags = ImPlotFlags_Crosshairs | ImPlotFlags_NoFrame /*| ImPlotFlags_NoMenus*/;
    if (!cfg_.showLegend) {
        plotFlags |= ImPlotFlags_NoLegend;
    }
    if (ImPlot::BeginPlot("##plot", ImVec2(-1, -1), plotFlags)) {
        // X axis
        ImPlot::SetupAxis(ImAxis_X1, axisLabels_[ImAxis_X1].c_str());
        ImPlot::SetupAxisLimits(
            ImAxis_X1, plotLimitsY1_.X.Min, plotLimitsY1_.X.Max, setLimitsX_ ? ImPlotCond_Always : ImPlotCond_None);
        // Y axes
        const bool showY1 = axisUsed_[ImAxis_Y1] || (dndHovered_ && axisUsed_[ImAxis_X1]) || setLimitsY_;
        const bool showY2 = axisUsed_[ImAxis_Y2] || (dndHovered_ && axisUsed_[ImAxis_X1]) || setLimitsY_;
        const bool showY3 = axisUsed_[ImAxis_Y3] || (dndHovered_ && axisUsed_[ImAxis_X1]) || setLimitsY_;
        ImPlot::SetupAxis(ImAxis_Y1, axisLabels_[ImAxis_Y1].c_str());
        ImPlot::SetupAxisLimits(
            ImAxis_Y1, plotLimitsY1_.Y.Min, plotLimitsY1_.Y.Max, setLimitsY_ ? ImPlotCond_Always : ImPlotCond_None);
        if (showY2) {
            ImPlot::SetupAxis(ImAxis_Y2, axisLabels_[ImAxis_Y2].c_str(), ImPlotAxisFlags_AuxDefault);
            ImPlot::SetupAxisLimits(
                ImAxis_Y2, plotLimitsY2_.Y.Min, plotLimitsY2_.Y.Max, setLimitsY_ ? ImPlotCond_Always : ImPlotCond_None);
        }
        if (showY3) {
            ImPlot::SetupAxis(ImAxis_Y3, axisLabels_[ImAxis_Y3].c_str(), ImPlotAxisFlags_AuxDefault);
            ImPlot::SetupAxisLimits(
                ImAxis_Y3, plotLimitsY3_.Y.Min, plotLimitsY3_.Y.Max, setLimitsY_ ? ImPlotCond_Always : ImPlotCond_None);
        }
        ImPlot::SetupFinish();
        setLimitsX_ = false;
        setLimitsY_ = false;

        // Plot data
        if (xVar_.valid && !yVars_.empty()) {
            input_->database_->BeginGetRows();

            struct Helper
            {
                Database::FieldIx xIx;
                Database::FieldIx yIx;
                Database* db;
            } helper = { xVar_.field.field, Database::FieldIx::ix_none, input_->database_.get() };
            for (const auto& yVar : yVars_) {
                // Plot
                helper.yIx = yVar.field.field;
                ImPlot::SetAxes(xVar_.axis, yVar.axis);
                ImPlot::PlotLineG(
                    yVar.field.label,
                    [](int ix, void* arg) {
                        Helper* h = (Helper*)arg;
                        const Database& db = *h->db;
                        return ImPlotPoint(db[ix][h->xIx], db[ix][h->yIx]);
                    },
                    &helper, input_->database_->Usage());

                if (ImPlot::BeginLegendPopup(yVar.field.label, ImGuiMouseButton_Right)) {
                    if (ImGui::Button("Remove from plot")) {
                        dragdrop = std::make_unique<DragDrop>(yVar.field, DragDrop::LEGEND, DragDrop::TRASH);
                        ImGui::CloseCurrentPopup();
                    }
                    ImPlot::EndLegendPopup();
                }

                // Allow legend labels to be dragged and dropped
                if (ImPlot::BeginDragDropSourceItem(yVar.field.label)) {
                    DragDrop dd(yVar.field, DragDrop::LEGEND);
                    ImGui::SetDragDropPayload(dndType_, &dd, sizeof(dd));
                    ImPlot::ItemIcon(ImPlot::GetLastItemColor());
                    ImGui::SameLine();
                    ImGui::TextUnformatted(yVar.field.label);
                    ImGui::EndDragDropSource();
                }
            }

            input_->database_->EndGetRows();
        }

        plotLimitsY1_ = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y1);
        plotLimitsY2_ = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y2);
        plotLimitsY3_ = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y3);

        // Drop on plot (only if free axis available)
        if (!dragdrop &&
            (!axisUsed_[ImAxis_X1] || !axisUsed_[ImAxis_Y1] || !axisUsed_[ImAxis_Y2] || !axisUsed_[ImAxis_Y3]) &&
            ImPlot::BeginDragDropTargetPlot()) {
            const ImGuiPayload* imPayload = ImGui::AcceptDragDropPayload(dndType_);
            if (imPayload) {
                dragdrop = std::make_unique<DragDrop>(imPayload, DragDrop::PLOT);
            }
            ImPlot::EndDragDropTarget();
        }
        // Drop on X1 axis
        if (!dragdrop) {
            dragdrop = _CheckDrop(ImAxis_X1);
        }
        // Drop on Y axes
        if (!dragdrop && showY1) {
            dragdrop = _CheckDrop(ImAxis_Y1);
        }
        if (!dragdrop && showY2) {
            dragdrop = _CheckDrop(ImAxis_Y2);
        }
        if (!dragdrop && showY3) {
            dragdrop = _CheckDrop(ImAxis_Y3);
        }

        // Axis tooltips
        if (showY1 && !axisTooltips_[ImAxis_Y1].empty() && ImPlot::IsAxisHovered(ImAxis_Y1)) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(axisTooltips_[ImAxis_Y1].c_str());
            ImGui::EndTooltip();
        }
        if (showY2 && !axisTooltips_[ImAxis_Y2].empty() && ImPlot::IsAxisHovered(ImAxis_Y2)) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(axisTooltips_[ImAxis_Y2].c_str());
            ImGui::EndTooltip();
        }
        if (showY1 && !axisTooltips_[ImAxis_Y3].empty() && ImPlot::IsAxisHovered(ImAxis_Y3)) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(axisTooltips_[ImAxis_Y2].c_str());
            ImGui::EndTooltip();
        }

        ImPlot::EndPlot();
    }
    ImPlot::PopColormap();

    // Trash
    if (dndHovered_) {
        const auto pad = Vec2f(GUI_VAR.imguiStyle->WindowPadding) * 2.0f;
        const auto size = Vec2f(GUI_VAR.iconSize) * 2.0f;
        ImGui::SetCursorPos(Vec2f(ImGui::GetWindowSize()) - size - pad);
        ImGui::Button(ICON_FK_TRASH_O "##Trash", size);
        if (ImGui::BeginDragDropTarget()) {
            const ImGuiPayload* imPayload = ImGui::AcceptDragDropPayload(dndType_);
            if (imPayload) {
                dragdrop = std::make_unique<DragDrop>(imPayload, DragDrop::TRASH);
            }
            ImGui::EndDragDropTarget();
        }
    }

    // End wrapper
    ImGui::EndChild();

    // Detect hovering with a drag and drop thingy to display all axes to reveal them as dnd targets
    if (ImGui::BeginDragDropTarget()) {
        dndHovered_ = true;
        ImGui::EndDragDropTarget();
    } else {
        dndHovered_ = false;
    }

    // Handle drops (*after* plotting!)
    if (dragdrop) {
        _HandleDrop(*dragdrop);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWinDataPlot::DragDrop::DragDrop(const Database::FieldDef& _field, Target _src)
    : src{ _src }, dst{ UNKNOWN }, field{ _field }
{
}

GuiWinDataPlot::DragDrop::DragDrop(const Database::FieldDef& _field, const Target _src, const Target _dst)
    : src{ _src }, dst{ _dst }, field{ _field }
{
}

GuiWinDataPlot::DragDrop::DragDrop(const ImGuiPayload* imPayload, const Target _dst)
    : src{ ((DragDrop*)imPayload->Data)->src }, dst{ _dst }, field{ ((DragDrop*)imPayload->Data)->field }
{
}

// ---------------------------------------------------------------------------------------------------------------------

std::unique_ptr<GuiWinDataPlot::DragDrop> GuiWinDataPlot::_CheckDrop(const ImAxis axis)
{
    std::unique_ptr<DragDrop> res;
    if (ImPlot::BeginDragDropTargetAxis(axis)) {
        const ImGuiPayload* imPayload = ImGui::AcceptDragDropPayload(dndType_);
        if (imPayload) {
            res = std::make_unique<DragDrop>(imPayload, (DragDrop::Target)axis);
        }
        ImPlot::EndDragDropTarget();
    }
    return res;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataPlot::_HandleDrop(const DragDrop& dragdrop)
{
    // DEBUG("dragdrop %d -> %d", dragdrop.src, dragdrop.dst);

    DragDrop::Target dst = dragdrop.dst;

    // Drops on plot go to an axis if possible
    if (dst == DragDrop::PLOT) {
        // No x-axis yet -> set as x axis
        if (!axisUsed_[ImAxis_X1]) {
            dst = DragDrop::X;
        }
        // Have x-axis -> use next free y axis
        else if (!axisUsed_[ImAxis_Y1]) {
            dst = DragDrop::Y1;
        } else if (!axisUsed_[ImAxis_Y2]) {
            dst = DragDrop::Y2;
        } else if (!axisUsed_[ImAxis_Y3]) {
            dst = DragDrop::Y3;
        }
        // Invalid drop
        else {
            return;
        }
    }

    // Remove (only y vars), also for drag from legend
    if ((dst == DragDrop::TRASH) || (dragdrop.src == DragDrop::LEGEND)) {
        for (auto iter = yVars_.begin(); iter != yVars_.end();) {
            auto& yVar = *iter;
            if (yVar.field.name == dragdrop.field.name) {
                iter = yVars_.erase(iter);
            } else {
                iter++;
            }
        }
    }

    // Drop on X axis
    if (dst == DragDrop::X) {
        xVar_ = { true, dst, dragdrop.field };
    }
    // Drop on Y axis
    else if ((dst >= DragDrop::Y1) && (dst <= DragDrop::Y3)) {
        const PlotVar var({ true, dst, dragdrop.field });
        if (std::find(yVars_.begin(), yVars_.end(), var) == yVars_.end()) {
            yVars_.push_back(var);
        }
    }
    // else: Invalid

    _UpdateVars();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataPlot::_UpdateVars()
{
    cfg_.vars.clear();
    usedFields_.clear();
    if (xVar_.valid) {
        axisLabels_[ImAxis_X1] = xVar_.field.label;
        usedFields_[xVar_.field.name] = true;
        axisUsed_[ImAxis_X1] = true;
        cfg_.vars.push_back({ ImAxis_X1, xVar_.field.name });
    } else {
        axisLabels_[ImAxis_X1].clear();
        axisUsed_[ImAxis_X1] = false;
    }

    for (ImAxis axis = ImAxis_Y1; axis <= ImAxis_Y3; axis++) {
        axisUsed_[axis] = false;
        axisLabels_[axis].clear();
        axisTooltips_[axis].clear();

        // Find relevant variables
        std::vector<const Database::FieldDef*> fieldDefs;
        for (const auto& var : yVars_) {
            if (var.axis == axis) {
                fieldDefs.push_back(std::addressof(var.field));
                axisUsed_[axis] = true;
                usedFields_[var.field.name] = true;
                cfg_.vars.push_back({ axis, var.field.name });
            }
        }

        // Set label, use short variable names resp. full variable label depending on the number of vars
        if (fieldDefs.size() > 1) {
            std::string label;
            std::string tooltip;
            for (auto fd : fieldDefs) {
                label += " / ";
                label += fd->name;
                tooltip += "\n";
                tooltip += fd->label;
            }
            axisLabels_[axis] = label.substr(3);
            axisTooltips_[axis] = tooltip.substr(1);
        } else if (fieldDefs.size() > 0) {
            axisLabels_[axis] = fieldDefs[0]->label;
        }
    }
    std::sort(cfg_.vars.begin(), cfg_.vars.end());
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataPlot::_SetConfig(const Config& cfg)
{
    const Config save = cfg;
    _ResetPlot();
    cfg_ = save;

    for (auto& var : cfg_.vars) {
        // Known field?
        const auto field = std::find_if(Database::FIELDS.begin(), Database::FIELDS.end(),
            [name = var.name](const auto& cand) { return name == cand.name; });
        if (field == Database::FIELDS.end()) {
            continue;
        }

        switch (var.axis) {
            case ImAxis_X1:
                xVar_ = { true, var.axis, *field };
                break;
            case ImAxis_Y1:
            case ImAxis_Y2:
            case ImAxis_Y3:
                yVars_.push_back({ true, var.axis, *field });
                break;
        }
    }
    if (!xVar_.valid) {
        yVars_.clear();
    }

    _UpdateVars();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataPlot::_ResetPlot()
{
    cfg_.note.clear();
    xVar_ = {};
    yVars_.clear();
    plotLimitsY1_ = PLOT_LIMITS_DEF;
    plotLimitsY2_ = PLOT_LIMITS_DEF;
    plotLimitsY3_ = PLOT_LIMITS_DEF;
    setLimitsX_ = true;
    setLimitsY_ = true;
    _UpdateVars();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
