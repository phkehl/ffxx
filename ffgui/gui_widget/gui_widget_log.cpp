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
#include "gui_widget_log.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::string;

GuiWidgetLog::LogLine::LogLine(std::string _str, const ImU32 _colour, std::string _time) /* clang-format off */ :
    str    { std::move(_str) },
    time   { _time.empty() ? (Time::FromClockRealtime() - GUI_VAR.t0).Stringify() : std::move(_time) },
    colour { _colour }  // clang-format on
{
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWidgetLog::GuiWidgetLog(const std::string& name, HistoryEntries& history, const bool showControls,
    const std::size_t maxLines) /* clang-format off */ :
    name_           { name },
    maxLines_       { std::clamp<std::size_t>(maxLines, 10, 100000) },
    filter_         { name + ".GuiWidgetLog", history },
    showControls_   { showControls }  // clang-format on
{
    GuiGlobal::LoadObj(name_ + ".GuiWidgetLog", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWidgetLog::~GuiWidgetLog()
{
    GuiGlobal::SaveObj(name_ + ".GuiWidgetLog", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetLog::Clear()
{
    lines_.clear();
    filterNumMatch_ = 0;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetLog::AddLine(const char* line, const ImU32 colour, const std::string& time)
{
    if ((line != NULL) && (line[0] != '\0')) {
        _AddLogLine({line, colour, time});
    }
}

void GuiWidgetLog::AddLine(const std::string& line, const ImU32 colour, const std::string& time)
{
    if (line.size() > 0) {
        _AddLogLine({line, colour, time});
    }
}

void GuiWidgetLog::AddLine(const char* line, const LoggingLevel level, const std::string& time)
{
    // Prepend prefix to actual string so that we can filter on it
    const char* prefix = "?: ";
    ImU32 colour = 0;
    switch (level) {  // clang-format off
        case LoggingLevel::TRACE:   prefix = "T: "; colour = C_DEBUG_TRACE();   break;
        case LoggingLevel::DEBUG:   prefix = "D: "; colour = C_DEBUG_DEBUG();   break;
        case LoggingLevel::INFO:    prefix = "I: "; colour = C_DEBUG_INFO();    break;
        case LoggingLevel::NOTICE:  prefix = "N: "; colour = C_DEBUG_NOTICE();  break;
        case LoggingLevel::WARNING: prefix = "W: "; colour = C_DEBUG_WARNING(); break;
        case LoggingLevel::ERROR:   prefix = "E: "; colour = C_DEBUG_ERROR();   break;
        case LoggingLevel::FATAL:   prefix = "F: "; colour = C_DEBUG_FATAL();   break;
    }  // clang-format on

    // Copy and add prefix
    char tmp[1000];
    int len = snprintf(tmp, sizeof(tmp), "%s%s", prefix, line);

    // Remove trailing \n
    if ((len > 0) && (tmp[len - 1] == '\n')) {
        tmp[len - 1] = '\0';
    }

    _AddLogLine({ tmp, colour, time });
}

void GuiWidgetLog::AddLine(const std::string& line, const LoggingLevel level, const std::string& time)
{
    if (!line.empty()) {
        AddLine(line.data(), level, time);
    }
}

void GuiWidgetLog::AddNotice(const char* fmt, ...)
{
    char line[1000];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    AddLine(line, LoggingLevel::NOTICE, "");
}

void GuiWidgetLog::AddInfo(const char* fmt, ...)
{
    char line[1000];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    AddLine(line, LoggingLevel::INFO, "");
}

void GuiWidgetLog::AddDebug(const char* fmt, ...)
{
    if (!LoggingIsLevel(LoggingLevel::DEBUG)) {
        return;
    }

    char line[1000];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    AddLine(line, LoggingLevel::DEBUG, "");
}

void GuiWidgetLog::AddWarning(const char* fmt, ...)
{
    char line[1000];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    AddLine(line, LoggingLevel::WARNING, "");
}

void GuiWidgetLog::AddError(const char* fmt, ...)
{
    char line[1000];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    AddLine(line, LoggingLevel::ERROR, "");
}

void GuiWidgetLog::_AddLogLine(LogLine&& logLine)
{
    logLine.match = filter_.Match(logLine.str);
    if (logLine.match) {
        filterNumMatch_++;
    }
    lines_.push_back(std::move(logLine));

    while (lines_.size() > maxLines_) {
        if (lines_[0].match) {
            filterNumMatch_--;
        }
        if (lines_[0].selected) {
            numSelected_--;
        }
        lines_.pop_front();
    }
    _UpdateTooltip();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetLog::_UpdateTooltip()
{
    if (filter_.IsActive()) {
        filter_.SetTooltip(Sprintf("Filter matches %d of %d lines", filterNumMatch_, (int)lines_.size()));
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetLog::DrawWidget()
{
    if (DrawControls()) {
        ImGui::Separator();
    }
    DrawLog();
}

// ---------------------------------------------------------------------------------------------------------------------

std::string GuiWidgetLog::GetFilterStr()
{
    return filter_.GetFilterStr();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetLog::SetFilterStr(const std::string& str)
{
    filter_.SetFilterStr(str);
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetLog::DrawControls()
{
    if (!showControls_) {
        return false;
    }

    ImGui::PushID(this);

    // Options
    if (ImGui::Button(ICON_FK_COG "##Options")) {
        ImGui::OpenPopup("Options");
    }
    Gui::ItemTooltip("Options");
    if (ImGui::BeginPopup("Options")) {
        if (ImGui::Checkbox("Auto-scroll", &cfg_.autoscroll)) {
            scrollToBottom_ = cfg_.autoscroll;
        }
        ImGui::Checkbox("Timestamps", &cfg_.timestamps);
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // Clear
    if (ImGui::Button(ICON_FK_ERASER "##Clear")) {
        Clear();
    }
    Gui::ItemTooltip("Clear log");

    Gui::VerticalSeparator();

    // Filter
    if (filter_.DrawWidget()) {
        // Update filter match flags
        const bool active = filter_.IsActive();
        filterNumMatch_ = 0;
        numSelected_ = 0;
        for (auto& line : lines_) {
            line.match = active ? filter_.Match(line.str) : true;
            if (line.match) {
                filterNumMatch_++;
            }
            if (line.match && line.selected) {
                numSelected_++;
            }
        }
        _UpdateTooltip();
    }

    ImGui::PopID();

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetLog::DrawLog()
{
#if 0
    // Work out longest line and consider that for the horizontal scrolling
    if (lines_.size() > 0) {
        std::size_t maxLen = 0;
        const std::string* maxStr = &lines_[0].str;
        for (auto& line : lines_) {
            if (line.str.size() > maxLen) {
                maxLen = line.str.size();
                maxStr = &line.str;
            }
        }
        if (maxLen > 0) {
            const float maxWidth = ImGui::CalcTextSize(maxStr->c_str()).x;
            // https://github.com/ocornut/imgui/issues/3285
            ImGui::SetNextWindowContentSize(ImVec2(maxWidth + 100, 0.0));  // Add a bit for the timestamp...
        }
    }
#endif

    ImGui::BeginChild("Lines", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImVec2 spacing = GUI_VAR.imguiStyle->ItemInnerSpacing;
    spacing.y *= 0.5;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, spacing);

    const bool haveFilter = filter_.IsActive();
    const bool highlight = haveFilter && filter_.IsHighlight();

    // Displaying all lines -> can use lines_[ix]
    if (!haveFilter || highlight) {
        ImGuiListClipper clipper;
        clipper.Begin(lines_.size());
        while (clipper.Step()) {
            for (int ix = clipper.DisplayStart; ix < clipper.DisplayEnd; ix++) {
                auto& line = lines_[ix];
                if (cfg_.timestamps) {
                    ImGui::PushStyleColor(ImGuiCol_Text, C_C_GREY());
#if 0
                    ImGui::TextUnformatted(line.time.c_str());
#else
                    ImGui::SetNextItemAllowOverlap();
                    ImGui::PushID(ix);
                    if (ImGui::Selectable(line.time.c_str(), line.selected)) {
                        if (line.selected) {
                            numSelected_--;
                        } else {
                            numSelected_++;
                        }
                        line.selected = !line.selected;
                    }
                    ImGui::PopID();
#endif
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                }
                const bool dim = highlight && !line.match;
                if (dim) { ImGui::PushStyleVar(ImGuiStyleVar_Alpha, GUI_VAR.alphaDimVal); }
                if (line.colour != C_AUTO()) { ImGui::PushStyleColor(ImGuiCol_Text, line.colour); }
                ImGui::TextUnformatted(line.str.c_str());
                if (line.colour != C_AUTO()) { ImGui::PopStyleColor(); }
                if (dim) { ImGui::PopStyleVar(); }
            }
        }
        clipper.End();
    }
    // Displaying only some lines
    else {
        ImGuiListClipper clipper;
        clipper.Begin(filterNumMatch_);
        while (clipper.Step()) {
            int linesIx = -1;
            int matchIx = -1;
            for (int ix = clipper.DisplayStart; ix < clipper.DisplayEnd; ix++) {
                // Find (ix+1)-th matching line
                while (matchIx < ix) {
                    while (true) {
                        if (linesIx >= (int)lines_.size()) {
                            matchIx = ix;
                            break;
                        } else if (lines_[++linesIx].match) {
                            matchIx++;
                            break;
                        }
                    }
                }
                if (linesIx < (int)lines_.size()) {
                    auto& line = lines_[linesIx];
                    if (cfg_.timestamps) {
                        ImGui::PushStyleColor(ImGuiCol_Text, C_C_GREY());
#if 0
                        ImGui::TextUnformatted(line.time.c_str());
#else
                        ImGui::SetNextItemAllowOverlap();
                        ImGui::PushID(ix);
                        if (ImGui::Selectable(line.time.c_str(), line.selected)) {
                            if (line.selected) {
                                numSelected_--;
                            } else {
                                numSelected_++;
                            }
                            line.selected = !line.selected;
                        }
                        ImGui::PopID();
#endif
                        ImGui::PopStyleColor();
                        ImGui::SameLine();
                    }
                    if (line.colour != 0) {
                        ImGui::PushStyleColor(ImGuiCol_Text, line.colour);
                    }
                    ImGui::TextUnformatted(line.str.c_str());
                    if (line.colour != 0) {
                        ImGui::PopStyleColor();
                    }
                }
                // else
                //{
                //     ImGui::TextUnformatted("wtf?!");
                // }
            }
        }
    }

    ImGui::PopStyleVar();

    if (scrollToBottom_) {
        ImGui::SetScrollHereY();
        scrollToBottom_ = false;
    }

    // Autoscroll, but only when at end
    if (cfg_.autoscroll && (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    if (ImGui::BeginPopupContextItem("LogSettings")) {
        if (ImGui::Checkbox("Auto-scroll", &cfg_.autoscroll)) {
            scrollToBottom_ = cfg_.autoscroll;
        }
        ImGui::Checkbox("Timestamps", &cfg_.timestamps);
        ImGui::Checkbox("Controls", &showControls_);
        ImGui::Separator();
        if (ImGui::MenuItem("Clear log")) {
            Clear();
        }
        ImGui::Separator();
        ImGui::BeginDisabled(numSelected_ == 0);
        if (ImGui::MenuItem("Deselect all")) {
            for (auto& line : lines_) {
                line.selected = false;
            }
            numSelected_ = 0;
        }
        ImGui::EndDisabled();


            ImGui::Separator();
        if (ImGui::MenuItem("Copy log")) {
            std::string log;
            for (const auto& line : lines_) {
                if ((filterNumMatch_ == 0) || line.match) {
                    log += line.time + " " + line.str + "\n";
                }
            }
            ImGui::SetClipboardText(log.c_str());
        }
        ImGui::BeginDisabled(numSelected_ == 0);
        if (ImGui::MenuItem("Copy selected")) {
            std::string log;
            for (const auto& line : lines_) {
                if (line.selected && ((filterNumMatch_ == 0) || line.match)) {
                    log += line.time + " " + line.str + "\n";
                }
            }
            ImGui::SetClipboardText(log.c_str());
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
