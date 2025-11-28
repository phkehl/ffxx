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
#include <fpsdk_common/parser.hpp>
using namespace fpsdk::common::parser;
#include <fpsdk_common/parser/ubx.hpp>
using namespace fpsdk::common::parser::ubx;
#include <ffxx/utils.hpp>
#include <ffxx/ublox.hpp>
using namespace ffxx;
//
#include "gui_win_data_messages.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinDataMessages::GuiWinDataMessages(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 130, 25, 0, 0 }, ImGuiWindowFlags_None, input),
    selectedEntry_  { messages_.end() },
    displayedEntry_ { messages_.end() }  // clang-format on
{
    DEBUG("~GuiWinDataMessages(%s)", WinName().c_str());
    latestEpochMaxAge_ = {};
    GuiGlobal::LoadObj(WinName() + ".GuiWinDataMessages", cfg_);
    _InitMsgConfs();

    ClearData();
}

GuiWinDataMessages::~GuiWinDataMessages()
{
    DEBUG("~GuiWinDataMessages(%s)", WinName().c_str());
    GuiGlobal::SaveObj(WinName() + ".GuiWinDataMessages", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataMessages::_ProcessData(const DataPtr& data)
{
    switch (data->type_) {
        case DataType::MSG: {
            const auto& msg = DataPtrToDataMsgPtr(data);
            auto entry = messages_.find(msg->msg_.name_);
            if (entry == messages_.end()) { // clang-format off
                entry = messages_.emplace(msg->msg_.name_,
                    MsgInfo(msg->msg_.name_, WinName(), GuiMsg::GetInstance(msg->msg_.name_, WinName(), input_))).first; // clang-format on
            }
            entry->second.Update(msg);
            break;
        }
        case DataType::EPOCH:
        case DataType::DEBUG:
        case DataType::EVENT:
            break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataMessages::_Loop(const Time& now)
{
    for (auto entry = messages_.begin(); entry != messages_.end(); entry++) {
        entry->second.Update(now);

        // Select message from saved config, once it appears
        if (!cfg_.selectedMessage.empty() && (selectedEntry_ == messages_.end()) &&
            (cfg_.selectedMessage == entry->first)) {
            selectedEntry_ = entry;
            displayedEntry_ = selectedEntry_;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataMessages::_ClearData()
{
    messages_.clear();
    selectedEntry_ = messages_.end();
    displayedEntry_ = messages_.end();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataMessages::_DrawToolbar()
{
    listWidth_ = 60.0f * GUI_VAR.charSizeX;

    ImGui::SameLine();

    const bool ctrlEnabled = (input_->receiver_ && (input_->receiver_->GetState() == StreamState::CONNECTED));

    // Message enable/disable
    ImGui::BeginDisabled(!ctrlEnabled);
    if (ImGui::Button(ICON_FK_WRENCH "##MsgConf", GUI_VAR.iconSize)) {
        ImGui::OpenPopup("MsgConf");
    }
    Gui::ItemTooltip("Enable/disable output messages");
    ImGui::EndDisabled();
    if (ImGui::IsPopupOpen("MsgConf")) {
        if (ImGui::BeginPopup("MsgConf")) {
            _DrawMsgConfMenu();
            ImGui::EndPopup();
        }
    }

    Gui::VerticalSeparator();

    Gui::ToggleButton(ICON_FK_LIST "###ShowList", NULL, &cfg_.showList, "Showing message list",
        "Not showing message list", GUI_VAR.iconSize);

    Gui::VerticalSeparator(cfg_.showList ? listWidth_ + (2 * GUI_VAR.imguiStyle->ItemSpacing.x) : 0);
    const bool haveDispEntry = (displayedEntry_ != messages_.end());

    Gui::ToggleButton(ICON_FK_FILE_CODE_O "###ShowHexdump", NULL, &cfg_.showHexDump, "Showing hexdump",
        "Not showing hexdump", GUI_VAR.iconSize);

    ImGui::SameLine();

    // Clear
    ImGui::BeginDisabled(!haveDispEntry);
    if (ImGui::Button(ICON_FK_ERASER "##ClearMsg", GUI_VAR.iconSize)) {
        displayedEntry_->second.Reset();
    }
    ImGui::EndDisabled();
    Gui::ItemTooltip("Clear message data");

    ImGui::SameLine();

    // Poll
    ImGui::BeginDisabled(!haveDispEntry || !ctrlEnabled || (msgConfs_.find(displayedEntry_->first) == msgConfs_.end()));
    if (ImGui::Button(ICON_FK_REFRESH "##PollMsg", GUI_VAR.iconSize)) {
        auto e = msgConfs_.find(displayedEntry_->first);
        if ((e != msgConfs_.end()) && e->second.cmdPoll_) {
            input_->receiver_->Write(*e->second.cmdPoll_);
        }
    }
    ImGui::EndDisabled();
    Gui::ItemTooltip("Poll message");

    ImGui::SameLine();

    // Step
    ImGui::BeginDisabled(!haveDispEntry || !input_->logfile_ || !input_->logfile_->CanStep() );
    if (ImGui::Button(ICON_FK_STEP_FORWARD "##StepMsg", GUI_VAR.iconSize)) {
        input_->logfile_->StepMsg(displayedEntry_->first);
    }
    ImGui::EndDisabled();
    Gui::ItemTooltip("Step message");

    // Optional message view buttons
    if (haveDispEntry && displayedEntry_->second.guiMsg_) {
        ImGui::PushID(std::addressof(displayedEntry_->second.guiMsg_));
        displayedEntry_->second.guiMsg_->DrawToolbar();
        ImGui::PopID();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataMessages::_DrawContent()
{
    const bool showList = (cfg_.showList || (displayedEntry_ == messages_.end()));

    // ImGui::Separator();

    // Messsages list (left side of window)
    if (showList) {
        if (ImGui::BeginChild("##List", ImVec2(listWidth_, 0.0f)/*, ImGuiChildFlags_ResizeX*/)) { // TODO
            _DrawList();
        }
        ImGui::EndChild();

        Gui::VerticalSeparator();
    }

    // Message (right side of window)
    if (displayedEntry_ != messages_.end()) {
        if (ImGui::BeginChild("##Message", ImVec2(0.0f, 0.0f))) {
            _DrawMessage();
        }
        ImGui::EndChild();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataMessages::_DrawList()
{
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoSavedSettings |
                                  ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("List", 6, flags)) {
        ImGui::TableSetupColumn("Message");
        ImGui::TableSetupColumn("Count");
        ImGui::TableSetupColumn("Bytes");
        ImGui::TableSetupColumn("M/s");
        ImGui::TableSetupColumn("B/s");
        ImGui::TableSetupColumn("Age");
        ImGui::TableHeadersRow();

        std::size_t prevGroupId = 0;
        bool drawNodes = false;
        const bool showTimeAndRates = (input_->receiver_ && (input_->receiver_->GetState() == StreamState::CONNECTED));
        for (auto entry = messages_.begin(); entry != messages_.end(); entry++) {
            //auto& name = entry->first;
            auto& info = entry->second;

            if (prevGroupId != info.groupId_) {
                if (drawNodes && (prevGroupId != 0)) {
                    ImGui::TreePop();
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                //drawNodes = ImGui::TreeNode(infoa.group_.c_str());
                drawNodes = ImGui::TreeNodeEx(info.group_.c_str(),
                ImGuiTreeNodeFlags_SpanLabelWidth /*ImGuiTreeNodeFlags_LabelSpanAllColumns|ImGuiTreeNodeFlags_SpanAllColumns*/);

                prevGroupId = info.groupId_;
            }

            if (!drawNodes) {
                continue;
            }

            ImGui::TableNextRow();

            // if (info.recent_) {
            //     ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_HIGHLIGHT_FG());
            // }

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(info.recent_ ? "*" : " ");
            ImGui::SameLine();
            ImGui::TextUnformatted(info.name_.c_str());

            ImGui::TableNextColumn();

            // Click on message
            const bool selected = (selectedEntry_ == entry);
            if (ImGui::Selectable(info.imguiId_.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                // Unselect currently selected message
                if (selected) {
                    selectedEntry_ = messages_.end();
                    cfg_.selectedMessage.clear();
                }
                // Select new message
                else {
                    selectedEntry_ = entry;
                    displayedEntry_ = selectedEntry_;
                    cfg_.selectedMessage = selectedEntry_->first;
                }
            }
            // Hover message
            if ((selectedEntry_ == messages_.end()) && (ImGui::IsItemActive() || ImGui::IsItemHovered())) {
                displayedEntry_ = entry;
            }

            ImGui::SameLine(0,0);
            ImGui::TextUnformatted(info.countStr_.c_str());

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(info.bytesStr_.c_str());

            if (showTimeAndRates) {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(info.msgRateStr_.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(info.byteRateStr_.c_str());
            } else {
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
            }

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(info.ageStr_.c_str());

            // if (info.recent_) {
            //     ImGui::PopStyleColor();
            // }
        }
        if (drawNodes && (prevGroupId != 0)) {
            ImGui::TreePop();
        }
        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataMessages::_DrawMessage()
{
    if (!displayedEntry_->second.msg_) {
        return;
    }

    auto& info = displayedEntry_->second;
    const auto& msg = DataMsgPtrToDataMsg(displayedEntry_->second.msg_);

    // clang-format off
    const float sepPadding     = (2 * GUI_VAR.imguiStyle->ItemSpacing.y) + 1;
    const ImVec2 sizeAvail     = ImGui::GetContentRegionAvail();
    const ImVec2 msgInfoSize   { 0, (2 * GUI_VAR.charSizeY) + (1 * GUI_VAR.imguiStyle->ItemSpacing.y) };
    const ImVec2 hexDumpSize   { 0, cfg_.showHexDump ? std::clamp<std::size_t>(info.hexdump_.Lines(), 5, 10) * GUI_VAR.charSizeY : 0 };
    const float remHeight      = sizeAvail.y - msgInfoSize.y - hexDumpSize.y - sepPadding - (cfg_.showHexDump ? sepPadding : 0);
    const float minHeight      = 10 * GUI_VAR.charSizeY;
    const ImVec2 msgDetailSize { 0, std::max(remHeight, minHeight) };
    // clang-format on

    const bool receiverReady = (input_->receiver_ && (input_->receiver_->GetState() == StreamState::CONNECTED));

    // Message info
    if (ImGui::BeginChild("##MsgInfo", msgInfoSize)) {
        //            12345678901234567890 12345678 12345678 12345 12345 12345 12345 12345
        Gui::TextDim("Name:                  Proto:   Origin:   Seq:  Cnt:  Age: Rate: Size:");
        if (receiverReady) {
            ImGui::Text("%-20s %c %-8s %-8s %5" PRIuMAX " %5" PRIuMAX " %5.1f %5.1f %5" PRIuMAX "",
                msg.msg_.name_.c_str(), info.recent_ ? '*' : ' ', ProtocolStr(msg.msg_.proto_),
                DataMsg::OriginStr(msg.origin_), msg.msg_.seq_, info.count_, info.age_ != 0.0f ? info.age_ : NAN,
                info.msgRate_ != 0.0f ? info.msgRate_ : NAN, msg.msg_.Size());
        } else {
            ImGui::Text("%-20s %c %-8s %-8s %5" PRIuMAX " %5" PRIuMAX "     -     - %5" PRIuMAX "",
                msg.msg_.name_.c_str(), info.recent_ ? '*' : ' ', ProtocolStr(msg.msg_.proto_),
                DataMsg::OriginStr(msg.origin_), msg.msg_.seq_, info.count_, msg.msg_.Size());
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Message details
    if (ImGui::BeginChild(
            "##MsgDetails", msgDetailSize, false, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (!msg.msg_.info_.empty()) {
            Gui::TextDim("Info:");
            ImGui::SameLine();
            ImGui::TextWrapped("%s", msg.msg_.info_.c_str());
            if (info.guiMsg_) {
                ImGui::Separator();
            }
        }

        if (info.guiMsg_) {
            if (ImGui::BeginChild("##GuiMsg")) {
                info.guiMsg_->DrawMsg();
            }
            ImGui::EndChild();
        }
    }
    ImGui::EndChild();

    // Hexdump
    if (cfg_.showHexDump) {
        ImGui::Separator();
        info.hexdump_.DrawWidget();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataMessages::_DrawMsgConfMenu()
{
    Gui::BeginMenuPersist();

    std::size_t prevGroupId = 0;
    bool groupMenuOpen = false;

    for (auto& [ msgName, msgConf ] : msgConfs_) {

        if (msgConf.groupId_ != prevGroupId) {
            if (groupMenuOpen) {
                ImGui::EndMenu();
            }
            prevGroupId = msgConf.groupId_;
            groupMenuOpen = ImGui::BeginMenu(msgConf.group_.c_str());
        }
        if (groupMenuOpen) {
            // Check if message is enabled  FIXME: this could be nicer...
            bool enabled = false;
            // NMEA 0183 messages are named NMEA-<talker>-<formatter>, but in the configuration
            // library we use NMEA-*S*TANDAR*D*-<formatter>. Look for formatter
            MsgInfoMap::iterator info = messages_.end();
            if ((msgConf.msgName_[5] == 'S') && (msgConf.msgName_[12] == 'D'))  //
            {
                const std::string search = msgConf.msgName_.substr(14);  // NMEA-STANDARD-GGA -> GGA
                info = std::find_if(messages_.begin(), messages_.end(),
                    [&search](const auto& cand) { return StrEndsWith(cand.first, search); });
            } else {
                info = messages_.find(msgConf.msgName_);
            }
            enabled = !((info == messages_.end()) || (info->second.age_ > 1.5f));

            if (ImGui::BeginMenu(enabled ? msgConf.menuNameEnabled_.c_str() : msgConf.menuNameDisabled_.c_str())) {
                if (ImGui::MenuItem("Enable", NULL, false, msgConf.cmdEnable_ && !enabled)) {
                    input_->receiver_->Write(*msgConf.cmdEnable_);
                }
                if (ImGui::MenuItem("Disable", NULL, false, msgConf.cmdDisable_ && enabled)) {
                    input_->receiver_->Write(*msgConf.cmdDisable_);
                }
                if (ImGui::MenuItem("Poll", NULL, false, msgConf.cmdPoll_ ? true : false)) {
                    input_->receiver_->Write(*msgConf.cmdPoll_);
                }
                ImGui::EndMenu();
            }
        }
    }
    if (groupMenuOpen) {
        ImGui::EndMenu();
    }

    Gui::EndMenuPersist();
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWinDataMessages::MsgInfo::MsgInfo(
    const std::string& msgName, const std::string& winName, std::unique_ptr<GuiMsg> guiMsg) /* clang-format off */ :
    name_      { msgName },
    nameId_    { std::hash<std::string>{}(name_) },
    imguiId_   { "###" + name_ },
    hexdump_   { winName },
    guiMsg_    { std::move(guiMsg) }  // clang-format on
{
    const auto offs = name_.rfind("-");
    group_ = (offs != std::string::npos ? name_.substr(0, offs) : "Other");
    groupId_ = std::hash<std::string>{}(group_);
    Reset();
}

void GuiWinDataMessages::MsgInfo::Reset()
{
    msg_.reset();
    count_ = 0;
    bytes_ = 0;
    msgRate_ = 0.0f;
    byteRate_ = 0.0f;
    age_ = 0.0f;
    msgRateStr_.clear();
    byteRateStr_.clear();
    ageStr_.clear();
    countStr_.clear();
    bytesStr_.clear();
    recent_ = false;
    intervals_.fill(0.0f);
    sizes_.fill(0);
    ix_ = 0;
}

void GuiWinDataMessages::MsgInfo::Update(const DataMsgPtr& msg)
{
    if (!msg) {
        return;
    }

    // Store time since last message [ms]
    if (msg_) {
        intervals_[ix_] = (msg->time_ - msg_->time_).GetSec();
        sizes_[ix_] = msg->msg_.Size();
        ix_++;
        ix_ %= N;
    }

    // Store new message
    count_++;
    bytes_ += msg->msg_.Size();
    msg_ = msg;

    // Calculate average message and size rates
    float tTot = 0.0f;
    float sTot = 0.0f;
    int num = 0;
    for (std::size_t ix = 0; ix < intervals_.size(); ix++) {
        if (intervals_[ix] != 0.0f) {
            tTot += intervals_[ix];
            sTot += sizes_[ix];
            num++;
        }
    }
    msgRate_ = (num > 0 ? 1.0f / (tTot / (float)num) : 0.0f);
    byteRate_ = (tTot > 0.0f ? (sTot / tTot) : 0.0f);

    countStr_ = Sprintf("%6" PRIuMAX, count_);
    bytesStr_ = Sprintf("%7" PRIuMAX, bytes_);
    msgRateStr_ = Sprintf("%4.1f", msgRate_);
    byteRateStr_ = Sprintf("%5.0f", byteRate_);

    recent_ = true;

    hexdump_.Set(msg_->msg_.data_);

    if (guiMsg_) {
        guiMsg_->Update(msg);
    }
}

void GuiWinDataMessages::MsgInfo::Update(const Time& now)
{
    if (!msg_) {
        age_ = 0.0f;
    } else {
        age_ = (now - msg_->time_).GetSec();
    }
    if (age_ > 300.0f) {
        age_ = 0.0f;
    }

    ageStr_ = (age_ != 0.0 ? Sprintf("%4.1f", age_) : "-");


    // Stop displaying rate?
    if ((age_ > 10.0f) || (msgRate_ < 0.05f) || (msgRate_ > 99.5f)) {
        msgRate_ = 0.0f;
        msgRateStr_.clear();
        byteRate_ = 0.0f;
        byteRateStr_.clear();
        intervals_.fill(0.0f);
        sizes_.fill(0);
    }

    recent_ = ((age_ > 0.0f) && (age_ < 0.2f));

    if (guiMsg_) {
        guiMsg_->Update(now);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

static std::unique_ptr<ParserMsg> sMakeUbxPoll(const std::string& msgName);

/*static*/ std::map<std::string, GuiWinDataMessages::MsgConf> GuiWinDataMessages::msgConfs_;

void GuiWinDataMessages::_InitMsgConfs()
{
    if (!msgConfs_.empty()) {
        return;
    }
    DEBUG("GuiWinDataMessages::_InitMsgConfs()");

    // UBX messages with rate config
    int numMsgRates = 0;
    const UBLOXCFG_MSGRATE_t** msgRates = ubloxcfg_getAllMsgRateCfgs(&numMsgRates);
    for (int ix = 0; ix < numMsgRates; ix++) {
        const UBLOXCFG_MSGRATE_t* rate = msgRates[ix];
        msgConfs_.emplace(rate->msgName, *rate);
    }
    // Pollable non-periodic messages
    msgConfs_.emplace(UBX_MON_VER_STRID, UBX_MON_VER_STRID).first->second.cmdPoll_ =
        std::make_unique<ParserMsg>(GetCommonMessage(CommonMessage::UBX_MON_VER));
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWinDataMessages::MsgConf::MsgConf(const std::string& msgName) /* clang-format off */ :
    msgName_   { msgName } // clang-format on
{
    const auto offs = msgName_.rfind("-");
    group_ = (offs != std::string::npos ? msgName_.substr(0, offs) : "Other");
    groupId_ = std::hash<std::string>{}(group_);

    // "(#) UBX-CLASS-MESSAGE##UBX-CLASS-MESSAGE"
    menuNameEnabled_ = ICON_FK_CIRCLE " " + msgName_ + "###" + msgName_;
    // "( ) UBX-CLASS-MESSAGE##UBX-CLASS-MESSAGE"
    menuNameDisabled_ = ICON_FK_CIRCLE_THIN " " + msgName_ + "###" + msgName_;

    // DEBUG("MsgConf: %s (%s)", msgName_.c_str(), group_.c_str());
}

// ---------------------------------------------------------------------------------------------------------------------

static std::unique_ptr<ParserMsg> sMakeUbxEnableDisable(const UBLOXCFG_MSGRATE_t& msgRate, const bool enable)
{
    std::vector<UBLOXCFG_KEYVAL_t> keyVal;
    const uint8_t val = (enable ? 1 : 0);  // clang-format off
    if (msgRate.itemUart1) { keyVal.push_back({ .id = msgRate.itemUart1->id, .val = { .U1 = val } }); }
    if (msgRate.itemUart2) { keyVal.push_back({ .id = msgRate.itemUart2->id, .val = { .U1 = val } }); }
    if (msgRate.itemSpi  ) { keyVal.push_back({ .id = msgRate.itemSpi->id,   .val = { .U1 = val } }); }
    if (msgRate.itemI2c  ) { keyVal.push_back({ .id = msgRate.itemI2c->id,   .val = { .U1 = val } }); }
    if (msgRate.itemUsb  ) { keyVal.push_back({ .id = msgRate.itemUsb->id,   .val = { .U1 = val } }); }  // clang-format on

    if (keyVal.empty()) {
        return nullptr;
    }

    auto msgs = MakeUbxCfgValset({ UBLOXCFG_LAYER_RAM }, keyVal);

    if (!msgs.empty()) {
        return std::make_unique<ParserMsg>(std::move(msgs.front()));
    } else {
        return nullptr;
    }
}

static std::unique_ptr<ParserMsg> sMakeUbxPoll(const std::string& msgName)
{
    UbxMsgInfo info;
    if (!UbxMessageInfo(msgName, info)) {
        return nullptr;
    }

    auto msg = MakeUbxParserMsg(info.cls_id_, info.msg_id_, {});

    if (msg) {
        return std::make_unique<ParserMsg>(std::move(msg.value()));
    } else {
        return nullptr;
    }
}

GuiWinDataMessages::MsgConf::MsgConf(const UBLOXCFG_MSGRATE_t& msgRate) : MsgConf(msgRate.msgName)
{
    cmdEnable_ = sMakeUbxEnableDisable(msgRate, true);
    cmdDisable_ = sMakeUbxEnableDisable(msgRate, false);
    if ((msgName_ != UBX_RXM_SFRBX_STRID) && (msgName_ != UBX_RXM_RTCM_STRID)) { // not pollable
        cmdPoll_ = sMakeUbxPoll(msgName_);
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
