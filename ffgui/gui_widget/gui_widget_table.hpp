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

#ifndef __GUI_WIDGET_TABLE_HPP__
#define __GUI_WIDGET_TABLE_HPP__

#include "ffgui_inc.hpp"
//
#include <functional>
#include <unordered_map>
//
#include <fpsdk_common/math.hpp>
using namespace fpsdk::common::math;

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWidgetTable
{
   public:
    GuiWidgetTable(
        const std::string& name, const ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY);
    ~GuiWidgetTable();

    // Setup: Add columns (first thing, and only once)
    enum ColumnFlags
    {
        NONE = 0,
        ALIGN_RIGHT = Bit<int>(0),
        ALIGN_CENTER = Bit<int>(1),
        SORTABLE = Bit<int>(2),
        HIDE_SAME = Bit<int>(3)
    };
    void AddColumn(const char* title, const float width = 0.0f, const /*enum ColumnFlags*/ int = ColumnFlags::NONE);

    // Set table parameters (optional, can be done on setup and also changed later)
    void SetTableScrollFreeze(const int cols, const int rows);  // Set first n cols and/or rows frozen (never scroll)
    void SetTableRowFilter(const uint64_t filter = 0);          // Set filter

    // Add data. Call these for each cell until the table is full
    // The first cell can have an ##id to support row selection in case of duplicate or empty values for some of those
    // cells
    void AddCellText(const std::string& text, const uint32_t sort = 0);
    void AddCellText(const char* text, const uint32_t sort = 0);
    void AddCellTextF(const char* fmt, ...);
    using CellDrawCb_t = std::function<void(const void*)>;
    void AddCellCb(CellDrawCb_t cb, const void* arg);  // FIXME: not for the first column!
    void AddCellEmpty(const uint32_t sort = 0);

    // Cell stuff (must be called right after AddCell...())
    void SetCellColour(const ImU32 colour);
    void SetCellSort(const uint32_t sort);  // Set sorting key for cell (default: content string)

    // Row stuff (must be called after first AddCell..() of the row and before first AddCell..() of the next row)
    void SetRowFilter(const uint64_t filter);  // Set a filter for the current column
    void SetRowColour(const ImU32 colour);
    void SetRowSort(
        const uint32_t sort);  // Set key for sorting rows if no column sort is active (default: in order added)
    void SetRowUid(
        const uint32_t uid);  // Set a non-zero unique ID to enable row(s) selection and checking for selected row(s)

    // Table info, valid after adding data
    int GetNumRows();
    bool IsSelected(const uint32_t uid);  // Must use SetRowUid() for this to work
    bool IsHovered(const uint32_t uid);   // Must use SetRowUid() for this to work

    // Clear data. Must add new data afterwards.
    void ClearRows();

    // Draw table
    void DrawTable(const ImVec2 size = {});

    // Clear selected rows
    void ClearSelected();

   private:
    std::string name_;
    ImGuiTableFlags tableFlagsUser_ = ImGuiTableFlags_None;

    struct Config
    {
        bool dummy = false;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, dummy)
    Config cfg_;

    int freezeCols_ = 1;
    int freezeRows_ = 1;
    bool rowsSelectable_ = false;
    uint64_t rowFilter_ = 0;

    bool dirty_ = false;

    struct Column
    {
        Column(const std::string& _title, const float _width = 0.0f, const ColumnFlags _flags = ColumnFlags::NONE);
        std::string title;
        enum ColumnFlags flags;
        Vec2f titleSize;
        float width;
        float maxWidth;
    };

    struct Cell
    {
        std::string content;
        Vec2f contentSize;
        float paddingLeft = 0.0f;
        CellDrawCb_t drawCb = nullptr;
        const void* drawCbArg = nullptr;
        ImU32 colour = C_AUTO();
        uint32_t sort = 0;
        bool hide = false;
    };

    struct Row
    {
        std::vector<Cell> cells;
        uint64_t filter = 0;
        ImU32 colour = C_AUTO();
        uint32_t sort = defSort++;
        uint32_t uid = 0;

       private:
        static uint32_t defSort;
    };
    ImGuiTableFlags tableFlagsEff_ =  ImGuiTableFlags_None;

    Cell& _NextCell();
    void _CheckData();
    void _SortData();

    std::vector<Column> columns_;
    std::vector<Row> rows_;
    std::unordered_map<uint32_t, bool> selectedRows_;  // row.uid
    uint32_t _hoveredRow = 0;                          // row.uid

    std::vector<ImGuiTableColumnSortSpecs> sortSpecs_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIDGET_TABLE_HPP__
