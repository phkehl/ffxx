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
#include <fpsdk_common/parser/ubx.hpp>
using namespace fpsdk::common::parser::ubx;
#include <fpsdk_common/parser/nmea.hpp>
using namespace fpsdk::common::parser::nmea;
//
#include "gui_win_data_custom.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinDataCustom::GuiWinDataCustom(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 85, 25, 85, 20 }, ImGuiWindowFlags_None, input),
    tabbar_   { WinName() },
    hexdump_  { WinName() }  // clang-format on
{
    DEBUG("GuiWinDataCustom(%s)", WinName().c_str());
    GuiGlobal::LoadObj(WinName() + ".GuiWinDataCustom", cfg_);
    GuiGlobal::LoadObj("GuiWinDataCustom", savedCfg_); // static!

    latestEpochMaxAge_ = {};
}

GuiWinDataCustom::~GuiWinDataCustom()
{
    DEBUG("~GuiWinDataCustom(%s)", WinName().c_str());
    GuiGlobal::SaveObj(WinName() + ".GuiWinDataCustom", cfg_);
    GuiGlobal::SaveObj("GuiWinDataCustom", savedCfg_); // static!
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataCustom::_ProcessData(const DataPtr& /*data*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataCustom::_Loop(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataCustom::_ClearData()
{
    // clang-format off
    if      (doHex_)   { cfg_.hex  = {}; }
    else if (doText_)  { cfg_.text = {}; }
    else if (doUbx_)   { cfg_.ubx  = {}; }
    else if (doNmea_)  { cfg_.nmea = {}; }
    // clang-format on
    dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataCustom::_DrawToolbar()
{
    Gui::VerticalSeparator();

    const bool receiverReady = (input_->receiver_ && (input_->receiver_->GetState() == StreamState::CONNECTED));
    ImGui::BeginDisabled(dataMsg_.data_.empty() || !receiverReady);
    if (ImGui::Button(ICON_FK_THUMBS_UP "##Send", GUI_VAR.iconSize)) {
        input_->receiver_->Write(dataMsg_);
    }
    Gui::ItemTooltip("Send");
    ImGui::EndDisabled();

    // Note
    Gui::VerticalSeparator();
    ImGui::SetNextItemWidth(-(2 * GUI_VAR.iconSize.x) - GUI_VAR.imguiStyle->ItemInnerSpacing.x - GUI_VAR.imguiStyle->ItemSpacing.x);
    bool canSave = false;
    auto savedHex = savedCfg_.hex.end();
    auto savedText = savedCfg_.text.end();
    auto savedUbx = savedCfg_.ubx.end();
    auto savedNmea = savedCfg_.nmea.end();
    // clang-format off
    if      (doHex_)  { if (ImGui::InputTextWithHint("##Note", "Note", &cfg_.hex.note))  { dirty_ = true; }
                        savedHex = std::find_if(savedCfg_.hex.begin(), savedCfg_.hex.end(), [note = cfg_.hex.note](const auto& cand) { return note == cand.note; });
                        canSave = !cfg_.hex.note.empty() && ((savedHex == savedCfg_.hex.end()) || (cfg_.hex != *savedHex)); }
    else if (doText_) { if (ImGui::InputTextWithHint("##Note", "Note", &cfg_.text.note)) { dirty_ = true; }
                        savedText = std::find_if(savedCfg_.text.begin(), savedCfg_.text.end(), [note = cfg_.text.note](const auto& cand) { return note == cand.note; });
                        canSave = !cfg_.text.note.empty() && ((savedText == savedCfg_.text.end()) || (cfg_.text != *savedText)); }
    else if (doUbx_)  { if (ImGui::InputTextWithHint("##Note", "Note", &cfg_.ubx.note))  { dirty_ = true; }
                        savedUbx = std::find_if(savedCfg_.ubx.begin(), savedCfg_.ubx.end(), [note = cfg_.ubx.note](const auto& cand) { return note == cand.note; });
                        canSave = !cfg_.ubx.note.empty() && ((savedUbx == savedCfg_.ubx.end()) || (cfg_.ubx != *savedUbx)); }
    else if (doNmea_) { if (ImGui::InputTextWithHint("##Note", "Note", &cfg_.nmea.note)) { dirty_ = true; }
                        savedNmea = std::find_if(savedCfg_.nmea.begin(), savedCfg_.nmea.end(), [note = cfg_.nmea.note](const auto& cand) { return note == cand.note; });
                        canSave = !cfg_.nmea.note.empty() && ((savedNmea == savedCfg_.nmea.end()) || (cfg_.nmea != *savedNmea)); }

    // clang-format on
    WinToggleFlag(ImGuiWindowFlags_UnsavedDocument, canSave);

    // Save
    ImGui::SameLine(0, GUI_VAR.imguiStyle->ItemInnerSpacing.x);
    ImGui::BeginDisabled(!canSave);
    if (ImGui::Button(ICON_FK_FLOPPY_O)) { // clang-format off
        if      (doHex_)  { if (savedHex  != savedCfg_.hex.end())  { *savedHex  = cfg_.hex;  } else { savedCfg_.hex.push_back(cfg_.hex);   std::sort(savedCfg_.hex.begin(), savedCfg_.hex.end()); } }
        else if (doText_) { if (savedText != savedCfg_.text.end()) { *savedText = cfg_.text; } else { savedCfg_.text.push_back(cfg_.text); std::sort(savedCfg_.text.begin(), savedCfg_.text.end()); } }
        else if (doUbx_)  { if (savedUbx  != savedCfg_.ubx.end())  { *savedUbx  = cfg_.ubx;  } else { savedCfg_.ubx.push_back(cfg_.ubx);   std::sort(savedCfg_.ubx.begin(), savedCfg_.ubx.end()); } }
        else if (doNmea_) { if (savedNmea != savedCfg_.nmea.end()) { *savedNmea = cfg_.nmea; } else { savedCfg_.nmea.push_back(cfg_.nmea); std::sort(savedCfg_.nmea.begin(), savedCfg_.nmea.end()); } } // clang-format on
    }
    Gui::ItemTooltip("Save message");
    ImGui::EndDisabled();

    // Load
    ImGui::BeginDisabled(savedCfg_.hex.empty() && savedCfg_.text.empty() && savedCfg_.ubx.empty() && savedCfg_.nmea.empty());
    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_FOLDER_O)) {
        ImGui::OpenPopup("Load");
    }
    Gui::ItemTooltip("Load message");
    ImGui::EndDisabled();
    if (ImGui::BeginPopup("Load")) {

        const ImVec2 dummySize(3 * GUI_VAR.charSizeX, GUI_VAR.lineHeight);
        const auto a0 = ImGui::GetContentRegionAvail();
        const float trashOffs = a0.x - (2 * GUI_VAR.charSizeX);
        std::size_t id = 1;

        Gui::TextTitle("Hex");
        if (savedCfg_.hex.empty()) {
            Gui::TextDim("No saved Hex messages");
        }
        for (auto it = savedCfg_.hex.begin(); it != savedCfg_.hex.end(); ) {
            ImGui::PushID(id++);
            ImGui::SetNextItemAllowOverlap();
            if (ImGui::Selectable(
                    Sprintf("%.30s%s", it->note.c_str(), it->note.size() > 30 ? "..." : "").c_str())) {
                cfg_.hex = *it;
                dirty_ = true;
                tabbar_.Switch("Hex");
            }
            ImGui::SameLine();
            ImGui::Dummy(dummySize);
            ImGui::SameLine(trashOffs);
            bool remove = ImGui::SmallButton(ICON_FK_TRASH);
            Gui::ItemTooltip("Remove message");
            ImGui::PopID();
            if (remove) {
                it = savedCfg_.hex.erase(it);
            } else {
                it++;
            }
        }

        ImGui::Separator();

        Gui::TextTitle("Text");
        if (savedCfg_.text.empty()) {
            Gui::TextDim("No saved Text messages");
        }
        for (auto it = savedCfg_.text.begin(); it != savedCfg_.text.end(); ) {
            ImGui::PushID(id++);
            ImGui::SetNextItemAllowOverlap();
            if (ImGui::Selectable(
                    Sprintf("%.30s%s", it->note.c_str(), it->note.size() > 30 ? "..." : "").c_str())) {
                cfg_.text = *it;
                dirty_ = true;
                tabbar_.Switch("Text");
            }
            ImGui::SameLine();
            ImGui::Dummy(dummySize);
            ImGui::SameLine(trashOffs);
            bool remove = ImGui::SmallButton(ICON_FK_TRASH);
            Gui::ItemTooltip("Remove message");
            ImGui::PopID();
            if (remove) {
                it = savedCfg_.text.erase(it);
            } else {
                it++;
            }
        }

        ImGui::Separator();

        Gui::TextTitle("UBX");
        if (savedCfg_.ubx.empty()) {
            Gui::TextDim("No saved UBX messages");
        }
        for (auto it = savedCfg_.ubx.begin(); it != savedCfg_.ubx.end(); ) {
            ImGui::PushID(id++);
            ImGui::SetNextItemAllowOverlap();
            if (ImGui::Selectable(
                    Sprintf("%.30s%s", it->note.c_str(), it->note.size() > 30 ? "..." : "").c_str())) {
                cfg_.ubx = *it;
                dirty_ = true;
                tabbar_.Switch("UBX");
            }
            ImGui::SameLine();
            ImGui::Dummy(dummySize);
            ImGui::SameLine(trashOffs);
            bool remove = ImGui::SmallButton(ICON_FK_TRASH);
            Gui::ItemTooltip("Remove message");
            ImGui::PopID();
            if (remove) {
                it = savedCfg_.ubx.erase(it);
            } else {
                it++;
            }
        }

        ImGui::Separator();

        Gui::TextTitle("NMEA");
        if (savedCfg_.nmea.empty()) {
            Gui::TextDim("No saved NMEA messages");
        }
        for (auto it = savedCfg_.nmea.begin(); it != savedCfg_.nmea.end(); ) {
            ImGui::PushID(id++);
            ImGui::SetNextItemAllowOverlap();
            if (ImGui::Selectable(
                    Sprintf("%.30s%s", it->note.c_str(), it->note.size() > 30 ? "..." : "").c_str())) {
                cfg_.nmea = *it;
                dirty_ = true;
                tabbar_.Switch("NMEA");
            }
            ImGui::SameLine();
            ImGui::Dummy(dummySize);
            ImGui::SameLine(trashOffs);
            bool remove = ImGui::SmallButton(ICON_FK_TRASH);
            Gui::ItemTooltip("Remove message");
            ImGui::PopID();
            if (remove) {
                it = savedCfg_.nmea.erase(it);
            } else {
                it++;
            }
        }

        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataCustom::_DrawContent()
{
    if (tabbar_.Begin()) {
        tabbar_.Item("Hex", dirty_, doHex_);
        tabbar_.Item("Text", dirty_, doText_);
        tabbar_.Item("UBX", dirty_, doUbx_);
        tabbar_.Item("NMEA", dirty_, doNmea_);
        tabbar_.End();
    }

    const float sepHeight = (2 * GUI_VAR.imguiStyle->ItemSpacing.y) + 1;
    const ImVec2 sizeAvail = ImGui::GetContentRegionAvail();
    const ImVec2 dumpSize{ -1, std::max(sizeAvail.y * 0.4f, 5 * GUI_VAR.charSizeY) };
    const ImVec2 editSize{ sizeAvail.x, sizeAvail.y - dumpSize.y - sepHeight };

    if (ImGui::BeginChild("##Edit", editSize)) {  // clang-format off
        if      (doHex_)   { _DrawHex(); }
        else if (doText_)  { _DrawText(); }
        else if (doUbx_)   { _DrawUbx(); }
        else if (doNmea_)  { _DrawNmea(); }
    }  // clang-format on
    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::BeginChild("##Dump", dumpSize)) {
        if (dataMsg_.data_.empty()) {
            Gui::TextDim("No message yet");
        } else {
            ImGui::Text("Message: %s (size %" PRIuMAX ")", dataMsg_.name_.c_str(), dataMsg_.Size());
            if (!dataMsg_.info_.empty() && (dataMsg_.proto_ != Protocol::OTHER)) {
                ImGui::TextUnformatted(dataMsg_.info_.c_str());
            }
            hexdump_.DrawWidget();
        }
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataCustom::_DrawHex()
{
    if (!rawHexOk_) {
        ImGui::PushStyleColor(ImGuiCol_Border, C_TEXT_ERROR());
    }

    ImGui::TextUnformatted("Payload");
    ImGui::SameLine();
    Gui::TextDim("(space spearated hex bytes)");
    if (ImGui::InputTextMultiline("##RawHex", &cfg_.hex.payload, {-FLT_MIN, -FLT_MIN}, ImGuiInputTextFlags_AllowTabInput)) {
        dirty_ = true;
    }
    if (!rawHexOk_) {
        ImGui::PopStyleColor();
    }

    if (dirty_) {
        if (cfg_.hex.payload.empty()) {
            rawHexOk_ = true;
            _SetData({});
        } else {
            const auto data = _HexToBin(cfg_.hex.payload);
            rawHexOk_ = !data.empty();
            _SetData(data);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataCustom::_DrawText()
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Payload");
    ImGui::SameLine();
    Gui::TextDim("(anything)");

    ImGui::SameLine(0, GUI_VAR.charSizeX * 5);
    ImGui::TextUnformatted("Line endings:");
    ImGui::SameLine();
    if (ImGui::RadioButton("LF", !cfg_.text.crlf)) {
        cfg_.text.crlf = !cfg_.text.crlf;
        dirty_ = true;
    }
    ImGui::SameLine();
    Gui::ItemTooltip("Don't replace LF (0x0a) with CRLF (0x0a 0x0d)");
    if (ImGui::RadioButton("CRLF", cfg_.text.crlf)) {
        cfg_.text.crlf = !cfg_.text.crlf;
        dirty_ = true;
    }
    Gui::ItemTooltip("Replace LF (0x0a) with CRLF (0x0a 0x0d)");

    if (ImGui::InputTextMultiline("##RawText", &cfg_.text.payload, { -FLT_MIN, -FLT_MIN },  ImGuiInputTextFlags_AllowTabInput)) {
        dirty_ = true;
    }
    if (dirty_) {
        std::string copy = cfg_.text.payload;
        if (cfg_.text.crlf) {
            StrReplace(copy, "\n", "\r\n");
        }
        _SetData(StrToBuf(copy));
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataCustom::_DrawUbx()
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Message");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(GUI_VAR.charSizeX * 20);
    if (ImGui::BeginCombo("##msgName", cfg_.ubx.message.c_str(), ImGuiComboFlags_HeightLarge)) {
        if (ImGui::Selectable("##None", cfg_.ubx.message.empty())) {
            cfg_.ubx.clsId = 0x00;
            cfg_.ubx.msgId = 0x00;
            dirty_ = true;
        }
        if (cfg_.ubx.message.empty()) {
            ImGui::SetItemDefaultFocus();
        }
        for (const auto& msg : UbxGetMessagesInfo()) {
            const bool selected = (cfg_.ubx.message == msg.name_);
            if (ImGui::Selectable(msg.name_, selected)) {
                cfg_.ubx.message = msg.name_;
                cfg_.ubx.clsId = msg.cls_id_;
                cfg_.ubx.msgId = msg.msg_id_;
                dirty_ = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    Gui::VerticalSeparator();
    bool idsChanged = false;
    ImGui::TextUnformatted("Cls ID");
    ImGui::SameLine();
    ImGui::SetNextItemWidth((GUI_VAR.charSizeX * 2) + (GUI_VAR.imguiStyle->ItemInnerSpacing.x * 2));
    if (ImGui::InputScalar(
            "##ClsId", ImGuiDataType_U8, &cfg_.ubx.clsId, NULL, NULL, "%02x", ImGuiInputTextFlags_CharsHexadecimal)) {
        idsChanged = true;
        dirty_ = true;
    }
    Gui::VerticalSeparator();
    ImGui::TextUnformatted("Msg ID");
    ImGui::SameLine();
    ImGui::SetNextItemWidth((GUI_VAR.charSizeX * 2) + (GUI_VAR.imguiStyle->ItemInnerSpacing.x * 2));
    if (ImGui::InputScalar(
            "##MsgId", ImGuiDataType_U8, &cfg_.ubx.msgId, NULL, NULL, "%02x", ImGuiInputTextFlags_CharsHexadecimal)) {
        idsChanged = true;
        dirty_ = true;
    }
    Gui::VerticalSeparator();
    ImGui::TextUnformatted("Size");
    ImGui::SameLine();
    ImGui::SetNextItemWidth((GUI_VAR.charSizeX * 5) + (GUI_VAR.imguiStyle->ItemInnerSpacing.x * 2));
    if (!ubxSizeOk_) {
        ImGui::PushStyleColor(ImGuiCol_Border, C_TEXT_ERROR());
    }
    if (ImGui::InputScalar(
            "##Size", ImGuiDataType_U16, &cfg_.ubx.size, NULL, NULL, "%u", ImGuiInputTextFlags_CharsDecimal)) {
        cfg_.ubx.sizeUser = true;
        dirty_ = true;
    }
    if (!ubxSizeOk_) {
        ImGui::PopStyleColor();
    }
    Gui::VerticalSeparator();
    ImGui::TextUnformatted("Checksum");
    ImGui::SameLine();
    ImGui::SetNextItemWidth((GUI_VAR.charSizeX * 4) + (GUI_VAR.imguiStyle->ItemInnerSpacing.x * 2));
    if (!ubxCkOk_) {
        ImGui::PushStyleColor(ImGuiCol_Border, C_TEXT_ERROR());
    }
    if (ImGui::InputScalar("##Checksum", ImGuiDataType_U16, &cfg_.ubx.checksum, NULL, NULL, "%04x",
            ImGuiInputTextFlags_CharsHexadecimal)) {
        cfg_.ubx.ckUser = true;
        dirty_ = true;
    }
    if (!ubxCkOk_) {
        ImGui::PopStyleColor();
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Payload");
    ImGui::SameLine();
    Gui::TextDim("(space spearated hex bytes)");
    if (!ubxPayloadOk_) {
        ImGui::PushStyleColor(ImGuiCol_Border, C_TEXT_ERROR());
    }
    if (ImGui::InputTextMultiline("##EditUbx", &cfg_.ubx.payload, { -FLT_MIN, -FLT_MIN }, ImGuiInputTextFlags_AllowTabInput)) {
        cfg_.ubx.sizeUser = false;
        cfg_.ubx.ckUser = false;
        dirty_ = true;
    }
    if (!ubxPayloadOk_) {
        ImGui::PopStyleColor();
    }

    if (dirty_) {
        if (idsChanged) {
            cfg_.ubx.message.clear();
            for (auto& msg : UbxGetMessagesInfo()) {
                if ((msg.cls_id_ == cfg_.ubx.clsId) && (msg.msg_id_ == cfg_.ubx.msgId)) {
                    cfg_.ubx.message = msg.name_;
                    break;
                }
            }
        }

        std::vector<uint8_t> payload;
        if (cfg_.ubx.payload.empty()) {
            ubxPayloadOk_ = true;
        } else {
            payload = _HexToBin(cfg_.ubx.payload);
            ubxPayloadOk_ = !payload.empty();
        }

        std::vector<uint8_t> msg;
        UbxMakeMessage(msg, cfg_.ubx.clsId, cfg_.ubx.msgId, payload.data(), payload.size());

        if (cfg_.ubx.ckUser) {
            const uint8_t c1 = (cfg_.ubx.checksum >> 8) & 0xff;
            const uint8_t c2 = cfg_.ubx.checksum & 0xff;
            ubxCkOk_ = ((msg[msg.size() - 2] == c1) && (msg[msg.size() - 1] == c2));
            msg[msg.size() - 2] = c1;
            msg[msg.size() - 1] = c2;
        } else {
            cfg_.ubx.checksum = ((uint16_t)msg[msg.size() - 2] << 8) | msg[msg.size() - 1];
            ubxCkOk_ = true;
        }

        if (cfg_.ubx.sizeUser) {
            const uint8_t s1 = cfg_.ubx.size & 0xff;
            const uint8_t s2 = (cfg_.ubx.size >> 8) & 0xff;
            ubxSizeOk_ = ((msg[4] == s1) && (msg[5] != s2));
            msg[4] = s1;
            msg[5] = s2;
        } else {
            cfg_.ubx.size = payload.size();
            ubxSizeOk_ = true;
        }

        if (ubxPayloadOk_) {
            _SetData(msg);
        } else {
            _SetData({});
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataCustom::_DrawNmea()
{
    // clang-format off
    static constexpr std::array<const char*, 8> TALKERS = {{
        "", "GP", "GL", "GA", "GB", "GQ", "GN", "P"
    }};
    static constexpr std::array<const char*, 18> FORMATTERS = {{
        "", "DTM", "GBS", "GGA", "GLL", "GNS", "GRS", "GSA", "GSV", "RLM", "RMC", "THS", "VLW", "VTG", "ZDA", "UBX", "AIR", "QTM"
    }};
    // clang-format on

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Talker");
    ImGui::SameLine();
    ImGui::SetNextItemWidth((GUI_VAR.charSizeX * 3) + (GUI_VAR.imguiStyle->ItemInnerSpacing.x * 2));
    if (ImGui::InputText(
            "##Talker", &cfg_.nmea.talker, ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank) ||
        (cfg_.nmea.talkerIx < 0)) {
        dirty_ = true;
        cfg_.nmea.talkerIx = 0;
        for (int ix = 0; ix < (int)TALKERS.size(); ix++) {
            if (cfg_.nmea.talker == TALKERS[ix]) {
                cfg_.nmea.talkerIx = ix;
                break;
            }
        }
    }
    ImGui::SameLine(0, 0);
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight());
    if (ImGui::Combo("##TalkerChoice", &cfg_.nmea.talkerIx, TALKERS.data(), TALKERS.size())) {
        dirty_ = true;
        if (cfg_.nmea.talkerIx >= 0) {
            cfg_.nmea.talker = TALKERS[cfg_.nmea.talkerIx];
        }
    }
    Gui::VerticalSeparator();
    ImGui::TextUnformatted("Formatter");
    ImGui::SameLine();
    ImGui::SetNextItemWidth((GUI_VAR.charSizeX * 4) + (GUI_VAR.imguiStyle->ItemInnerSpacing.x * 2));
    if (ImGui::InputText("##Formatter", &cfg_.nmea.formatter,
            ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank) ||
        (cfg_.nmea.formatterIx < 0)) {
        dirty_ = true;
        cfg_.nmea.formatterIx = 0;
        for (int ix = 0; ix < (int)FORMATTERS.size(); ix++) {
            if (cfg_.nmea.formatter == FORMATTERS[ix]) {
                cfg_.nmea.formatterIx = ix;
                break;
            }
        }
    }
    ImGui::SameLine(0, 0);
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight());
    if (ImGui::Combo("##FormatterChoice", &cfg_.nmea.formatterIx, FORMATTERS.data(), FORMATTERS.size())) {
        dirty_ = true;
        if (cfg_.nmea.formatterIx >= 0) {
            cfg_.nmea.formatter = FORMATTERS[cfg_.nmea.formatterIx];
        }
    }
    Gui::VerticalSeparator();
    ImGui::TextUnformatted("Checksum");
    ImGui::SameLine();
    ImGui::SetNextItemWidth((GUI_VAR.charSizeX * 2) + (GUI_VAR.imguiStyle->ItemInnerSpacing.x * 2));
    if (!nmeaCkOk_) {
        ImGui::PushStyleColor(ImGuiCol_Border, C_TEXT_ERROR());
    }
    if (ImGui::InputText("##Checksum", &cfg_.nmea.checksum,
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsNoBlank)) {
        cfg_.nmea.ckUser = true;
        dirty_ = true;
    }
    if (!nmeaCkOk_) {
        ImGui::PopStyleColor();
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Payload");
    ImGui::SameLine();
    Gui::TextDim("(valid NMEA payload characters)");
    if (!nmeaPayloadOk_) {
        ImGui::PushStyleColor(ImGuiCol_Border, C_TEXT_ERROR());
    }
    if (ImGui::InputTextMultiline("##EditNmea", &cfg_.nmea.payload, { -FLT_MIN, -FLT_MIN })) {
        cfg_.nmea.ckUser = false;
        dirty_ = true;
    }
    if (!nmeaPayloadOk_) {
        ImGui::PopStyleColor();
    }

    if (dirty_) {
        // Check payload
        nmeaPayloadOk_ = true;
        for (const char c : cfg_.nmea.payload) {  // clang-format off
            if ((c < 0x20) || (c > 0x7e) ||                               // invalid range,
                (c == '$') || (c == '\\') || (c == '!') || (c == '~') ||  // reserved...
                (c == '^') || (c == '*'))                                 // ...or sepecial (but do allow ',')
            {  // clang-format on
                nmeaPayloadOk_ = false;
                break;
            }
        }
        if (nmeaPayloadOk_) {
            std::vector<uint8_t> m;
            NmeaMakeMessage(m, cfg_.nmea.talker + cfg_.nmea.formatter + cfg_.nmea.payload);
            auto msg = BufToStr(m);

            if (cfg_.nmea.ckUser) {
                const std::string ck = msg.substr(msg.size() - 4, 2);
                nmeaCkOk_ = (ck == cfg_.nmea.checksum);
                msg.replace(msg.size() - 4, 2, cfg_.nmea.checksum);
            } else {
                cfg_.nmea.checksum = msg.substr(msg.size() - 4, 2);
                nmeaCkOk_ = true;
            }
            _SetData(StrToBuf(msg));

        } else {
            _SetData({});
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataCustom::_SetData(const std::vector<uint8_t>& data)
{
    hexdump_.Clear();
    dataMsg_ = {};

    // Check if it is a single known message
    parser_.Reset();
    ParserMsg msg;
    if (!data.empty() && parser_.Add(data) && parser_.Process(msg) && (msg.proto_ != Protocol::OTHER) &&
        (msg.Size() == data.size())) {
        dataMsg_ = msg;
    }
    // It's something OTHER
    else if (!data.empty()) {
        dataMsg_.proto_ = Protocol::OTHER;
        dataMsg_.data_ = data;
        dataMsg_.name_ = ProtocolStr(Protocol::OTHER);
    }

    if (!dataMsg_.data_.empty()) {
        dataMsg_.MakeInfo();
        hexdump_.Set(dataMsg_.data_);
    }

    dirty_ = false;
}

// ---------------------------------------------------------------------------------------------------------------------

std::vector<uint8_t> GuiWinDataCustom::_HexToBin(const std::string& hex) const
{
    std::string copy = hex;
    StrReplace(copy, "\n", " ");
    StrReplace(copy, "\t", " ");
    StrTrim(copy);
    int iter = 100;
    while ((StrReplace(copy, "  ", " ") > 0) && (iter > 0)) {
        iter--;
    }
    std::vector<uint8_t> data;
    bool ok = true;
    for (auto part : StrSplit(copy, " ")) {
        int n = 0;
        uint8_t byte = 0;
        if ((std::sscanf(part.c_str(), "%" SCNx8 "%n", &byte, &n) == 1) && (n == 2)) {
            data.push_back(byte);
        } else {
            ok = false;
            break;
        }
    }
    if (!ok) {
        data.clear();
    }
    return data;
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ GuiWinDataCustom::SavedConfig GuiWinDataCustom::savedCfg_ {};

/* ****************************************************************************************************************** */
}  // namespace ffgui
