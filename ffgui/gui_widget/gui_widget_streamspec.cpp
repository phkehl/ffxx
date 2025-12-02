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
#include <functional>
#include <regex>
//
#include "gui_widget_streamspec.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWidgetStreamSpec::GuiWidgetStreamSpec(const std::string& name, HistoryEntries& history, OnBaudrateFn onBaudrateFn) /* clang-format off */ :
    name_           { name },
    id_             { "##" + name_ },
    history_        { name, history, 0, std::bind(&GuiWidgetStreamSpec::_AddHistoryEntries, this) },
    onBaudrateFn_   { onBaudrateFn }  // clang-format on
{
    GuiGlobal::LoadObj(name_ + ".GuiWidgetStreamSpec", cfg_);
    _Update();
}

GuiWidgetStreamSpec::~GuiWidgetStreamSpec()
{
    GuiGlobal::SaveObj(name_ + ".GuiWidgetStreamSpec", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetStreamSpec::DrawWidget(const bool disabled, const bool allowBaudrate, const float width)
{
    bool res = false;
    ImGui::PushID(this);

    // -- Baudrate stuff for streams that can do it --

    ImGui::BeginDisabled(!allowBaudrate || (baudrate_ == 0));
    if (ImGui::Button(ICON_FK_TACHOMETER "##BaudOpts", GUI_VAR.iconSize)) {
        ImGui::OpenPopup("BaudOpts");
    }
    ImGui::EndDisabled();
    Gui::ItemTooltip("Baudrate");
    if (ImGui::BeginPopup("BaudOpts")) {
        ImGui::Separator();
        Gui::TextTitle("Baudrate");
        ImGui::Separator();
        ImGui::BeginDisabled(!allowBaudrate);
        for (std::size_t ix = 0; ix < StreamOpts::BAUDRATES.size(); ix++) {
            if (ImGui::RadioButton(StreamOpts::BAUDRATE_FANCY_STRS[ix], StreamOpts::BAUDRATES[ix] == baudrate_)) {
                SetBaudrate(StreamOpts::BAUDRATES[ix]);
                if (onBaudrateFn_) {
                    onBaudrateFn_(baudrate_);
                }
            }
        }
        ImGui::Separator();
        Gui::TextTitle("Autobauding");
        ImGui::Separator();
        // clang-format off
        static const struct { AutobaudMode mode; const char* str; const char* tooltip; } AB[] = {
            { AutobaudMode::AUTO,    "Auto",        "Automatic (poll UBX-MON-VER and FP_B-VERSION))" },
            { AutobaudMode::PASSIVE, "Passive",     "Passive autobauding (accept any message)"       },
            { AutobaudMode::UBX,     "u-blox",      "u-blox receivers (poll UBX-MON-VER)"            },
            { AutobaudMode::FP,      "Fixposition", "Fixposition sensors (poll FP_B-VERSION)"        },
            { AutobaudMode::NONE,    "None",        "No autobauding, use specifed baudrate"          } };  // clang-format on
        for (const auto& ab : AB) {
            if (ImGui::RadioButton(ab.str, ab.mode == abMode_)) {
                SetAbMode(ab.mode);
            }
            Gui::ItemTooltip(ab.tooltip);
        }
        ImGui::EndDisabled();

        ImGui::EndPopup();
    }

    ImGui::SameLine(0, GUI_VAR.imguiStyle->ItemInnerSpacing.x);

    // ----- Text input -----

    ImGui::BeginDisabled(disabled);

    ImGui::PushItemWidth(width - GUI_VAR.iconSize.x - GUI_VAR.imguiStyle->ItemInnerSpacing.x);
    const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                      (selectAll_ ? ImGuiInputTextFlags_AutoSelectAll : ImGuiInputTextFlags_None);

    if (!error_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, C_C_BRIGHTRED());
    }
    if (ImGui::InputTextWithHint(id_.c_str(), "Stream spec", &cfg_.spec, flags)) {
        StrTrim(cfg_.spec);
        res = true;
    }
    const bool isEdited = ImGui::IsItemEdited();
    if (!error_.empty()) {
        ImGui::PopStyleColor();
    }
    ImGui::PopItemWidth();
    if (focus_) {
        ImGui::SetKeyboardFocusHere(-1);
        focus_ = false;
    }
    if (!error_.empty()) {
        Gui::ItemTooltip(error_.c_str());
    } else {
        Gui::ItemTooltip("See Tools -> Help for details");
    }
    ImGui::EndDisabled();

    // --- History ---

    ImGui::SameLine(0, GUI_VAR.imguiStyle->ItemInnerSpacing.x);
    ImGui::BeginDisabled(disabled);
    if (ImGui::Button(ICON_FK_STAR "##History", GUI_VAR.iconSize)) {
        ImGui::OpenPopup("History");
        enumeratedPorts_ = EnumeratePorts();
    }
    Gui::ItemTooltip("Recent streams");
    ImGui::EndDisabled();

    if (ImGui::BeginPopup("History")) {
        auto entry = history_.DrawHistory("Recent streams");
        if (entry) {
            Focus();
            SetSpec(entry.value());
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();

    if (isEdited) {
        _Update();
    }

    return res;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetStreamSpec::Focus(const bool selectAll)
{
    focus_ = true;
    selectAll_ = selectAll;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetStreamSpec::Clear()
{
    SetSpec("");
}

void GuiWidgetStreamSpec::SetSpec(const std::string& data)
{
    cfg_.spec = data;
    _Update();
}

// ---------------------------------------------------------------------------------------------------------------------

// serial://<device>[:<baudrate>[:<autobaud>
// telnet(s)://<host>:<port>[:<baudrate>[:<autobaud>[
// static std::regex RE_SERIAL("^(serial://[^:]+)(?::([0-9]+|)|)(?::(none|passive|ubx|fp|auto)|)(.*)$");
static std::regex RE(
    "^(serial://[^:]+|telnets?://(?:\\[[^]+\\]|[^:]+):(?:[0-9]+))(?::([0-9]+|)|)(?::(none|passive|ubx|fp|auto)|)(.*)$");

// serial:///dev/ttyUSB0:921600:auto:foo,bar    [serial:///dev/ttyUSB0] [921600] [auto] [:foo,bar]
// telnet://host:123:921600:auto:foo,bar        [telnet://host:123]     [921600] [auto] [:foo,bar]
// serial:///dev/ttyUSB0:921600:auto:foo        [serial:///dev/ttyUSB0] [921600] [auto] [:foo]
// serial:///dev/ttyUSB0:921600:auto            [serial:///dev/ttyUSB0] [921600] [auto] []
// serial:///dev/ttyUSB0:921600                 [serial:///dev/ttyUSB0] [921600] []     []
// serial:///dev/ttyUSB0:921600,R=5.0           [serial:///dev/ttyUSB0] [921600] []     [,R=5.0]
// serial:///dev/ttyUSB0                        [serial:///dev/ttyUSB0] []       []     []
// 0                                            1                       2        3      4

static void sSpecToBaudrate(
    const StreamType streamType, const std::string& spec, uint32_t& baudrate, AutobaudMode& abMode)
{
    std::smatch m;
    baudrate = 0;
    switch (streamType) {
        case StreamType::SERIAL:
        case StreamType::TELNET:
        case StreamType::TELNETS:
            if (std::regex_match(spec, m, RE) && (m.size() == 5)) {
                TRACE("sSpecToBaudrate(%s) match [%s] -> [%s] [%s] [%s] [%s]", spec.c_str(), m[0].str().c_str(),
                    m[1].str().c_str(), m[2].str().c_str(), m[3].str().c_str(), m[4].str().c_str());
                if (!StrToValue(m[2].str(), baudrate)) {
                    baudrate = StreamOpts::BAUDRATE_DEF;
                }
                abMode = AutobaudMode::AUTO;
                for (auto mode : { AutobaudMode::NONE, AutobaudMode::PASSIVE, AutobaudMode::UBX, AutobaudMode::FP }) {
                    if (m[3].str() == StrToLower(AutobaudModeStr(mode))) {
                        abMode = mode;
                    }
                }
            }
            break;
            break;
        default:
            break;
    }
}

static bool sBaudrateToSpec(
    const StreamType streamType, const uint32_t baudrate, const AutobaudMode abMode, std::string& spec)
{
    std::smatch m;
    switch (streamType) {
        case StreamType::SERIAL:
        case StreamType::TELNET:
        case StreamType::TELNETS:
            if (std::regex_match(spec, m, RE) && (m.size() == 5)) {
                TRACE("sBaudrateToSpec(%" PRIu32 ", %s)  match [%s] -> [%s] [%s] [%s] [%s]", baudrate,
                    AutobaudModeStr(abMode), m[0].str().c_str(), m[1].str().c_str(), m[2].str().c_str(),
                    m[3].str().c_str(), m[4].str().c_str());
                spec = m[1].str() + ":" + std::to_string(baudrate) + ":" + StrToLower(AutobaudModeStr(abMode)) +
                       m[4].str();
                TRACE("sBaudrateToSpec --> %s", spec.c_str());
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetStreamSpec::AddHistory(const std::string& spec)
{
    std::smatch m;
    std::string oldSpec;
    if (std::regex_match(spec, m, RE) && (m.size() == 5)) {
        const std::string match = m[1].str();
        for (auto& cand : history_.GetHistory()) {
            if (StrStartsWith(cand, match)) {
                oldSpec = cand;
                break;
            }
        }
    }
    if (!oldSpec.empty()) {
        TRACE("AddHistory %s -> %s", oldSpec.c_str(), spec.c_str());
        history_.ReplaceHistory(oldSpec, spec);
    } else {
        TRACE("AddHistory -> %s", spec.c_str());
        history_.AddHistory(spec);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetStreamSpec::_Update()
{
    opts_ = StreamOpts::FromSpec(cfg_.spec, error_);
    ok_ = (!cfg_.spec.empty() && opts_);

    if (ok_) {
        sSpecToBaudrate(opts_->type_, cfg_.spec, baudrate_, abMode_);
    } else {
        baudrate_ = 0;
        abMode_ = AutobaudMode::NONE;
        opts_.reset();
    }

    DEBUG("GuiWidgetStreamSpec(%s) ok=%s baudrate=%" PRIu32 " abMode=%s spec=%s", name_.c_str(), ToStr(ok_),
        baudrate_, AutobaudModeStr(abMode_), cfg_.spec.c_str());
}

// ---------------------------------------------------------------------------------------------------------------------

uint32_t GuiWidgetStreamSpec::SetBaudrate(const uint32_t baudrate)
{
    if (!ok_ || !sBaudrateToSpec(opts_->type_, baudrate, abMode_, cfg_.spec)) {
        return 0;
    }
    _Update();
    baudrate_ = baudrate;
    return baudrate_;
}

AutobaudMode GuiWidgetStreamSpec::SetAbMode(const AutobaudMode abMode)
{
    if (!ok_ || !sBaudrateToSpec(opts_->type_, baudrate_, abMode, cfg_.spec)) {
        return AutobaudMode::NONE;
    }
    _Update();
    abMode_ = abMode;
    return abMode_;
}

// ---------------------------------------------------------------------------------------------------------------------

std::optional<std::string> GuiWidgetStreamSpec::_AddHistoryEntries()
{
    std::optional<std::string> res;
    Gui::TextTitle("Detected ports");
    ImGui::Separator();
    if (enumeratedPorts_.empty()) {
        Gui::TextDim("No ports detected");
    }
    for (const auto& entry : enumeratedPorts_) {
        ImGui::PushID(&entry);
        if (ImGui::Selectable(entry.port.c_str())) {
            res = "serial://" + entry.port + ":921600:auto,R=2,H=on";
        }
        if (!entry.desc.empty()) {
            ImGui::SameLine();
            Gui::TextDim(entry.desc.c_str());
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    Gui::TextTitle("Stream examples");
    ImGui::Separator();
    for (auto& spec : STREAM_EXAMPLES) {
        if (ImGui::Selectable(spec)) {
            res = spec;
        }
    }

    return res;
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
