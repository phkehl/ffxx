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
#include "gui_widget_table.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::string;

GuiWidgetTable::GuiWidgetTable(const std::string& name, const ImGuiTableFlags flags) /* clang-format off */ :
    name_             { name },
    tableFlagsUser_   { flags } // clang-format on
{
    GuiGlobal::LoadObj(name_ + ".GuiWidgetTable", cfg_);
    ClearRows();
}

GuiWidgetTable::~GuiWidgetTable()
{
    GuiGlobal::SaveObj(name_ + ".GuiWidgetTable", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWidgetTable::Column::Column(
    const std::string& _title, const float _width, const ColumnFlags _flags) /* clang-format off */ :
    title    { _title },
    flags    { _flags },
    width    { _width },
    maxWidth { 0.0f }  // clang-format on
{
}

/*static*/ uint32_t GuiWidgetTable::Row::defSort = 0;

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::AddColumn(const char* title, const float width, /*const enum ColumnFlags*/ int flags)
{
    columns_.emplace_back(title, width, static_cast<ColumnFlags>(flags));
    dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::SetTableScrollFreeze(const int cols, const int rows)
{
    freezeCols_ = std::max(cols, 0);
    freezeRows_ = std::max(rows, 0);
    dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::ClearRows()
{
    rows_.clear();
    dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::ClearSelected()
{
    selectedRows_.clear();
    dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWidgetTable::Cell& GuiWidgetTable::_NextCell()
{
    // Determine next cell
    IM_ASSERT(!columns_.empty());

    // No rows yet? Add one.
    if (rows_.empty()) {
        rows_.emplace_back();
    }

    // Row full? Add one.
    if (rows_[rows_.size() - 1].cells.size() >= columns_.size()) {
        rows_.emplace_back();
    }

    // Get last row
    auto& row = rows_[rows_.size() - 1];

    dirty_ = true;

    // Add new cell and return it
    return row.cells.emplace_back();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::AddCellText(const std::string& text, const uint32_t sort)
{
    Cell& cell = _NextCell();
    cell.content = text;
    cell.sort = sort;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::AddCellText(const char* text, const uint32_t sort)
{
    Cell& cell = _NextCell();
    cell.content = std::string(text);
    cell.sort = sort;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::AddCellTextF(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Cell& cell = _NextCell();
    cell.content = Vsprintf(fmt, args);
    va_end(args);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::AddCellCb(GuiWidgetTable::CellDrawCb_t cb, const void* arg)
{
    Cell& cell = _NextCell();
    cell.drawCb = cb;
    cell.drawCbArg = arg;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::AddCellEmpty(const uint32_t sort)
{
    Cell& cell = _NextCell();
    cell.sort = sort;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::SetCellColour(const ImU32 colour)
{
    if (!rows_.empty()) {
        auto& row = rows_[rows_.size() - 1];
        if (!row.cells.empty()) {
            row.cells[row.cells.size() - 1].colour = colour;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::SetCellSort(const uint32_t sort)
{
    if (!rows_.empty()) {
        auto& row = rows_[rows_.size() - 1];
        if (!row.cells.empty()) {
            row.cells[row.cells.size() - 1].sort = sort;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::SetRowFilter(const uint64_t filter)
{
    if (!rows_.empty()) {
        rows_[rows_.size() - 1].filter = filter;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::SetRowColour(const ImU32 colour)
{
    if (!rows_.empty()) {
        rows_[rows_.size() - 1].colour = colour;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::SetRowSort(const uint32_t sort)
{
    if (!rows_.empty()) {
        rows_[rows_.size() - 1].sort = sort;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::SetRowUid(const uint32_t uid)
{
    if (!rows_.empty()) {
        rows_[rows_.size() - 1].uid = uid;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::SetTableRowFilter(const uint64_t filter)
{
    rowFilter_ = filter;
}

// ---------------------------------------------------------------------------------------------------------------------

int GuiWidgetTable::GetNumRows()
{
    return rows_.size();
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetTable::IsSelected(const uint32_t uid)
{
    return (uid != 0) && (selectedRows_.find(uid) != selectedRows_.end());
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetTable::IsHovered(const uint32_t uid)
{
    return (uid != 0) && (_hoveredRow == uid);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::_CheckData()
{
    // Check sorting
    bool doSort = dirty_;
    ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
    if (sortSpecs && sortSpecs->SpecsDirty) {
        sortSpecs_.clear();
        const int maxSpecColIx = (int)columns_.size() - 1;
        for (int ix = 0; ix < sortSpecs->SpecsCount; ix++) {
            // FIXME: spurious bad sortspecs from imgui
            const int specColIx = sortSpecs->Specs[ix].ColumnIndex;
            if (specColIx > maxSpecColIx) {
                WARNING("Ignore fishy SortSpecs (%d > %d)!", specColIx, maxSpecColIx);
                continue;
            }
            if (!CheckBitsAny(columns_[specColIx].flags, ColumnFlags::SORTABLE)) {
                WARNING("Ignore fishy SortSpecs (col %d no sort)!", specColIx);
                continue;
            }
            sortSpecs_.push_back(sortSpecs->Specs[ix]);
        }
        sortSpecs->SpecsDirty = false;
        doSort = true;
    }
    // DEBUG("sort specs %lu", sortSpecs_.size());
    if (doSort) {
        _SortData();
    }

    if (!dirty_) {
        return;
    }

    tableFlagsEff_ = tableFlagsUser_ | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                     ImGuiTableFlags_NoSavedSettings |
                     ImGuiTableFlags_SizingFixedFit;  // | ImGuiTableFlags_NoBordersInBody
    for (auto& col : columns_) {
        col.titleSize = ImGui::CalcTextSize(col.title.c_str());
        if (CheckBitsAny(col.flags, ColumnFlags::SORTABLE)) {
            tableFlagsEff_ |= ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_SortTristate;
            col.titleSize.x += 1.5f * GUI_VAR.charSizeX;  // add space for the sort indicator
        }
    }

    for (auto& row : rows_) {
        // Calculate text size
        for (auto& cell : row.cells) {
            cell.contentSize = ImGui::CalcTextSize(cell.content.c_str(), nullptr, true);
            cell.paddingLeft = 0.0f;
        }
    }

    int colIx = 0;
    for (auto& col : columns_) {
        // Find row max width
        col.maxWidth = col.titleSize.x;
        bool noMaxWidth = false;
        for (const auto& row : rows_) {
            if ((int)row.cells.size() > colIx) {
                // Row (cell) uses custome draw callback, we cannot know max size
                if (row.cells[colIx].drawCb) {
                    noMaxWidth = true;
                } else if (row.cells[colIx].contentSize.x > col.maxWidth) {
                    col.maxWidth = row.cells[colIx].contentSize.x;
                }
            }
        }
        if (noMaxWidth) {
            col.maxWidth = 0.0;  // let ImGui deal with it
        }

        // Determine left padding for content if align center or right is desired
        if (CheckBitsAny<int>(col.flags, ColumnFlags::ALIGN_RIGHT | ColumnFlags::ALIGN_CENTER)) {
            // Calculate and store padding for each cell
            for (auto& row : rows_) {
                if ((int)row.cells.size() >= colIx) {
                    auto& cell = row.cells[colIx];
                    const float delta = col.maxWidth - cell.contentSize.x;
                    cell.paddingLeft =
                        CheckBitsAll(col.flags, ColumnFlags::ALIGN_CENTER) ? std::floor(0.5f * delta) : delta;
                }
            }
        }

        if (CheckBitsAny(col.flags, ColumnFlags::HIDE_SAME)) {
            int rowIx = 0;
            std::string prevContent;
            for (auto& row : rows_) {
                row.cells[colIx].hide = (rowIx > 0 ? (row.cells[colIx].content == prevContent) : false);
                prevContent = row.cells[colIx].content;
                rowIx++;
            }
        }

        colIx++;
    }

    dirty_ = false;
    return;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::_SortData()
{
    // No specs -> default sort by row sort key
    if (sortSpecs_.empty()) {
        std::sort(rows_.begin(), rows_.end(), [](const Row& a, const Row& b) { return a.sort < b.sort; });
        return;
    }

    // Sort by specs
    std::sort(rows_.begin(), rows_.end(), [this](const Row& rowA, const Row& rowB) {
        for (const auto& spec : sortSpecs_) {
            const auto& cellA = rowA.cells[spec.ColumnIndex];
            const auto& cellB = rowB.cells[spec.ColumnIndex];

            // Sort by numeric key given by user
            if ((cellA.sort != 0) && (cellB.sort != 0) && (cellA.sort != cellB.sort)) {
                return spec.SortDirection == ImGuiSortDirection_Ascending ? cellA.sort < cellB.sort
                                                                          : cellB.sort < cellA.sort;
            }
            // Sort by content (string)
            else if (cellA.content != cellB.content) {
                return spec.SortDirection == ImGuiSortDirection_Ascending ? cellA.content < cellB.content
                                                                          : cellB.content < cellA.content;
            }
        }
        return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTable::DrawTable(ImVec2 size)
{
    if (columns_.empty()) {
        ImGui::TextUnformatted("no table?!");
        return;
    }

    if (ImGui::BeginTable(name_.c_str(), columns_.size() + 1, tableFlagsEff_, size)) {
        // Enable first n columns and/or rows frozen (scroll only the remaining parts of the table)
        if ((freezeRows_ > 0) || (freezeCols_ > 0)) {
            ImGui::TableSetupScrollFreeze(freezeCols_, freezeRows_);
        }

        // Table header
        for (const auto& col : columns_) {
            ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_None;
            if (!CheckBitsAny(col.flags, ColumnFlags::SORTABLE)) {
                flags |= ImGuiTableColumnFlags_NoSort;
            }
            ImGui::TableSetupColumn(col.title.c_str(), flags, col.width > 0.0f ? col.width : col.maxWidth);
        }
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();

        // Check data (calculate column widths, sort data, etc.)
        _CheckData();

        // Draw table body
        _hoveredRow = 0;
        for (const auto& row : rows_) {
            // Skip?
            if ((rowFilter_ != 0) && (row.filter != 0)) {
                if (row.filter != rowFilter_) {
                    continue;
                }
            }

            ImGui::TableNextRow();

            if (row.colour != C_AUTO()) {
                ImGui::PushStyleColor(ImGuiCol_Text, row.colour);
            }

            int ix = 0;
            for (const auto& cell : row.cells) {
                ImGui::TableSetColumnIndex(ix);
                ImGui::PushID(&cell);

                if (cell.colour != C_AUTO()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, cell.colour);
                }

                // Left padding?
                if (cell.paddingLeft != 0.0f) {
                    ImGui::Dummy(ImVec2(cell.paddingLeft, 1));
                    ImGui::SameLine(0, 0);
                }

                // First column is a selectable, and handles selecting rows
                if (ix == 0) {
                    // Rows can be selected
                    if (row.uid != 0) {
                        auto selEntry = selectedRows_.find(row.uid);
                        bool selected = (selEntry != selectedRows_.end() ? selEntry->second : false);
                        ImGui::PushID(row.uid);
                        if (ImGui::Selectable(
                                cell.hide ? "" : cell.content.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                            auto& io = ImGui::GetIO();
                            if (io.KeyCtrl) {
                                selectedRows_[row.uid] = (selEntry != selectedRows_.end() ? !selected : true);
                            } else {
                                selectedRows_.clear();
                                if (!selected) {
                                    selectedRows_[row.uid] = true;
                                }
                            }
                        }
                        ImGui::PopID();
                        if (ImGui::IsItemHovered()) {
                            _hoveredRow = row.uid;
                        }
                    } else {
                        ImGui::Selectable(cell.content.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
                    }
                }
                // Other columns
                else {
                    if (cell.drawCb) {
                        cell.drawCb(cell.drawCbArg);
                    } else {
                        ImGui::TextUnformatted(cell.hide ? "" : cell.content.c_str());
                    }
                }

                if (cell.colour != C_AUTO()) {
                    ImGui::PopStyleColor();
                }

                ImGui::PopID();
                ix++;
            }  // columns

            if (row.colour != C_AUTO()) {
                ImGui::PopStyleColor();
            }
        }  // rows

        ImGui::EndTable();
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
