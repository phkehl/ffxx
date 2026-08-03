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

#ifndef __GUI_WIDGET_LOG_HPP__
#define __GUI_WIDGET_LOG_HPP__

#include "ffgui_inc.hpp"
//
#include <cstdarg>
#include <deque>
#include <regex>
//
#include <fpsdk_common/string.hpp>
//
#include "gui_widget_filter.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWidgetLog
{
   public:
    GuiWidgetLog(const std::string& name, HistoryEntries& history, const bool showControls = true, const std::size_t maxLines = 10000);
    virtual ~GuiWidgetLog();

    void Clear();

    // clang-format off
    void AddLine(const char*        line, const ImU32 colour = C_AUTO(), const std::string& time = "");
    void AddLine(const std::string& line, const ImU32 colour = C_AUTO(), const std::string& time = "");
    void AddLine(const char*        line, const LoggingLevel level,      const std::string& time = "");
    void AddLine(const std::string& line, const LoggingLevel level,      const std::string& time = "");

    void AddNotice( const char* fmt, ...)  PRINTF_ATTR(2);
    void AddInfo(   const char* fmt, ...)  PRINTF_ATTR(2);
    void AddDebug(  const char* fmt, ...)  PRINTF_ATTR(2);
    void AddWarning(const char* fmt, ...)  PRINTF_ATTR(2);
    void AddError(  const char* fmt, ...)  PRINTF_ATTR(2);
    // clang-format on

    // Draw entire widget
    void DrawWidget();

    // Draw controls and log separately, e.g. only draw the log
    bool DrawControls();
    void DrawLog();

    std::string GetFilterStr();
    void SetFilterStr(const std::string& str);

   private:
    std::string name_;

    struct Config
    {
        bool timestamps = true;
        bool autoscroll = true;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, timestamps, autoscroll)
    Config cfg_;

    struct LogLine
    {
        LogLine(std::string _str, const ImU32 _colour, std::string _time);
        std::string str;
        std::string time;
        ImU32 colour = C_AUTO();
        bool match = false;
        bool hovered = false;
        bool selected = false;
    };
    std::deque<LogLine> lines_;
    std::size_t maxLines_ = 0;
    bool scrollToBottom_ = true;

    GuiWidgetFilter filter_;
    int filterNumMatch_ = 0;
    int numSelected_ = 0;
    bool showControls_ = true;

    void _AddLogLine(LogLine&& logLine);
    void _UpdateTooltip();
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIDGET_LOG_HPP__
