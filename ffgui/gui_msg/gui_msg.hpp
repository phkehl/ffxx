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

#ifndef __GUI_MSG_HPP__
#define __GUI_MSG_HPP__

//
#include "ffgui_inc.hpp"
//
#include "input.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsg
{
   public:
    GuiMsg(const std::string& viewName, const InputPtr& input);
    virtual ~GuiMsg() = default;

    virtual void Update(const DataMsgPtr& msg) = 0;
    virtual void Update(const Time& now) = 0;

    virtual void DrawToolbar() = 0;
    virtual void DrawMsg() = 0;
    virtual void Clear() = 0;

    static std::unique_ptr<GuiMsg> GetInstance(const std::string& msgName, const std::string& winName, const InputPtr& input);

   protected:
    std::string viewName_;
    InputPtr input_;

    static constexpr ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody |
                                                   ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingFixedFit |
                                                   ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
    struct StatusFlag
    {
        int value = 0;
        const char* text = nullptr;
        std::size_t colourIx = CIX_AUTO;
    };

    ImVec2 _CalcTopSize(const int nLinesOfTopText);
    void _RenderStatusText(const std::string& label, const std::string& text, const float dataOffs);
    void _RenderStatusText(const std::string& label, const char* text, const float dataOffs);
    void _RenderStatusText(const char* label, const std::string& text, const float dataOffs);
    void _RenderStatusText(const char* label, const char* text, const float dataOffs);
    template <typename StatusFlagsT>
    void _RenderStatusFlag(const StatusFlagsT& flags, const int value, const char* label, const float offs)
    {
        ImGui::TextUnformatted(label);
        ImGui::SameLine(offs);

        for (const auto& f : flags) {
            if (value == f.value) {
                const bool colour = (f.colourIx != CIX_AUTO);
                if (colour) {
                    ImGui::PushStyleColor(ImGuiCol_Text, IX_COLOUR(f.colourIx));
                }
                ImGui::TextUnformatted(f.text);
                if (colour) {
                    ImGui::PopStyleColor();
                }
                return;
            }
        }

        char str[100];
        snprintf(str, sizeof(str), "? (%d)", value);
        ImGui::TextUnformatted(str);
    }
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_HPP__
