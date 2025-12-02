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
#include <fpsdk_common/math.hpp>
using namespace fpsdk::common::math;
//
#include "gui_win_hexdecbin.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinHexDecBin::GuiWinHexDecBin() /* clang-format off */ :
    GuiWin("HexDecBin", { 75.0, 10.0, 0.0, 0.0 }, ImGuiWindowFlags_NoDocking)  // clang-format on
{
    DEBUG("GuiWinHexDecBin(%s)", WinName().c_str());
    GuiGlobal::LoadObj(WinName() + ".GuiWinHexDecBin", cfg_);

}

GuiWinHexDecBin::~GuiWinHexDecBin()
{
    DEBUG("~GuiWinHexDecBin(%s)", WinName().c_str());

    GuiGlobal::SaveObj(WinName() + ".GuiWinHexDecBin", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinHexDecBin::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }

    if (cfg_.entries.empty()) {
        cfg_.entries.push_back(Entry());
    }

    //const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 size1(GUI_VAR.iconSize.x + (2.0f * GUI_VAR.imguiStyle->ItemInnerSpacing.x), (3.0f * GUI_VAR.lineHeight) + (4.0f * GUI_VAR.imguiStyle->ItemInnerSpacing.y));
    const ImVec2 size2(-FLT_MIN, size1.y);

    uint32_t id = 1;
    for (auto it = cfg_.entries.begin(); it != cfg_.entries.end();) {
        ImGui::PushID(id++);
        auto& entry = *it;

        bool remove = false;
        bool add = false;
        if (ImGui::BeginChild("buttons", size1, ImGuiChildFlags_None, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse)) {
            remove = ImGui::Button(ICON_FK_TRASH_O, GUI_VAR.iconSize);
            ImGui::BeginDisabled(cfg_.entries.size() >= MAX_ENTRIES);
            add = ImGui::Button(ICON_FK_PLUS, GUI_VAR.iconSize);
            ImGui::EndDisabled();
            if (ImGui::Button(ICON_FK_CLIPBOARD, GUI_VAR.iconSize)) { ImGui::OpenPopup("Copy"); }
            if (ImGui::BeginPopup("Copy")) {
                if (ImGui::Selectable("Copy hex value")) {
                    ImGui::SetClipboardText(Sprintf("0x%016" PRIx64, entry.val).c_str());
                }
                if (ImGui::Selectable("Copy dec value")) {
                    ImGui::SetClipboardText(Sprintf("%" PRIu64, entry.val).c_str());
                }
                if (ImGui::Selectable("Copy hex dump")) {
                    const Value v = { .u64 = entry.val };
                    ImGui::SetClipboardText(Sprintf("%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
                        v.bytes[0], v.bytes[1], v.bytes[2], v.bytes[3], v.bytes[4], v.bytes[5], v.bytes[6], v.bytes[7]).c_str());
                }
                ImGui::EndPopup();
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        if (remove) {
            it = cfg_.entries.erase(it);
        } else if (add) {
            it = cfg_.entries.insert(it, Entry(entry));
            it++;
        } else {
            it++;
        }

        if (ImGui::BeginChild("entry", size2, ImGuiChildFlags_None, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse)) {
            _DrawEntry(entry);
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::PopID();
    }

    _DrawWindowEnd();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinHexDecBin::_DrawEntry(Entry& entry)
{
    Value val = { .u64 = entry.val };
    // clang-format off

    const float w64 = GUI_VAR.charSize.x * (16.0f + 2.0f);
    const float w32 = GUI_VAR.charSize.x * ( 8.0f + 2.0f);
    const float w16 = GUI_VAR.charSize.x * ( 4.0f + 2.0f);
    const float w8  = GUI_VAR.charSize.x * ( 2.0f + 2.0f);

    ImGui::AlignTextToFramePadding();

    ImGui::TextUnformatted("X64");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(w64);
    ImGui::InputScalar("##x64", ImGuiDataType_U64, &val.u64, NULL, NULL, "%016" PRIx64, ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::TextUnformatted("X32");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(w32);
    if (ImGui::InputScalar("##x32", ImGuiDataType_U32, &val.u32, NULL, NULL, "%08" PRIx32, ImGuiInputTextFlags_CharsHexadecimal)) { val.u64 = val.u32; }
    ImGui::SameLine();
    ImGui::TextUnformatted("X16");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(w16);
    if (ImGui::InputScalar("##x16", ImGuiDataType_U16, &val.u16, NULL, NULL, "%04" PRIx16, ImGuiInputTextFlags_CharsHexadecimal)) { val.u64 = val.u16; }
    ImGui::SameLine();
    ImGui::TextUnformatted("X8");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(w8);
    if (ImGui::InputScalar("##x8", ImGuiDataType_U8, &val.u8, NULL, NULL, "%02" PRIx8, ImGuiInputTextFlags_CharsHexadecimal)) { val.u64 = val.u8; }
    ImGui::SameLine();
    ImGui::TextUnformatted("X");
    for (int ix = 0; ix < 8; ix++) {
        ImGui::PushID(ix);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(w8);
        ImGui::InputScalar("##bytes", ImGuiDataType_U8, &val.bytes[ix], NULL, NULL, "%02" PRIx8, ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::PopID();
    }

    ImGui::AlignTextToFramePadding();

    ImGui::TextUnformatted("U64");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(w64);
    ImGui::InputScalar("##u64", ImGuiDataType_U64, &val.u64, NULL, NULL, "%" PRIu64, ImGuiInputTextFlags_CharsDecimal);
    ImGui::SameLine();
    ImGui::TextUnformatted("U32");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(w32);
    if (ImGui::InputScalar("##u32", ImGuiDataType_U32, &val.u32, NULL, NULL, "%" PRIu32, ImGuiInputTextFlags_CharsDecimal)) { val.u64 = val.u32; }
    ImGui::SameLine();
    ImGui::TextUnformatted("U16");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(w16);
    if (ImGui::InputScalar("##u16", ImGuiDataType_U16, &val.u16, NULL, NULL, "%" PRIu16, ImGuiInputTextFlags_CharsDecimal)) { val.u64 = val.u16; }
    ImGui::SameLine();
    ImGui::TextUnformatted("U8");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(w8);
    if (ImGui::InputScalar("##u8", ImGuiDataType_U8, &val.u8, NULL, NULL, "%" PRIu8, ImGuiInputTextFlags_CharsDecimal)) { val.u64 = val.u8; }
    ImGui::SameLine();
    ImGui::TextUnformatted("C");
    for (int ix = 0; ix < 8; ix++) {
        ImGui::PushID(ix);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(w8);
        char c[2] = { (char)val.bytes[ix], '\0' };
        if (ImGui::InputText("##char", c, sizeof(c))) { val.bytes[ix] = (uint8_t)c[0]; }
        ImGui::PopID();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("B");
    for (int ix = 7; ix >= 0; ix--) {
        uint8_t b = val.bytes[ix];
        char bits[4+1+4+1] = {
            CheckBitsAll(b, Bit<uint8_t>(7)) ? '1' : '0',
            CheckBitsAll(b, Bit<uint8_t>(6)) ? '1' : '0',
            CheckBitsAll(b, Bit<uint8_t>(5)) ? '1' : '0',
            CheckBitsAll(b, Bit<uint8_t>(4)) ? '1' : '0',
            ' ',
            CheckBitsAll(b, Bit<uint8_t>(3)) ? '1' : '0',
            CheckBitsAll(b, Bit<uint8_t>(2)) ? '1' : '0',
            CheckBitsAll(b, Bit<uint8_t>(1)) ? '1' : '0',
            CheckBitsAll(b, Bit<uint8_t>(0)) ? '1' : '0', 0x00 };

        ImGui::SameLine(0.0f, GUI_VAR.charSizeX * 2.0f);
        ImGui::TextUnformatted(bits);
    }

    // clang-format on
    entry.val = val.u64;
}


/* ****************************************************************************************************************** */
}  // namespace ffgui
