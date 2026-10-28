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
#include <cctype>
//
#include "gui_widget_hexdump.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWidgetHexdump::GuiWidgetHexdump(const std::string& name, const std::size_t maxSize) /* clang-format off */ :
    name_      { name },
    maxSize_   { std::clamp<std::size_t>(maxSize, 1024, 1 * 1024 * 1024) }// clang-format on
{
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWidgetHexdump::~GuiWidgetHexdump()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetHexdump::Set(const std::vector<uint8_t>& data)
{
    data_ = data;
    dump_ = HexDump(data_);
    isText_ = std::all_of(data_.data(), data_.data() + data_.size(),
              [](const uint8_t c) { return std::isprint(c) || std::isspace(c); });
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetHexdump::Clear()
{
    dump_.clear();
    data_.clear();
    isText_ = false;
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetHexdump::Empty() const {
    return data_.empty();
}

// ---------------------------------------------------------------------------------------------------------------------

std::size_t GuiWidgetHexdump::Lines() const {
    return dump_.size();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetHexdump::DrawWidget(const ImVec2& size)
{
    if (ImGui::BeginChild(name_.c_str(), size)) {
        if (Empty()) {
            Gui::TextDim("No data");
            DEBUG("empty");
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (const auto& line : dump_) {
                ImGui::TextUnformatted(line.c_str());
            }
            ImGui::PopStyleVar();
        }
    }
    ImGui::EndChild();
    if (ImGui::BeginPopupContextItem("Copy")) {
        std::string text = "";
        ImGui::BeginDisabled(Empty());
        if (ImGui::MenuItem("Copy (full dump)")) {
            for (const auto& line : dump_) {
                text += line + "\n";
            }
        }
        if (ImGui::MenuItem("Copy (hex only)")) {
            for (const auto& line : dump_) {
                text += line.substr(14, 48) + "\n";
            }
        }
        if (ImGui::MenuItem("Copy (hex only, one line)")) {
            for (const auto& line : dump_) {
                text += line.substr(14, 48) + " ";
            }
            text.resize(text.size() - 1);
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!isText_ || Empty());
        if (ImGui::MenuItem("Copy (string)")) {
            text = BufToStr(data_);
        }
        ImGui::EndDisabled();

        if (!text.empty()) {
            ImGui::SetClipboardText(text.c_str());
        }
        ImGui::EndPopup();
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
