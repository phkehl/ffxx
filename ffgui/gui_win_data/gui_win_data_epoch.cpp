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
    GuiWinData(name, { 80, 25, 0, 0 }, ImGuiWindowFlags_None, input),
    table_   { WinName() } // clang-format off
{
    DEBUG("GuiWinDataEpoch(%s)", WinName().c_str());
    table_.AddColumn("Field");
    table_.AddColumn("Value");
}

GuiWinDataEpoch::~GuiWinDataEpoch()
{
    DEBUG("~GuiWinDataEpoch(%s)", WinName().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataEpoch::_ProcessData(const DataPtr& data)
{
    // New epoch means a new database row is available
    if (data->type_ == DataType::EPOCH)
    {
        table_.ClearRows();

        const auto& row = input_->database_->LatestRow();
        for (const auto &def: input_->database_->FIELDS) {
            if (def.label) {
                table_.AddCellText(def.label);
                if (def.field == Database::FieldIx::ix_fix_type_val) {
                    table_.AddCellTextF("%g %s", row[def.field], row.epoch_.fixTypeStr);
                } else {
                    table_.AddCellTextF(def.fmt ? def.fmt : "%g", row[def.field]);
                }
                table_.SetRowUid((uint32_t)(uint64_t)(void *)def.label);
            }
            if ((def.field == Database::FieldIx::ix_time_tai) && !row.epoch_.time.IsZero()) {
                const uint32_t rowUid = 0xdeadbeef;
                table_.AddCellText("-> Time UTC");
                table_.AddCellText(row.epoch_.time.StrUtcTime());
                table_.SetRowUid(rowUid);

                table_.AddCellText("-> Time GPS");
                table_.AddCellText(row.epoch_.time.StrWnoTow(WnoTow::Sys::GPS));
                table_.SetRowUid(rowUid + 1);

                table_.AddCellText("-> Time GAL");
                table_.AddCellText(row.epoch_.time.StrWnoTow(WnoTow::Sys::GAL));
                table_.SetRowUid(rowUid + 2);

                table_.AddCellText("-> Time BDS");
                table_.AddCellText(row.epoch_.time.StrWnoTow(WnoTow::Sys::BDS));
                table_.SetRowUid(rowUid + 3);

                table_.AddCellText("-> Time GLO");
                const auto glo = row.epoch_.time.GetGloTime(3);
                table_.AddCellTextF("%d:%04d:%09.3f", glo.N4_, glo.Nt_, glo.TOD_);
                table_.SetRowUid(rowUid + 4);
            }
        }
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
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataEpoch::_DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataEpoch::_DrawContent()
{
    table_.DrawTable();
}

/* ****************************************************************************************************************** */
} // ffgui
