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

#ifndef __GUI_WIN_HEXDECBIN_HPP__
#define __GUI_WIN_HEXDECBIN_HPP__

//
#include "ffgui_inc.hpp"
//
extern "C" {
#include "tetris.h"
}
//
#include "gui_win.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinHexDecBin : public GuiWin
{
   public:
    GuiWinHexDecBin();
    ~GuiWinHexDecBin();

    void DrawWindow() override final;

   private:
    union Value
    {
        uint64_t u64;
        uint32_t u32;
        uint16_t u16;
        uint8_t u8;
        uint8_t bytes[8];
    };

    struct Entry
    {
        std::string note;
        uint64_t val = 0x0102030405060708;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Entry, note, val)

    struct Config
    {
        std::list<Entry> entries;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, entries)
    Config cfg_;

    static constexpr std::size_t MAX_ENTRIES = 50;

    void _DrawEntry(Entry& entry);
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_HEXDECBIN_HPP__
