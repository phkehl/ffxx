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
#include <filesystem>
#include <istream>
#include <csignal>
#include <unistd.h>
//
#include <fpsdk_common/path.hpp>
using namespace fpsdk::common::path;
//
#include "gui_win_streammux.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

static constexpr const char* TB_COMMAND = "Command";
static constexpr const char* TB_OUTPUT = "Output";

static HistoryEntries dummyEntries;  // we're not using the filter here

GuiWinStreammux::GuiWinStreammux(const std::string& name) /* clang-format off */ :
    GuiWin(name, { 80, 25, 0, 0 }, ImGuiWindowFlags_None | ImGuiWindowFlags_NoDocking),
    tabbar_      { WinName() },
    log_         { WinName(), dummyEntries, false },
    smHelpWin_   { name + "_Help" }  // clang-format on
{
    DEBUG("GuiWinStreammux(%s)", WinName().c_str());
    GuiGlobal::LoadObj(WinName() + ".GuiWinStreammux", cfg_);
    GuiGlobal::LoadObj("GuiWinStreammux", savedCfg_);  // static!

    auto title = WinName();
    StrReplace(title, "_", " ");
    WinSetTitle(title);

    tabbar_.Switch(TB_COMMAND);
    dirty_ = true;
}

GuiWinStreammux::~GuiWinStreammux()
{
    DEBUG("~GuiWinStreammux(%s)", WinName().c_str());
    _Stop();
    GuiGlobal::SaveObj(WinName() + ".GuiWinStreammux", cfg_);
    GuiGlobal::SaveObj("GuiWinStreammux", savedCfg_);  // static!
}

// ---------------------------------------------------------------------------------------------------------------------

/*static*/ GuiWinStreammux::SavedConfig GuiWinStreammux::savedCfg_{};

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinStreammux::Loop(const Time& now)
{
    if (smProc_) {
        smProc_->ctx_.poll();
        if (!smProc_->child_.running()) {
            const auto exitCode = smProc_->child_.exit_code();
            log_.AddLine("Process exit(" + std::to_string(exitCode) + ")",
                exitCode == 0 ? LoggingLevel::NOTICE : LoggingLevel::FATAL);
            smProc_.reset();
            _UpdateWinTitle();
        }
    }

    smHelpWin_.Loop(now);
}

// ---------------------------------------------------------------------------------------------------------------------

static struct
{
    const char* note;
    const char* args;
} STREAMMUX_EXAMPLES[] = {
    { "Generic template",
R"sh(# Generic template for streammux
# Serial port
--stream serial:///dev/ttyUSB0:115200:auto,A=0.0,R=5.0,H=off,N=serial1
# TCP server
--stream tcpsvr://:10001,N=tcpsvr1
# TCP client
# --stream tcpcli://example.com:12345,C=10.0,A=0.0,R=0.0,N=tcpcli1

# Mux serial<->tcpsvr
--mux serial1=tcpsvr1

# Enable HTTP API and status on http://localhost:10000
--api localhost:10000
)sh"
    }
};

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinStreammux::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }

    const bool running = (smProc_ != nullptr);

    // Toolbar
    {
        // Clear
        switch (dispState_) {
            case DispState::CMD:
                ImGui::BeginDisabled(running);
                if (ImGui::Button(ICON_FK_ERASER "##Clear", GUI_VAR.iconSize)) {
                    cfg_.sm = {};
                }
                ImGui::EndDisabled();
                Gui::ItemTooltip("Clear config");
                break;
            case DispState::OUT:
                if (ImGui::Button(ICON_FK_ERASER "##Clear", GUI_VAR.iconSize)) {
                    log_.Clear();
                }
                Gui::ItemTooltip("Clear output");
                break;
        }

        Gui::VerticalSeparator();

        if (smProc_) {
            ImGui::PushStyleColor(ImGuiCol_Text, C_C_BRIGHTGREEN());
            if (ImGui::Button(ICON_FK_STOP "##Execute", GUI_VAR.iconSize)) {
                _Stop();
            }
            ImGui::PopStyleColor();
            Gui::ItemTooltip("Stop streammux");
        } else {
            ImGui::BeginDisabled(!cfgExeOk_ || !cfgArgsOk_);
            if (ImGui::Button(ICON_FK_PLAY "##Execute", GUI_VAR.iconSize)) {
                _Start();
            }
            ImGui::EndDisabled();
            Gui::ItemTooltip("Run streammux");
        }

        Gui::VerticalSeparator();

        // Note
        ImGui::SetNextItemWidth(-(2 * GUI_VAR.iconSize.x) - GUI_VAR.imguiStyle->ItemInnerSpacing.x - GUI_VAR.imguiStyle->ItemSpacing.x);
        if (ImGui::InputTextWithHint("##Note", "Note", &cfg_.sm.note)) {
            dirty_ = true;
        }

        auto savedCfg = std::find_if(savedCfg_.sms.begin(), savedCfg_.sms.end(),
            [note = cfg_.sm.note](const auto& cand) { return note == cand.note; });
        const bool canSave = !cfg_.sm.note.empty() && ((savedCfg == savedCfg_.sms.end()) || (cfg_.sm != *savedCfg));
        WinToggleFlag(ImGuiWindowFlags_UnsavedDocument, canSave || cfg_.sm.note.empty());

        // Save
        ImGui::SameLine(0, GUI_VAR.imguiStyle->ItemInnerSpacing.x);
        ImGui::BeginDisabled(!canSave);
        if (ImGui::Button(ICON_FK_FLOPPY_O, GUI_VAR.iconSize)) {
            if (savedCfg != savedCfg_.sms.end()) {
                *savedCfg = cfg_.sm;
            } else {
                savedCfg_.sms.push_back(cfg_.sm);
                std::sort(savedCfg_.sms.begin(), savedCfg_.sms.end());
            }
        }
        Gui::ItemTooltip("Save config");
        ImGui::EndDisabled();

        // Load
        ImGui::SameLine();
        if (ImGui::Button(ICON_FK_FOLDER_O, GUI_VAR.iconSize)) {
            ImGui::OpenPopup("Load");
        }
        Gui::ItemTooltip("Load config");
        if (ImGui::BeginPopup("Load")) {
            ImGui::BeginDisabled(running);
            const ImVec2 dummySize(3 * GUI_VAR.charSizeX, GUI_VAR.lineHeight);
            const auto a0 = ImGui::GetContentRegionAvail();
            const float trashOffs = a0.x - (2 * GUI_VAR.charSizeX);
            std::size_t id = 1;

            if (savedCfg_.sms.empty()) {
                Gui::TextDim("No saved configs");
            }
            for (auto it = savedCfg_.sms.begin(); it != savedCfg_.sms.end();) {
                ImGui::PushID(id++);
                ImGui::SetNextItemAllowOverlap();
                if (ImGui::Selectable(Sprintf("%.30s%s", it->note.c_str(), it->note.size() > 30 ? "..." : "").c_str())) {
                    cfg_.sm = *it;
                    dirty_ = true;
                    tabbar_.Switch(TB_COMMAND);
                }
                ImGui::SameLine();
                ImGui::Dummy(dummySize);
                ImGui::SameLine(trashOffs);
                bool remove = ImGui::SmallButton(ICON_FK_TRASH);
                Gui::ItemTooltip("Remove config");
                ImGui::PopID();
                if (remove) {
                    it = savedCfg_.sms.erase(it);
                } else {
                    it++;
                }
            }
            ImGui::Separator();
            Gui::TextTitle("Examples:");
            for (auto& ex : STREAMMUX_EXAMPLES) {
                if (ImGui::Selectable(ex.note)) {
                    cfg_.sm.note.clear();
                    cfg_.sm.args = ex.args;
                    dirty_ = true;
                    tabbar_.Switch(TB_COMMAND);
                }
            }

            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
    }


    ImGui::Separator();

    if (tabbar_.Begin()) {
        if (tabbar_.Item(TB_COMMAND)) { dispState_ = DispState::CMD; }
        if (tabbar_.Item(TB_OUTPUT))  { dispState_ = DispState::OUT; }
        tabbar_.End();
    }

    switch (dispState_) {
        // Edit command
        case DispState::CMD:
            ImGui::BeginDisabled(running);
            // Path to streammux executable
            if (ImGui::Button(ICON_FK_SEARCH, GUI_VAR.iconSize)) {
                const auto self = std::filesystem::canonical("/proc/self/exe").parent_path();
                for (const auto& cand : { self / ".." / "ffapps" / "streammux", self / ".." / "streammux", std::filesystem::path("/usr/bin/streammux") }) {
                    DEBUG("cand: %s", cand.c_str());
                    std::error_code ec;
                    const auto exe = std::filesystem::canonical(cand, ec);
                    if (!ec) {
                        DEBUG("found: %s", exe.c_str());
                        cfg_.sm.exe = exe;
                        dirty_ = true;
                        break;
                    }
                }
            }
            Gui::ItemTooltip("Find streammux");
            ImGui::SameLine(0, GUI_VAR.imguiStyle->ItemInnerSpacing.x);
            ImGui::SetNextItemWidth(-GUI_VAR.iconSize.x - GUI_VAR.imguiStyle->ItemSpacing.x);
            if (!cfgExeOk_) { ImGui::PushStyleColor(ImGuiCol_Border, C_C_BRIGHTRED()); }
            if (ImGui::InputTextWithHint("##Exe", "/path/to/streammux", &cfg_.sm.exe)) { dirty_ = true; }
            if (!cfgExeOk_) { ImGui::PopStyleColor(); }
            ImGui::SameLine();
            ImGui::BeginDisabled(!cfgExeOk_);
            if (ImGui::Button(ICON_FK_QUESTION_CIRCLE, GUI_VAR.iconSize)) {
                smHelpWin_.Update(cfg_.sm.exe);
            }
            ImGui::EndDisabled();
            Gui::ItemTooltip("Get streammux help");
            if (!cfgArgsOk_) { ImGui::PushStyleColor(ImGuiCol_Border, C_C_BRIGHTRED()); }
            if (ImGui::InputTextMultiline("##Args", &cfg_.sm.args, { -FLT_MIN, -FLT_MIN }, ImGuiInputTextFlags_AllowTabInput)) {
                dirty_ = true;
            }
            if (!cfgArgsOk_) { ImGui::PopStyleColor(); }
            ImGui::EndDisabled();
            break;

        // Show output
        case DispState::OUT:
            log_.DrawWidget();
            break;
    }

    if (dirty_) {
        cfgExeOk_ = (PathIsFile(cfg_.sm.exe) && PathIsExecutable(cfg_.sm.exe));
        cfgArgsOk_ = (_MakeArgv(cfg_.sm).size() > 1);
        // DEBUG("cfgExeOk=%s cfgArgsOk=%s", ToStr(cfgExeOk_), ToStr(cfgArgsOk_));
        dirty_ = false;
    }

    _DrawWindowEnd();

    if (smHelpWin_.WinIsOpen()) {
        smHelpWin_.DrawWindow();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

std::vector<std::string> GuiWinStreammux::_MakeArgv(const SmCfg& cfg) const
{
    std::vector<std::string> argv;
    argv.push_back(cfg.exe);
    for (auto& line : StrSplit(cfg.args, "\n")) {
        if (line.empty()) {
            continue;
        }
        for (auto& arg : StrSplit(StrSplit(line, "#", 2)[0], " ")) {
            argv.push_back(arg);
        }
    }
    return argv;
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWinStreammux::SmProc::SmProc(const std::vector<std::string>& argv) /* clang-format off */ :
    stdout_pipe_   { ctx_ },
    stderr_pipe_   { ctx_ },
    child_         { argv,
                     boost::process::std_in.close(),
                     boost::process::std_out = stdout_pipe_,
                     boost::process::std_err = stderr_pipe_,
                     boost::process::env["FPSDK_LOGGING"] = "journal,none",
                     boost::process::on_exit(std::bind(&GuiWinStreammux::SmProc::OnExit, this, std::placeholders::_1, std::placeholders::_2)),
                     ctx_ }  // clang-format on
{
}
void GuiWinStreammux::SmProc::OnExit(const int status, const std::error_code& ec)
{
    DEBUG("OnExit: %d %s", status, ec.message().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinStreammux::_Start()
{
    log_.AddLine("Starting streammux...", LoggingLevel::NOTICE);
    const auto argv = _MakeArgv(cfg_.sm);
    DEBUG("%s", StrJoin(argv, " ").c_str());
    try {
        smProc_ = std::make_unique<SmProc>(argv);
    } catch (std::system_error& ex) {
        log_.AddLine(Sprintf("Failed creating process: %s", ex.what()), LoggingLevel::ERROR);
        return;
    }

    _DoRead(smProc_->stdout_pipe_, smProc_->stdout_buf_);
    _DoRead(smProc_->stderr_pipe_, smProc_->stderr_buf_);
    smProc_->ctx_.restart();

    log_.AddLine(Sprintf("Process started, PID %d", smProc_->child_.id()), LoggingLevel::NOTICE);
    _UpdateWinTitle();

    tabbar_.Switch(TB_OUTPUT);
}

void GuiWinStreammux::_DoRead(boost::process::async_pipe& pipe, boost::asio::streambuf& buf)
{
    boost::asio::async_read_until(pipe, buf, '\n',
        std::bind(&GuiWinStreammux::_OnRead, this, std::placeholders::_1, std::placeholders::_2, std::ref(pipe),
            std::ref(buf)));
}

void GuiWinStreammux::_OnRead(const boost::system::error_code& ec, std::size_t size, boost::process::async_pipe& pipe,
    boost::asio::streambuf& buf)
{
    // DEBUG("_OnRead %s %" PRIuMAX, ec.message().c_str(), size);
    if ((ec == boost::asio::error::operation_aborted) || (ec == boost::asio::error::eof)) {
    } else if (ec) {
        ERROR("%s", ec.message().c_str());
    } else {
        const auto data = boost::asio::buffer_cast<const char*>(buf.data());
        std::string str(data, data + size - 1);
        buf.consume(size);
        _DoRead(pipe, buf);

        if ((str.size() > 3) && (str[0] == '<') && (str[2] == '>')) {
            switch (str[1]) {  // clang-format off
                case '0' + EnumToVal(LoggingLevel::FATAL):   log_.AddLine(str.substr(3), LoggingLevel::FATAL);   break;
                case '0' + EnumToVal(LoggingLevel::ERROR):   log_.AddLine(str.substr(3), LoggingLevel::ERROR);   break;
                case '0' + EnumToVal(LoggingLevel::WARNING): log_.AddLine(str.substr(3), LoggingLevel::WARNING); break;
                case '0' + EnumToVal(LoggingLevel::NOTICE):  log_.AddLine(str.substr(3), LoggingLevel::NOTICE);  break;
                case '0' + EnumToVal(LoggingLevel::INFO):    log_.AddLine(str.substr(3), LoggingLevel::INFO);    break;
                case '0' + EnumToVal(LoggingLevel::DEBUG):   log_.AddLine(str.substr(3), LoggingLevel::DEBUG);   break;
                case '0' + EnumToVal(LoggingLevel::TRACE):   log_.AddLine(str.substr(3), LoggingLevel::TRACE);   break;
                default:                                     log_.AddLine(str);                                  break;
            }  // clang-format on
        } else if (str.empty()) {
            log_.AddLine(" ");
        } else {
            log_.AddLine(str);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinStreammux::_Stop()
{
    if (smProc_) {
        if (!smProc_->sigintSent_) {
            kill(smProc_->child_.id(), SIGINT);
            smProc_->sigintSent_ = true;
        } else {
            smProc_->child_.terminate();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinStreammux::_UpdateWinTitle()
{
    const auto mainWinName = WinName();  // "StreamMux_1"
    auto mainWinTitle = mainWinName;
    StrReplace(mainWinTitle, "_", " ");
    if (smProc_) {
        WinSetTitle(mainWinTitle + " [" + std::to_string(smProc_->child_.id()) + "]");
    } else {
        WinSetTitle(mainWinTitle);
    }
}

/* ****************************************************************************************************************** */

GuiWinStreammuxHelp::GuiWinStreammuxHelp(const std::string& name) /* clang-format off */ :
    GuiWin(name, { 120, 25, 0, 0 }, ImGuiWindowFlags_None) // clang-format on
{
    DEBUG("GuiWinStreammuxHelp(%s)", WinName().c_str());

    auto title = WinName();
    StrReplace(title, "_", " ");
    WinSetTitle(title);

    WinClose();
}

GuiWinStreammuxHelp::~GuiWinStreammuxHelp()
{
    DEBUG("~GuiWinStreammuxHelp(%s)", WinName().c_str());
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinStreammuxHelp::Loop(const Time& /*now*/)
{
    if (ready_ && fut_.valid()) {
        help_ = fut_.get();
        ready_ = false;
    }
}

void GuiWinStreammuxHelp::DrawWindow() {
    if (!_DrawWindowBegin()) {
        return;
    }

    for (const auto& line : help_) {
        ImGui::TextUnformatted(line.c_str());
    }

    _DrawWindowEnd();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinStreammuxHelp::Update(const std::string& streammuxExe)
{
    WinOpen();
    if (fut_.valid()) {
        return;
    }

    help_.clear();
    help_.push_back("Loading...");
    help_.push_back(streammuxExe);
    fut_ = std::async(std::launch::async, &GuiWinStreammuxHelp::_GetHelp, this, streammuxExe);
}

// ---------------------------------------------------------------------------------------------------------------------

std::vector<std::string>GuiWinStreammuxHelp::_GetHelp(const std::string& streammuxExe)
{
    boost::process::ipstream is;
    boost::process::child c(streammuxExe, "-h", boost::process::std_out > is);

    std::vector<std::string> data;
    std::string line;

    while (c.running() && std::getline(is, line)) {
        data.push_back(line);
    }
    c.wait();

    ready_ = true;
    return data;
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
