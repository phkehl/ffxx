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
#include "gui_win_input_logfile.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::types;

GuiWinInputLogfile::GuiWinInputLogfile(const std::string &name) /* clang-format off */ :
    GuiWinInput(name, { 80, 25, 0, 0 }, ImGuiWindowFlags_None, CreateInput(name, this, InputType_Logfile)),
    fileDialog_   { WinName() + ".RecordFileDialog" },
    history_      { WinName(), GUI_CFG.logfileHistory, 30 }  // clang-format on
{
    DEBUG("GuiWinInputLogfile(%s)", WinName().c_str());
    GuiGlobal::LoadObj(WinName() + ".GuiWinInputLogfile", cfg_);

    input_->logfile_ = std::make_shared<Logfile>(WinName(), input_->srcId_);
    input_->database_ = std::make_shared<Database>(WinName());

    _Init();

    input_->logfile_->SetSpeed(cfg_.limitSpeed ? cfg_.speed : Logfile::SPEED_INF);
};

// ---------------------------------------------------------------------------------------------------------------------

GuiWinInputLogfile::~GuiWinInputLogfile()
{
    DEBUG("~GuiWinInputLogfile(%s)", WinName().c_str());
    GuiGlobal::SaveObj(WinName() + ".GuiWinInputLogfile", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputLogfile::_ProcessData(const DataPtr& /*data*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputLogfile::_Loop(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputLogfile::_ClearData()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputLogfile::_DrawButtons()
{
    ImGui::NewLine();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputLogfile::_DrawStatus()
{
    _DrawLatestEpochStatus();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputLogfile::_DrawControls()
{
    auto& logfile = *input_->logfile_;
    const bool canOpen  = logfile.CanOpen();
    const bool canClose = logfile.CanClose();
    const bool canPlay  = logfile.CanPlay();
    const bool canStop  = logfile.CanStop();
    const bool canPause = logfile.CanPause();
    const bool canStep  = logfile.CanStep();
    const bool canSeek  = logfile.CanSeek();

    // ImGui::Text("%s canOpen=%s canClose=%s canPlay=%s canStop=%s canPause=%s canStep=%s canSeek=%s", logfile.StateStr(),
    //     ToStr(canOpen), ToStr(canClose), ToStr(canPlay), ToStr(canStop), ToStr(canPause), ToStr(canStep), ToStr(canSeek));

    // Open log
    {
        ImGui::BeginDisabled(!canOpen);
        if (ImGui::Button(ICON_FK_FOLDER_OPEN "##Open", GUI_VAR.iconSize)) {
            if (!fileDialog_.IsInit()) {
                fileDialog_.InitDialog(GuiWinFileDialog::FILE_OPEN);
                fileDialog_.WinSetTitle(WinTitle() + " - Open logfile...");
                fileDialog_.SetFileFilter("\\.(ubx|raw|ubx\\.gz)", true);
            } else {
                fileDialog_.WinFocus();
            }
        }
        Gui::ItemTooltip("Open logfile");

        ImGui::SameLine(0, GUI_VAR.imguiStyle->ItemInnerSpacing.x);

        if (ImGui::Button(ICON_FK_STAR "##History", GUI_VAR.iconSize)) {
            ImGui::OpenPopup("History");
        }
        Gui::ItemTooltip("Recent logfiles");
        if (ImGui::BeginPopup("History")) {
            const auto entry = history_.DrawHistory("Recent logfiles");
            if (entry) {
                DEBUG("selected log %s", entry.value().c_str());
            }
            if (entry && logfile.Open(entry.value())) {
                history_.AddHistory(entry.value());
                progressBarFmt_ = std::filesystem::path(entry.value()).filename().string() + " (%.3f%%)";
            }
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();

        // Handle file dialog, and log open
        if (fileDialog_.IsInit()) {
            if (fileDialog_.DrawDialog()) {
                const auto path = fileDialog_.GetPath();
                seekProgress_ = -1.0f;
                if (!path.empty() && logfile.Open(path)) {
                    history_.AddHistory(path);
                    progressBarFmt_ = std::filesystem::path(path).filename().string() + " (%.3f%%)";
                }
            }
        }
    }

    Gui::VerticalSeparator();

    // Close log
    {
        ImGui::BeginDisabled(!canClose);
        if (ImGui::Button(ICON_FK_EJECT "##Close", GUI_VAR.iconSize)) {
            logfile.Close();
        }
        ImGui::EndDisabled();
        Gui::ItemTooltip("Close logfile");
    }

    ImGui::SameLine();

    // Play / pause
    {
        ImGui::BeginDisabled(!canPlay && !canPause);
        if (canPause) {
            if (ImGui::Button(ICON_FK_PAUSE "##PlayPause", GUI_VAR.iconSize)) {
                logfile.Pause();
            }
        } else {
            if (ImGui::Button(ICON_FK_PLAY "##PlayPause", GUI_VAR.iconSize)) {
                logfile.Play();
            }
        }
        Gui::ItemTooltip(canPause ? "Pause" : "Play");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    // Stop
    {
        ImGui::BeginDisabled(!canStop);
        if (ImGui::Button(ICON_FK_STOP "##Stop", GUI_VAR.iconSize)) {
            logfile.Stop();
            _ClearData();
        }
        Gui::ItemTooltip("Stop (and rewind)");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    // Step epoch
    {
        ImGui::BeginDisabled(!canStep);
        ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
        if (ImGui::Button(ICON_FK_FORWARD "##StepEpoch", GUI_VAR.iconSize)) {
            logfile.StepEpoch();
        }
        ImGui::PopItemFlag();
        Gui::ItemTooltip("Step epoch");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    // Step message
    {
        ImGui::BeginDisabled(!canStep);
        ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
        if (ImGui::Button(ICON_FK_STEP_FORWARD "##StepMsg", GUI_VAR.iconSize)) {
            logfile.StepMsg();
        }
        ImGui::PopItemFlag();
        Gui::ItemTooltip("Step message");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    // Play speed
    {
        ImGui::BeginDisabled(true); // doesn't work well...
        if (ImGui::Checkbox("##SpeedLimit", &cfg_.limitSpeed)) {
            logfile.SetSpeed(cfg_.limitSpeed ? cfg_.speed : Logfile::SPEED_INF);
        }
        Gui::ItemTooltip("Limit play speed");
        ImGui::EndDisabled();

        ImGui::SameLine(0, 0);

        ImGui::BeginDisabled(!cfg_.limitSpeed);
        ImGui::SetNextItemWidth(GUI_VAR.charSizeX * 6);
        if (ImGui::DragFloat("##Speed", &cfg_.speed, 0.1f, Logfile::SPEED_MIN, Logfile::SPEED_MAX,
                cfg_.limitSpeed ? "%.1f" : "inf", ImGuiSliderFlags_AlwaysClamp))
        logfile.SetSpeed(cfg_.speed);
        std::array<const char*, 8> presets = { { "1.0", "2.0", "5.0", "10.0", "20.0", "50.0", "100.0", "500.0" } };
        ImGui::EndDisabled();
        Gui::ItemTooltip("Play speed [epochs/s]");

        ImGui::SameLine(0, 0);

        if (ImGui::BeginCombo("##SpeedChoice", NULL, ImGuiComboFlags_HeightLarge | ImGuiComboFlags_NoPreview)) {
            for (std::size_t ix = 0; ix < presets.size(); ix++) {
                if (ImGui::Selectable(presets[ix])) {
                    StrToValue(presets[ix], cfg_.speed);
                    logfile.SetSpeed(cfg_.speed);
                    cfg_.limitSpeed = true;
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(logfile.StateStr());

    // Progress indicator
    if (canClose) // i.e. log must be currently open
    {
        float progress = (seekProgress_ > -1.0f ? seekProgress_ : (logfile.GetRelPos() * 1e2f));
        ImGui::SetNextItemWidth(-1);
        ImGui::BeginDisabled(!canSeek);
        if (ImGui::SliderFloat("##PlayProgress", &progress, 0.0, 100.0, progressBarFmt_.c_str(), ImGuiSliderFlags_AlwaysClamp))
        {
            seekProgress_ = progress; // Store away because...
        }
        if (!ImGui::IsItemActive()) { Gui::ItemTooltip("Play position (progress) [%filesize]"); }
        if (ImGui::IsItemDeactivated() && // ...this fires one frame later
            ( std::abs( logfile.GetRelPos() - seekProgress_ ) > 1e-5 ) )
        {
            logfile.SetRelPos(seekProgress_ * 1e-2f);
            seekProgress_ = -1.0f;
        }
        ImGui::EndDisabled();
    }
    else
    {
        float progress = 0;
        ImGui::SetNextItemWidth(-1);
        ImGui::BeginDisabled(true);
        ImGui::SliderFloat("##PlayProgress", &progress, 0.0, 100.0, "");
        ImGui::EndDisabled();
        //ImGui::InvisibleButton("##PlayProgress", GUI_VAR.iconSize);
    }
}

/* ****************************************************************************************************************** */
} // ffgui
