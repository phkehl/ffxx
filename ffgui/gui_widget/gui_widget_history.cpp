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
#include "gui_widget_history.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWidgetHistory::GuiWidgetHistory(const std::string& name, HistoryEntries& entries, const std::size_t size,
    AddEntriesFn addEntriesFn) /* clang-format off */ :
    name_           { name },
    entries_        { entries },
    size_           { size > 0 ? size : 20 },
    addEntriesFn_   { addEntriesFn }  // clang-format on
{
    _Update();
}

GuiWidgetHistory::~GuiWidgetHistory()
{
}

// ---------------------------------------------------------------------------------------------------------------------

std::optional<std::string> GuiWidgetHistory::DrawHistory(const char* title)
{
    std::optional<std::string> res;
    ImGui::PushID(this);
    bool update = false;

    const ImVec2 dummySize(3 * GUI_VAR.charSizeX, GUI_VAR.lineHeight);
    const auto a0 = ImGui::GetContentRegionAvail();
    const float trashOffs = a0.x - (2 * GUI_VAR.charSizeX);

    Gui::TextTitle(title);
    ImGui::SameLine();
    ImGui::Dummy(dummySize);
    if (!entries_.empty()) {
        ImGui::SameLine(trashOffs);
        if (ImGui::SmallButton(ICON_FK_TRASH)) {
            ClearHistory();
        }
        Gui::ItemTooltip("Clear history");
    }

    ImGui::Separator();

    if (entries_.empty()) {
        Gui::TextDim("No recent entries");
    }

    std::size_t id = 1;
    for (auto it = entries_.begin(); it != entries_.end();) {
        ImGui::PushID(id++);
        ImGui::SetNextItemAllowOverlap();
        if (ImGui::Selectable(it->c_str())) {
            res = *it;
        }
        ImGui::SameLine();
        ImGui::Dummy(dummySize);
        ImGui::SameLine(trashOffs);
        bool remove = ImGui::SmallButton(ICON_FK_TRASH);
        Gui::ItemTooltip("Remove entry");
        ImGui::PopID();
        if (remove) {
            it = entries_.erase(it);
            update = true;
        } else {
            it++;
        }
    }

    if (!res && addEntriesFn_) {
        ImGui::Separator();
        res = addEntriesFn_();
    }

    if (update) {
        _Update();
    }

    ImGui::PopID();
    return res;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetHistory::ClearHistory()
{
    entries_.clear();
    _Update();
}

const std::list<std::string>& GuiWidgetHistory::GetHistory() const
{
    return entries_;
}

void GuiWidgetHistory::AddHistory(const std::string& entry)
{
    auto exists = std::find(entries_.begin(), entries_.end(), entry);
    if (exists != entries_.end()) {
        entries_.erase(exists);
    }
    entries_.push_front(entry);
    _Update();
}

bool GuiWidgetHistory::ReplaceHistory(const std::string& entry, const std::string& newEntry)
{
    auto exists = std::find(entries_.begin(), entries_.end(), entry);
    if (exists != entries_.end()) {
        *exists = newEntry;
        _Update();
        return true;
    }
    return false;
}

void GuiWidgetHistory::_Update()
{
    while (entries_.size() > size_) {
        entries_.pop_back();
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
