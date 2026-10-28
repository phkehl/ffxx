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

#ifndef __GUI_WIDGET_HEXDUMP_HPP__
#define __GUI_WIDGET_HEXDUMP_HPP__

#include "ffgui_inc.hpp"
//

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWidgetHexdump
{
   public:
    GuiWidgetHexdump(const std::string& name, const std::size_t maxSize = 1024 * 1024);
    virtual ~GuiWidgetHexdump();

    void Set(const std::vector<uint8_t>& data);
    void DrawWidget(const ImVec2& size = { -1, -1 });
    void Clear();
    bool Empty() const;
    std::size_t Lines() const;

   private:
    std::string name_;
    std::size_t maxSize_ = 0;
    std::vector<uint8_t> data_;
    std::vector<std::string> dump_;
    bool isText_ = false;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIDGET_HEXDUMP_HPP__
