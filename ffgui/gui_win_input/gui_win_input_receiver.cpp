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
#include <fpsdk_common/parser/ubx.hpp>
using namespace fpsdk::common::parser::ubx;
#include <ffxx/utils.hpp>
using namespace ffxx;
//
#include "gui_win_input_receiver.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinInputReceiver::GuiWinInputReceiver(const std::string& name) /* clang-format off */ :
    GuiWinInput(name, { 80, 25, 0, 0 }, ImGuiWindowFlags_None, CreateInput(name, this, InputType_Receiver)),
    streamSpec_         { WinName(), GUI_CFG.receiverSpecHistory, std::bind(&GuiWinInputReceiver::_OnBaudrateChange, this, std::placeholders::_1) },
    recordFileDialog_   { WinName() + ".RecordFileDialog" }  // clang-format on
{
    DEBUG("GuiWinInputReceiver(%s)", WinName().c_str());
    GuiGlobal::LoadObj(WinName() + ".GuiWinInputReceiver", cfg_);

    input_->receiver_ = std::make_shared<Receiver>(WinName(), input_->srcId_);
    input_->database_ = std::make_shared<Database>(WinName());

    _Init();
};

// ---------------------------------------------------------------------------------------------------------------------

GuiWinInputReceiver::~GuiWinInputReceiver()
{
    DEBUG("~GuiWinInputReceiver(%s)", WinName().c_str());
    GuiGlobal::SaveObj(WinName() + ".GuiWinInputReceiver", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::_ProcessData(const DataPtr& data)
{
    if (data->type_ == DataType::MSG) {
        auto& msg = DataPtrToDataMsg(data);
        if (recordLog_) {
            if (!recordLog_->Write(msg.msg_.data_)) {
                log_.AddLine(
                    Sprintf("Failed writing %s: %s", recordLog_->Path().c_str(), recordLog_->Error().c_str()),
                    C_DEBUG_ERROR());
                recordLog_->Close();
                recordLog_.reset();
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

// bool GuiWinInputReceiver::WinIsOpen()
// {
//     const bool isopen = GuiWin::WinIsOpen();
//     const bool active = (receiver_->GetState() != StreamState::CLOSED);
//
//     // User requested window close
//     if (!isopen && active) {
//         receiver_->Abort();
//     }
//
//     // Keep window open as long as receiver is still connected (during disconnect)
//     return isopen || active;
// }

void GuiWinInputReceiver::_Loop(const Time& now)
{
    // Update recording
    if (recordLog_) {
        if ((now - recordLastMsgTime_).GetSec() > 0.5) {
            recordMessage_ = Sprintf("Stop recording\n%s\n%.3f MiB, %.2f KiB/s", recordLog_->Path().c_str(),
                (double)recordLog_->Size() * (1.0 / 1024.0 / 1024.0), recordKiBs_);
            recordLastMsgTime_ = now;
            recordButtonColor_ = (recordButtonColor_ == C_AUTO() ? C_C_BRIGHTRED() : C_AUTO());
        }
        if ((now - recordLastSizeTime_).GetSec() > 2.0) {
            recordKiBs_ = (double)(recordLog_->Size() - recordLastSize_) * (1.0 / (2.0 * 1024.0));
            recordLastSize_ = recordLog_->Size();
            recordLastSizeTime_ = now;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::_ClearData()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::_DrawButtons()
{
    // ImGui::BeginDisabled(disable); // TODO

    if (!recordLog_) {
        if (ImGui::Button(ICON_FK_CIRCLE "###Record", GUI_VAR.iconSize)) {
            if (!recordFileDialog_.IsInit()) {
                auto& io = ImGui::GetIO();
                // recordFilePath_ = "";
                recordFileDialog_.InitDialog(GuiWinFileDialog::FILE_SAVE);
                recordFileDialog_.SetFilename(Strftime("log_%Y%m%d_%H%M") + (io.KeyCtrl ? ".raw.gz" : ".raw"));
                recordFileDialog_.WinSetTitle(WinTitle() + " - Record logfile...");
                recordFileDialog_.SetFileFilter("\\.(ubx|raw|ubx\\.gz)", true);
            } else {
                recordFileDialog_.WinFocus();
            }
        }
        Gui::ItemTooltip("Record logfile\n(CTRL+click for compressed file)");
    } else {
        if (recordButtonColor_ != C_AUTO()) {
            ImGui::PushStyleColor(ImGuiCol_Text, recordButtonColor_);
        }
        if (ImGui::Button(ICON_FK_STOP "###Record", GUI_VAR.iconSize)) {
            log_.AddLine("Recording stopped", C_DEBUG_NOTICE());
            recordLog_->Close();
            recordLog_.reset();
        }
        if (recordButtonColor_ != C_AUTO()) {
            ImGui::PopStyleColor();
        }
        Gui::ItemTooltip(recordMessage_);
    }

    // ImGui::EndDisabled();

    // Open record logfile
    if (recordFileDialog_.IsInit() && recordFileDialog_.DrawDialog()) {
        recordLastSize_ = 0;
        recordLastMsgTime_ = {};
        recordLastSizeTime_ = {};
        recordKiBs_ = 0.0;
        recordLog_ = std::make_unique<OutputFile>();
        if (recordLog_->Open(recordFileDialog_.GetPath())) {
            std::string marker = (versionStr_.empty() ? "unknown receiver" : versionStr_) + ", " +
                                 streamSpec_.GetSpec() + ", " + Strftime("%Y-%m-%d %H:%M:%S") + ", " +
                                 "ffgui " FF_VERSION_STRING;
            std::vector<uint8_t> msg;
            UbxMakeMessage(msg, UBX_INF_CLSID, UBX_INF_TEST_MSGID, StrToBuf(marker).data(), marker.size());
            recordLog_->Write(msg);
        } else {
            log_.AddLine(
                Sprintf("Failed recording to %s: %s", recordLog_->Path().c_str(), recordLog_->Error().c_str()),
                C_DEBUG_ERROR());
            recordLog_.reset();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::_DrawStatus()
{
    _DrawLatestEpochStatus();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::_DrawControls()
{
    // Connect/disconnect/abort button
    auto& receiver = *input_->receiver_;
    const auto streamState = receiver.GetState();
    bool allowBaudrate = true;
    switch (streamState) {
        case StreamState::CLOSED: {
            const bool streamSpecOk = streamSpec_.SpecOk();
            ImGui::BeginDisabled(!streamSpecOk);
            if (ImGui::Button(ICON_FK_PLAY "###StartStop", GUI_VAR.iconSize) || (streamSpecOk && triggerConnect_)) {
                const auto spec = streamSpec_.GetSpec();
                if (receiver.Start(spec)) {
                    streamSpec_.AddHistory(spec);
                }
                triggerConnect_ = false;
            }
            ImGui::EndDisabled();
            Gui::ItemTooltip("Connect receiver");
            break;
        }
        case StreamState::CONNECTING: {
            allowBaudrate = false;
            ImGui::PushStyleColor(ImGuiCol_Text, C_C_GREEN());
            if (ImGui::Button(ICON_FK_EJECT "###StartStop", GUI_VAR.iconSize)) {
                receiver.Stop();
            }
            ImGui::PopStyleColor();
            Gui::ItemTooltip("Disconnect receiver");
            break;
        }
        case StreamState::CONNECTED: {
            ImGui::PushStyleColor(ImGuiCol_Text, C_C_BRIGHTGREEN());
            if (ImGui::Button(ICON_FK_STOP "###StartStop", GUI_VAR.iconSize)) {
                receiver.Stop();
                latestEpoch_.reset();
                versionStr_.clear();
                _UpdateWinTitles();
            }
            ImGui::PopStyleColor();
            Gui::ItemTooltip("Disconnect receiver");
            break;
        }
        case StreamState::ERROR: {
            allowBaudrate = false;
            ImGui::PushStyleColor(ImGuiCol_Text, C_C_BRIGHTRED());
            if (ImGui::Button(ICON_FK_EJECT "###StartStop", GUI_VAR.iconSize)) {
                receiver.Stop();
            }
            ImGui::PopStyleColor();
            Gui::ItemTooltip("Disconnect receiver");
            break;
        }
    }

    ImGui::SameLine();

    // Stream spec input
    if (streamSpec_.DrawWidget(streamState != StreamState::CLOSED, allowBaudrate)) {
        streamSpec_.Focus();
        triggerConnect_ = true;
    }

    // When stream is CLOSED we get the baudrate from the widget and otherwise from the stream
    if (streamState != StreamState::CLOSED) {
        const uint32_t baudrate = receiver.GetBaudrate();
        if (baudrate != streamSpec_.Baudrate()) {
            streamSpec_.SetBaudrate(baudrate);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputReceiver::_OnBaudrateChange(const uint32_t baudrate)
{
    // DEBUG("onbaudrate %u,", baudrate);
    auto& receiver = *input_->receiver_;
    if (receiver.GetState() == StreamState::CONNECTED) {
        receiver.SetBaudrate(baudrate);
        switch (streamSpec_.AbMode()) {  // clang-format off
            case AutobaudMode::UBX:
            case AutobaudMode::AUTO:
                receiver.Write(GetCommonMessage(CommonMessage::UBX_TRAINING));
                break;
            case AutobaudMode::NONE:
            case AutobaudMode::PASSIVE:
            case AutobaudMode::FP:
                break;
        }  // clang-format on
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
