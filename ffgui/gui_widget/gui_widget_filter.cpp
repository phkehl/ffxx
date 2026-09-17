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

#include "ffgui_inc.hpp"
//
#include <cstring>
//
#include "gui_widget_filter.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWidgetFilter::GuiWidgetFilter(const std::string& name, HistoryEntries& history) /* clang-format off */ :
    name_      { name },
    history_   { name + ".GuiWidgetFilter", history }  // clang-format on
{
    GuiGlobal::LoadObj(name_ + ".GuiWidgetFilter", cfg_);
    _Update();
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWidgetFilter::~GuiWidgetFilter()
{
    GuiGlobal::SaveObj(name_ + ".GuiWidgetFilter", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetFilter::DrawWidget(const float width)
{
    ImGui::PushID(this);

    bool update = false;

    // Filter menu button
    if (ImGui::Button(ICON_FK_FILTER "##FilterMenu")) {
        ImGui::OpenPopup("FilterMenu");
    }
    Gui::ItemTooltip("Filter options");
    if (ImGui::BeginPopup("FilterMenu")) {
        if (ImGui::MenuItem("Clear filter", NULL, false, !cfg_.filterStr.empty())) {
            cfg_.filterStr.clear();
            update = true;
        }
        ImGui::Separator();
        if (ImGui::Checkbox("Filter case sensitive", &cfg_.caseSensitive)) {
            update = true;
        }
        if (ImGui::Checkbox("Filter highlights", &cfg_.highlight)) {
            update = true;
        }
        if (ImGui::Checkbox("Invert match", &cfg_.invert)) {
            update = true;
        }

        ImGui::Separator();
        auto entry = history_.DrawHistory("Recent filters");
        if (entry) {
            focus_ = true;
            cfg_.filterStr = entry.value();
        }


        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, GUI_VAR.imguiStyle->ItemInnerSpacing.x);

    // Filter input
    ImGui::PushItemWidth(width);
    if (!error_.empty()) { ImGui::PushStyleColor(ImGuiCol_Border, C_C_BRIGHTRED()); }
    if (ImGui::InputTextWithHint("##FilterStr", "Filter (regex)", &cfg_.filterStr,
            ImGuiInputTextFlags_EnterReturnsTrue | (focus_ ? ImGuiInputTextFlags_AutoSelectAll : ImGuiInputTextFlags_None)) &&
        (filterState_ == FILTER_OK)) {
        history_.AddHistory(cfg_.filterStr);
    }
    if (ImGui::IsItemEdited()) {
        update = true;
    }
    if (!error_.empty()) { ImGui::PopStyleColor(); }
    ImGui::PopItemWidth();
    if (focus_) { ImGui::SetKeyboardFocusHere(-1); focus_ = false; }
    if (!error_.empty()) {
        Gui::ItemTooltip(error_.c_str());
    } else if (!tooltip_.empty()) {
        Gui::ItemTooltip(tooltip_);
    }

    ImGui::PopID();

    if (update) {
        _Update();
        return true;
    } else {
        return false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetFilter::_Update()
{
    // Filter cleared
    if (cfg_.filterStr.empty()) {
        filterState_ = FILTER_NONE;
        filterRegex_.reset();
        return;
    }

    // Try regex
    try {
        std::regex_constants::syntax_option_type flags = std::regex_constants::nosubs | std::regex_constants::optimize;
        if (!cfg_.caseSensitive) {
            flags |= std::regex_constants::icase;
        }
        filterRegex_ = std::make_unique<std::regex>(cfg_.filterStr, flags);
        filterState_ = FILTER_OK;
        error_.clear();
    }
    catch (const std::exception& e) {
        filterState_ = FILTER_BAD;
        filterRegex_.reset();
        error_ = Sprintf("Bad filter: %s", e.what());
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetFilter::Match(const std::string& str)
{
    bool match = true;
    switch (filterState_) {
        case FILTER_OK:
            match = std::regex_search(str, *filterRegex_) == !cfg_.invert;
            break;
        case FILTER_BAD:
        case FILTER_NONE:
            break;
    }
    return match;
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetFilter::IsActive()
{
    return filterState_ == FILTER_OK;
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetFilter::IsHighlight()
{
    return cfg_.highlight;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetFilter::SetHightlight(const bool highlight)
{
    cfg_.highlight = highlight;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetFilter::SetTooltip(const std::string& tooltip)
{
    tooltip_ = tooltip;
}

// ---------------------------------------------------------------------------------------------------------------------

std::string GuiWidgetFilter::GetFilterStr()
{
    return cfg_.filterStr;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetFilter::SetFilterStr(const std::string& str)
{
    cfg_.filterStr = str;
    _Update();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
