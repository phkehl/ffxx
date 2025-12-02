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

#ifndef __GUI_WIDGET_FILTER_HPP__
#define __GUI_WIDGET_FILTER_HPP__

#include "ffgui_inc.hpp"
//
#include <regex>
//
#include "gui_widget_history.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWidgetFilter
{
   public:
    GuiWidgetFilter(const std::string& name, HistoryEntries& history);
    virtual ~GuiWidgetFilter();

    bool DrawWidget(const float width = -FLT_MIN);
    bool Match(const std::string& str);
    bool IsActive();
    bool IsHighlight();
    void SetHightlight(const bool highlight = true);
    void SetTooltip(const std::string& tooltip);
    std::string GetFilterStr();
    void SetFilterStr(const std::string& str);

   private:
    std::string name_;

    struct Config
    {  // clang-format off
        std::string filterStr;
        bool        caseSensitive = false;
        bool        highlight     = false;
        bool        invert        = false;
    };  // clang-format on
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, filterStr, caseSensitive, highlight, invert)
    Config cfg_;

    enum FILTER_e
    {
        FILTER_NONE,
        FILTER_BAD,
        FILTER_OK
    };
    enum FILTER_e filterState_ = FILTER_NONE;
    bool filterChanged_ = true;
    std::unique_ptr<std::regex> filterRegex_;
    void _Update();
    std::string error_;
    std::string tooltip_;
    bool focus_ = false;

    GuiWidgetHistory history_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIDGET_FILTER_HPP__
