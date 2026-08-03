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

#ifndef __GUI_WIDGET_HISTORY_HPP__
#define __GUI_WIDGET_HISTORY_HPP__

//
#include "ffgui_inc.hpp"
//
#include <functional>
#include <optional>
#include <list>

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWidgetHistory
{
   public:
    using AddEntriesFn = std::function<std::optional<std::string>()>;
    GuiWidgetHistory(const std::string& name, HistoryEntries& entries, const std::size_t size = 0 /* = 20 */,
        AddEntriesFn addEntriesFn = nullptr);
    virtual ~GuiWidgetHistory();

    std::optional<std::string> DrawHistory(const char* title);

    void ClearHistory();
    const std::list<std::string>& GetHistory() const;
    void AddHistory(const std::string& entry);
    bool ReplaceHistory(const std::string& entry, const std::string& newEntry);

   private:
    std::string name_;
    HistoryEntries& entries_;
    std::size_t size_;
    AddEntriesFn addEntriesFn_;
    void _Update();
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIDGET_HISTORY_HPP__
