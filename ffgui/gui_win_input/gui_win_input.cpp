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
#include <fpsdk_common/parser/fpa.hpp>
#include <fpsdk_common/parser/nmea.hpp>
#include <fpsdk_common/parser/ubx.hpp>
//
#include "gui_win_data_3d.hpp"
#include "gui_win_data_config.hpp"
#include "gui_win_data_control.hpp"
#include "gui_win_data_custom.hpp"
#include "gui_win_data_epoch.hpp"
#include "gui_win_data_fwupdate.hpp"
#include "gui_win_data_inf.hpp"
#include "gui_win_data_log.hpp"
#include "gui_win_data_map.hpp"
#include "gui_win_data_messages.hpp"
#include "gui_win_data_plot.hpp"
#include "gui_win_data_satellites.hpp"
#include "gui_win_data_scatter.hpp"
#include "gui_win_data_signals.hpp"
#include "gui_win_data_stats.hpp"
//
#include "gui_win_input.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

static HistoryEntries dummyEntries; // we're not using the filter here

GuiWinInput::GuiWinInput(const std::string& name, const ImVec4& size, const ImGuiWindowFlags flags,
    const InputPtr& input) /* clang-format off */ :
    GuiWin(name, size, flags | ImGuiWindowFlags_NoDocking),
    input_       { input },
    log_         { WinName(), dummyEntries, false, 2500 }  // clang-format on
{
    DEBUG("GuiWinInput(%s)", WinName().c_str());
    GuiGlobal::LoadObj(WinName() + ".GuiWinInput", cfg_);

    // Further initialisation, namely that involving input_, deferred to _Init(), which is called by the input window
    // instances (GuiWinInputReceiver, GuiWinInputLogfile).
}

void GuiWinInput::_Init()
{
    if (input_->receiver_) {
        latestEpochMaxAge_.SetSec(5.5);
    }

    AddDataObserver(input_->srcId_, this, std::bind(&GuiWinInput::_ProcessInputDataMain, this, std::placeholders::_1));

    _RestoreDataWins();
    _UpdateWinTitles();
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWinInput::~GuiWinInput()
{
    DEBUG("~GuiWinInput(%s)", WinName().c_str());

    RemoveDataObserver(input_->srcId_, this);
    RemoveInput(input_->name_);
    input_.reset();

    GuiGlobal::SaveObj(WinName() + ".GuiWinInput", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInput::_ClearLatestEpoch()
{
    latestEpoch_.reset();
    latestEpochWithSigCnoHist_.reset();
    latestEpochAge_ = NAN;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInput::_ClearDataCommon()
{
    if (input_->database_) {
        input_->database_->Clear();
    }
    _ClearLatestEpoch();
    versionMsg_.reset();
    versionStr_.clear();
    log_.Clear();
    _ClearData();
    for (auto& win : dataWins_) {
        win->ClearData();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInput::_ProcessInputDataMain(const DataPtr& data)
{
    switch (data->type_) {
        case DataType::MSG: {
            const auto& msg = DataPtrToDataMsg(data);
            switch (msg.msg_.proto_) {
                case Protocol::UBX:
                    // Version info
                    if (msg.msg_.name_ == ubx::UBX_MON_VER_STRID) {
                        versionMsg_ = DataPtrToDataMsgPtr(data);
                    }
                    // INF messages
                    else if (ubx::UbxClsId(msg.msg_.Data()) == ubx::UBX_INF_CLSID) {
                        switch (ubx::UbxMsgId(msg.msg_.Data())) {
                            case ubx::UBX_INF_NOTICE_MSGID:
                                INFO("%s: %s", WinName().c_str(), msg.msg_.info_.c_str());
                                log_.AddLine(msg.msg_.info_, C_DEBUG_INFO());
                                break;
                            case ubx::UBX_INF_WARNING_MSGID:
                                WARNING("%s: %s", WinName().c_str(), msg.msg_.info_.c_str());
                                log_.AddLine(msg.msg_.info_, C_DEBUG_WARNING());
                                break;
                            case ubx::UBX_INF_ERROR_MSGID:
                                ERROR("%s: %s", WinName().c_str(), msg.msg_.info_.c_str());
                                log_.AddLine(msg.msg_.info_, C_DEBUG_ERROR());
                                break;
                        }
                    }
                    break;
                case Protocol::NMEA:
                    // 01234567890123456789012345 6
                    // $GNTXT,01,01,02,blabla*XX\r\n
                    if (StrEndsWith(msg.msg_.name_, "-TXT") && (msg.msg_.Size() > 21)) {
                        const std::string str{ (const char*)&msg.msg_.data_[16], msg.msg_.Size() - 21 };
                        switch (msg.msg_.data_[14]) {
                            case '2':  // notice
                                INFO("%s: %s", WinName().c_str(), str.c_str());
                                log_.AddLine(str, C_DEBUG_INFO());
                                break;
                            case '1':  // warning
                                WARNING("%s: %s", WinName().c_str(), str.c_str());
                                log_.AddLine(str, C_DEBUG_WARNING());
                                break;
                            case '0':  // error
                                ERROR("%s: %s", WinName().c_str(), str.c_str());
                                log_.AddLine(str, C_DEBUG_ERROR());
                                break;
                        }
                    }
                    break;
                case Protocol::FP_A:
                    if (msg.msg_.name_ == fpa::FpaTextPayload::MSG_NAME) {
                        fpa::FpaTextPayload p;
                        if (p.SetFromBuf(msg.msg_.data_)) {
                            switch (p.level) {
                                case fpa::FpaTextLevel::INFO:
                                    INFO("%s: %s", WinName().c_str(), p.text);
                                    log_.AddLine(p.text, C_DEBUG_INFO());
                                    break;
                                case fpa::FpaTextLevel::WARNING:
                                    WARNING("%s: %s", WinName().c_str(), p.text);
                                    log_.AddLine(p.text, C_DEBUG_WARNING());
                                    break;
                                case fpa::FpaTextLevel::ERROR:
                                    ERROR("%s: %s", WinName().c_str(), p.text);
                                    log_.AddLine(p.text, C_DEBUG_ERROR());
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                    break;
                default:
                    break;
            }

            break;
        }
        case DataType::EPOCH:
            latestEpoch_ = DataPtrToDataEpochPtr(data);
            if (latestEpoch_->epoch_.haveSigCnoHist) {
                latestEpochWithSigCnoHist_ = latestEpoch_;
            }
            if (input_->database_) {
                input_->database_->AddEpoch(DataPtrToDataEpoch(data), (input_->receiver_ || input_->corr_));
            }
            break;
        case DataType::DEBUG: {
            auto& debug = DataPtrToDataDebug(data);
            log_.AddLine(debug.str_, debug.level_);
            break;
        }
        case DataType::EVENT:
            break;
    }

    _ProcessData(data);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInput::Loop(const Time& now)
{
    // Process version message
    if (versionMsg_) {
        const auto& msg = DataMsgPtrToDataMsg(versionMsg_);
        if (msg.msg_.name_ == ubx::UBX_MON_VER_STRID) {
            char str[100];
            if (ubx::UbxMonVerToVerStr(str, sizeof(str), msg.msg_.Data(), msg.msg_.Size())) {
                versionStr_ = str;
                _UpdateWinTitles();
            }
            versionMsg_.reset();
        }
    }

    // Expire epoch
    if (latestEpoch_) {
        if (!latestEpochMaxAge_.IsZero() && ((now - latestEpoch_->time_) > latestEpochMaxAge_)) {
            latestEpoch_.reset();
            latestEpochAge_ = NAN;
        } else {
            latestEpochAge_ = (now - latestEpoch_->time_).GetSec();
        }
    }
    if (latestEpochWithSigCnoHist_ && !latestEpochMaxAge_.IsZero() &&
        ((now - latestEpochWithSigCnoHist_->time_) > latestEpochMaxAge_)) {
        latestEpochWithSigCnoHist_.reset();
    }

    _Loop(now);

    for (auto& win : dataWins_) {
        win->Loop(now);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

// clang-format off
#define DATA_WINS_DEFS(_P_) /* class   flags                                              name/title          tooltip/menu         button */ \
    _P_(GuiWinDataLog,         InputType_Receiver | InputType_Logfile | InputType_Corr,  "Messages_log",      "Messages log",      ICON_FK_LIST_UL        "##Log") \
    _P_(GuiWinDataMessages,    InputType_Receiver | InputType_Logfile | InputType_Corr,  "Messages",          "Messages",          ICON_FK_SORT_ALPHA_ASC "##Messages") \
    _P_(GuiWinDataInf,         InputType_Receiver | InputType_Logfile,                   "Inf_messages",      "Inf messages",      ICON_FK_FILE_TEXT_O    "##Inf") \
    _P_(GuiWinDataScatter,     InputType_Receiver | InputType_Logfile,                   "Scatter_plot",      "Scatter plot",      ICON_FK_CROSSHAIRS     "##Scatter") \
    _P_(GuiWinDataSignals,     InputType_Receiver | InputType_Logfile,                   "Signals",           "Signals",           ICON_FK_SIGNAL         "##Signals") \
    _P_(GuiWinDataSatellites,  InputType_Receiver | InputType_Logfile,                   "Satellites",        "Satellites",        ICON_FK_ROCKET         "##Satellites") \
    _P_(GuiWinDataConfig,      InputType_Receiver,                                       "Configuration",     "Configuration",     ICON_FK_PAW            "##Config") \
    _P_(GuiWinDataPlot,        InputType_Receiver | InputType_Logfile,                   "Plots",             "Plots",             ICON_FK_LINE_CHART     "##Plots") \
    _P_(GuiWinDataMap,         InputType_Receiver | InputType_Logfile,                   "Map",               "Map",               ICON_FK_MAP            "##Map") \
    _P_(GuiWinDataStats,       InputType_Receiver | InputType_Logfile,                   "Statistics",        "Statistics",        ICON_FK_TABLE          "##Stats") \
    _P_(GuiWinDataEpoch,       InputType_Receiver | InputType_Logfile,                   "Epoch_details",     "Epoch details",     ICON_FK_TH             "##Epoch") \
    _P_(GuiWinDataFwupdate,    InputType_Receiver,                                       "Firmware_update",   "Firmware update",   ICON_FK_DOWNLOAD       "##Fwupdate") \
    _P_(GuiWinDataCustom,      InputType_Receiver | InputType_Logfile,                   "Custom_message",    "Custom message",    ICON_FK_TERMINAL       "##Custom") \
    _P_(GuiWinDataControl,     InputType_Receiver,                                       "Receiver_control",  "Receiver control",  ICON_FK_WRENCH         "##Control") \
    _P_(GuiWinData3d,          InputType_Receiver | InputType_Logfile,                   "3d_view",           "3d view",           ICON_FK_ROAD           "##3dView") \
    /* end of list */
// clang-format on


static constexpr int MAX_DATA_WINS = 5;

template <typename WinT>
void GuiWinInput::_AddDataWin(const std::string& baseName, const std::string& prevWinName)
{
    // Restoring a previously open window.
    if (!prevWinName.empty()) {
        dataWins_.push_back(std::make_unique<WinT>(prevWinName, input_));
    }
    // Create a new window
    else {
        for (int n = 1; n <= MAX_DATA_WINS; n++) {
            const std::string winName = baseName + "_" + std::to_string(n);
            if (std::find_if(dataWins_.begin(), dataWins_.end(),
                    [&winName](const auto& win) { return win->WinName() == winName; }) == dataWins_.end()) {
                dataWins_.push_back(std::make_unique<WinT>(winName, input_));
                dataWins_.back()->WinOpen();
                return;
            }
        }
    }
}

void GuiWinInput::_RestoreDataWins()
{
    // Restore previous receiver and logfile windows
    const auto dataWinNames = cfg_.dataWinNames;
    for (const auto& winName : dataWinNames) {
        if (false) {
        }
#define _EVAL1(_class_, _types_, _name_, _tooltip_, _button_)  \
    else if (StrStartsWith(winName, WinName() + "_" + _name_)) \
    {                                                          \
        _AddDataWin<_class_>("", winName);                     \
    }
        DATA_WINS_DEFS(_EVAL1)
#undef _EVAL1
    }
    _UpdateDataWins();
}

void GuiWinInput::_UpdateDataWins()
{
    // Keep them ordered (for the menu)
    dataWins_.sort([](const auto& a, const auto& b) { return a->WinName() < b->WinName(); });

    // Sync persistent config
    cfg_.dataWinNames.clear();
    for (const auto& win : dataWins_) {
        cfg_.dataWinNames.emplace(win->WinName());
    }
}

void GuiWinInput::DataWinMenu()
{
    if (ImGui::MenuItem("Main")) {
        WinFocus();
    }
    if (!dataWins_.empty()) {
        ImGui::Separator();
        const int offs = WinName().size() + 2;
        for (auto& win : dataWins_) {
            if (ImGui::MenuItem(win->WinTitle().substr(offs).c_str())) {
                win->WinOpen();
            }
        }
    }
    ImGui::Separator();

    if (ImGui::BeginMenu("New")) {
#define _EVAL1(_class_, _types_, _name_, _tooltip_, _button_)                  \
    if (CheckBitsAny(input_->type_, _types_) && ImGui::MenuItem(_tooltip_)) { \
        _AddDataWin<_class_>(WinName() + "_" + _name_, "");                    \
        _UpdateDataWins();                                                     \
        _UpdateWinTitles();                                                    \
    }
        DATA_WINS_DEFS(_EVAL1)
#undef _EVAL1
        ImGui::EndMenu();
    }
}
void GuiWinInput::DataWinMenuSwitcher()
{
    if (ImGui::MenuItem(WinTitle().c_str())) {
        WinFocus();
    }
    for (auto& win : dataWins_) {
        if (ImGui::MenuItem(win->WinTitle().c_str())) {
            win->WinOpen();
        }
    }

}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInput::_UpdateWinTitles()
{
    // Set titles
    const auto mainWinName = WinName();  // "Receiver_1"
    auto mainWinTitle = mainWinName;
    StrReplace(mainWinTitle, "_", " ");
    WinSetTitle(mainWinTitle + (versionStr_.empty() ? "" : " - " + versionStr_));

    for (auto& win : dataWins_) {
        auto winTitle = win->WinName().substr(mainWinName.size() + 1);  // "Receiver_1_Log_1" -> "Log_1"
        StrReplace(winTitle, "_", " ");
        win->WinSetTitle(mainWinTitle + ": " + winTitle);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInput::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }

    bool summonDataWindows = false;

    // .................................................................................................................

    // Options, other actions
    if (ImGui::Button(ICON_FK_COG "##Options")) {
        ImGui::OpenPopup("Options");
    }
    Gui::ItemTooltip("Options");
    if (ImGui::BeginPopup("Options")) {
        ImGui::Checkbox("Autohide data windows", &cfg_.autoHideDataWins);
        Gui::ItemTooltip(
            "Automatically hide all data windows if this window is collapsed\n"
            "respectively invisible while docked into another window.");
        if (ImGui::Button("Summon data windows")) {
            summonDataWindows = true;
        }
        ImGui::EndPopup();
    }

    Gui::VerticalSeparator();

#define _EVAL1(_class_, _types_, _name_, _tooltip_, _button_)   \
    if (CheckBitsAny(input_->type_, _types_)) {                \
        ImGui::SameLine();                                      \
        if (ImGui::Button(_button_, GUI_VAR.iconSize)) {        \
            _AddDataWin<_class_>(WinName() + "_" + _name_, ""); \
            _UpdateDataWins();                                  \
            _UpdateWinTitles();                                 \
        }                                                       \
        Gui::ItemTooltip(_tooltip_);                            \
    }
    DATA_WINS_DEFS(_EVAL1)
#undef _EVAL1

    Gui::VerticalSeparator();

    // Clear
    if (ImGui::Button(ICON_FK_ERASER "##Clear", GUI_VAR.iconSize)) {
        _ClearDataCommon();
    }
    Gui::ItemTooltip("Clear all data");


    // Database status
    if (input_->database_) {

        ImGui::SameLine();

        const char* const dbIcons[] = {
            ICON_FK_BATTERY_EMPTY /* "##DbStatus" */,           //  0% ..  20%
            ICON_FK_BATTERY_QUARTER /* "##DbStatus" */,         // 20% ..  40%
            ICON_FK_BATTERY_HALF /* "##DbStatus" */,            // 40% ..  60%
            ICON_FK_BATTERY_THREE_QUARTERS /* "##DbStatus" */,  // 60% ..  80%
            ICON_FK_BATTERY_FULL /* "##DbStatus" */,            // 80% .. 100%
        };
        const int dbSize = input_->database_->Size();
        const int dbUsage = input_->database_->Usage();
        const float dbFull = (float)dbUsage / (float)dbSize;
        const int dbIconIx = std::clamp(dbFull, 0.0f, 1.0f) * (float)(NumOf(dbIcons) - 1);
        if (ImGui::Button(dbIcons[dbIconIx] /*, GUI_VAR.iconSize*/)) {
            ImGui::OpenPopup("DbSettings");
        }
        if (Gui::ItemTooltipBegin()) {
            ImGui::Text("Database %d/%d epochs, %.1f%% full", dbUsage, dbSize, dbFull * 1e2f);
            Gui::ItemTooltipEnd();
        }
        if (ImGui::BeginPopup("DbSettings")) {
            Gui::TextTitle("Database status");
            ImGui::Text("Usage %.3f%%,", dbFull * 1e2f);
            ImGui::Text("%d/%d rows (epochs)", dbUsage, dbSize);
            // ImGui::Text("");
            if (ImGui::SmallButton("Clear database")) {
                input_->database_->Clear();
            }
            ImGui::Separator();
            Gui::TextTitle("Database settings");
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Database size");
            ImGui::BeginDisabled(dbSize == Database::DB_SIZE_DEF);
            int newSize = dbSize;
            if (ImGui::Button(dbSize != Database::DB_SIZE_DEF ? ICON_FK_TIMES : " ", GUI_VAR.iconSize)) {
                newSize = Database::DB_SIZE_DEF;
            }
            Gui::ItemTooltip("Reset to default");
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::PushItemWidth(GUI_VAR.widgetOffs);
            if (ImGui::SliderInt("##dbNumRows", &newSize, Database::DB_SIZE_MIN, Database::DB_SIZE_MAX)) {
                //DEBUG("set size = %d", newSize);
            }
            ImGui::Text("(~%.0fMiB at 100%% use)", (double)(sizeof(Database::Row) * newSize) * (1.0 / 1024.0 / 1024.0));
            ImGui::PopItemWidth();
            if (newSize != dbSize) {
                input_->database_->SetSize(newSize);
            }
            ImGui::EndPopup();
        }
    }

    Gui::VerticalSeparator();

    _DrawButtons();  // virtual

    // Age of latest epoch
    if (latestEpoch_ && CheckBitsAll(input_->type_, InputType_Receiver)) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - (4 * GUI_VAR.charSizeX));
        ImGui::AlignTextToFramePadding();
        if (latestEpochAge_ < 0.95) {
            ImGui::Text("  .%.0f", latestEpochAge_ * 10.0);
        } else {
            ImGui::Text("%4.1f", latestEpochAge_);
        }
    }

    ImGui::Separator();  // ............................................................................................

    _DrawStatus();  // virtual

    ImGui::Separator();  // ............................................................................................

    _DrawControls();  // virtual

    ImGui::Separator();  // ............................................................................................

    log_.DrawWidget();

    if (summonDataWindows) {
        const Vec2f sep = GUI_VAR.charSize * 2;
        Vec2f pos = ImGui::GetWindowPos();
        pos.y += ImGui::GetWindowSize().y + GUI_VAR.imguiStyle->ItemSpacing.y;
        for (auto& win : dataWins_) {
            // if (!win->WinIsDocked())
            {
                win->WinMoveTo(pos);
                win->WinResize();
                win->WinFocus();
                pos += sep;
            }
        }
        WinFocus();
    }

    _DrawWindowEnd();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInput::DrawDataWins(const bool closeFocusedWin)
{
    if (cfg_.autoHideDataWins && !WinIsDrawn()) {
        return;
    }
    // Draw data windows, destroy and remove closed ones
    for (auto iter = dataWins_.begin(); iter != dataWins_.end();) {
        auto& dataWin = *iter;
        if (dataWin->WinIsOpen()) {
            dataWin->DrawWindow();
            if (closeFocusedWin && dataWin->WinIsFocused()) {
                dataWin->WinClose();
            }
            iter++;
        } else {
            iter = dataWins_.erase(iter);
            _UpdateDataWins();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInput::_DrawLatestEpochStatus()
{
    // const EPOCH_t *epoch = _epoch && _epoch->epoch.valid ? &_epoch->epoch : nullptr;
    const float statusHeight = ImGui::GetTextLineHeightWithSpacing() * 10;
    const float maxHeight = ImGui::GetContentRegionAvail().y;

    // Left side
    if (ImGui::BeginChild("##StatusLeft", ImVec2(GUI_VAR.charSizeX * 40, statusHeight), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        const float dataOffs = GUI_VAR.charSizeX * 13;
        const Epoch* epoch = (latestEpoch_ ? &latestEpoch_->epoch_ : nullptr);

        // Sequence / status
        ImGui::Selectable("Seq, uptime");
        ImGui::SameLine(dataOffs);
        if (!epoch) {
            Gui::TextDim("no data");
        } else {
            ImGui::Text("%" PRIuMAX, epoch->seq);
            ImGui::SameLine();
            if (epoch->uptimeStr[0] != '\0') {
                ImGui::TextUnformatted(epoch->uptimeStr);
            } else {
                Gui::TextDim("n/a");
            }
        }
        ImGui::Selectable("Fix type");
        if (epoch && (epoch->fixType != FixType::UNKNOWN)) {
            ImGui::SameLine(dataOffs);
            ImGui::TextColored(FIX_COLOUR4(epoch->fixType, epoch->fixOk), "%s", epoch->fixTypeStr);
            // ImGui::SameLine();
            // Gui::TextDim("%.1f", epoch-> - _fixTime); // TODO
        }

        ImGui::Selectable("Latitude");
        if (epoch && epoch->havePos) {
            ImGui::SameLine(dataOffs);
            if (!epoch->fixOk) {
                ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM());
            }
            ImGui::Text(" %2d° %2d' %9.6f\" %c", std::abs(epoch->posDmsLat.deg_), epoch->posDmsLat.min_,
                epoch->posDmsLat.sec_, epoch->posDmsLat.deg_ < 0 ? 'S' : 'N');
            if (!epoch->fixOk) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::Selectable("Longitude");
        if (epoch && epoch->havePos) {
            ImGui::SameLine(dataOffs);
            if (!epoch->fixOk) {
                ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM());
            }
            ImGui::Text("%3d° %2d' %9.6f\" %c", std::abs(epoch->posDmsLon.deg_), epoch->posDmsLon.min_,
                epoch->posDmsLon.sec_, epoch->posDmsLon.deg_ < 0 ? 'W' : 'E');
            if (!epoch->fixOk) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::Selectable("Height");
        if (epoch && epoch->havePos) {
            ImGui::SameLine(dataOffs);
            if (!epoch->fixOk) {
                ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM());
            }
            ImGui::Text("%.2f m", epoch->posLlh[2]);
            if (epoch->haveHeightMsl) {
                ImGui::SameLine();
                ImGui::Text("(%.1f MSL)", epoch->heightMsl);
            }

            if (!epoch->fixOk) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::Selectable("Accuracy");
        if (epoch && epoch->havePos) {
            ImGui::SameLine(dataOffs);
            if (!epoch->fixOk) {
                ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM());
            }
            if (epoch->havePosAcc > 1000.0) {
                ImGui::Text("H %.1f, V %.1f [km]", epoch->posAccHoriz * 1e-3, epoch->posAccVert * 1e-3);
            } else if (epoch->posAccHoriz > 10.0) {
                ImGui::Text("H %.1f, V %.1f [m]", epoch->posAccHoriz, epoch->posAccVert);
            } else {
                ImGui::Text("H %.3f, V %.3f [m]", epoch->posAccHoriz, epoch->posAccVert);
            }
            if (!epoch->fixOk) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::Selectable("Velocity");
        if (epoch && epoch->haveVel) {
            ImGui::SameLine(dataOffs);
            if (!epoch->fixOk) {
                ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM());
            }
            ImGui::Text("%.2f [m/s] (%.1f [km/h])", epoch->vel3d, epoch->vel3d * (3600.0 / 1000.0));
            if (!epoch->fixOk) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::Selectable("Rel. pos.");
        if (epoch && epoch->haveRelPos) {
            ImGui::SameLine(dataOffs);
            if (!epoch->fixOk) {
                ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM());
            }
            if (epoch->relPosLen > 1000.0) {
                ImGui::Text("N %.1f, E %.1f, D %.1f [km]", epoch->relPosNed[0] * 1e-3, epoch->relPosNed[1] * 1e-3,
                    epoch->relPosNed[2] * 1e-3);
            } else if (epoch->relPosLen > 100.0) {
                ImGui::Text(
                    "N %.0f, E %.0f, D %.0f [m]", epoch->relPosNed[0], epoch->relPosNed[1], epoch->relPosNed[2]);
            } else {
                ImGui::Text(
                    "N %.1f, E %.1f, D %.1f [m]", epoch->relPosNed[0], epoch->relPosNed[1], epoch->relPosNed[2]);
            }
            if (!epoch->fixOk) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::Selectable("GPS time");
        if (epoch) {
            ImGui::SameLine(dataOffs);
            if (!epoch->haveGpsWno) {
                ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM());
            }
            ImGui::Text("%04d", epoch->gpsWno);
            if (!epoch->haveGpsWno) {
                ImGui::PopStyleColor();
            }
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextUnformatted(":");
            ImGui::SameLine(0.0f, 0.0f);
            if (!epoch->haveGpsTow) {
                ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM());
            }
            ImGui::Text(epoch->haveTimeAcc && (epoch->timeAcc < 0.001) ? "%013.6f" : "%010.3f", epoch->gpsTow);
            if (!epoch->haveGpsTow) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::Selectable("Date/time");
        if (epoch) {
            ImGui::SameLine(dataOffs);
            if (!epoch->haveTime) {
                ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM());
            }
            ImGui::Text("%04d-%02d-%02d", epoch->timeUtc.year_, epoch->timeUtc.month_, epoch->timeUtc.day_);
            ImGui::SameLine();
            ImGui::Text("%02d:%02d", epoch->timeUtc.hour_, epoch->timeUtc.min_);
            if (!epoch->haveTime) {
                ImGui::PopStyleColor();
            }
            ImGui::SameLine(0.0f, 0.0f);
            if (!epoch->haveTime || !epoch->leapsKnown) {
                ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_DIM());
            }
            ImGui::Text(":%06.3f", epoch->timeUtc.sec_);
            if (!epoch->haveTime || !epoch->leapsKnown) {
                ImGui::PopStyleColor();
            }
        }
    }
    ImGui::EndChild();

    Gui::VerticalSeparator();

    // Right side
    if (ImGui::BeginChild("##StatusRight", ImVec2(0, std::min(statusHeight, maxHeight)), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

        const float dataOffs = GUI_VAR.charSizeX * 12;
        const Epoch* epoch = (latestEpochWithSigCnoHist_ ? &latestEpochWithSigCnoHist_->epoch_ : nullptr);

        ImGui::Selectable("Sat. used");
        if (epoch) {
            ImGui::SameLine(dataOffs);
            ImGui::Text("%2d (%2dG %1dS %2dE %2dC %1dJ %2dR %1dI)", epoch->numSatUsed.numTotal,
                epoch->numSatUsed.numGps, epoch->numSatUsed.numSbas, epoch->numSatUsed.numGal, epoch->numSatUsed.numBds,
                epoch->numSatUsed.numQzss, epoch->numSatUsed.numGlo, epoch->numSatUsed.numNavic);
        }
        ImGui::Selectable("Sig. used");
        if (epoch) {
            ImGui::SameLine(dataOffs);
            ImGui::Text("%2d (%2dG %1dS %2dE %2dC %1dJ %2dR %1dI)", epoch->numSigUsed.numTotal,
                epoch->numSigUsed.numGps, epoch->numSigUsed.numSbas, epoch->numSigUsed.numGal, epoch->numSigUsed.numBds,
                epoch->numSigUsed.numQzss, epoch->numSigUsed.numGlo, epoch->numSigUsed.numNavic);
        }

        // ImGui::Separator();

        const Vec2f canvasOffs = ImGui::GetCursorScreenPos();
        const Vec2f canvasSize = ImGui::GetContentRegionAvail();
        const Vec2f canvasMax = canvasOffs + canvasSize;
        // DEBUG("%f %f %f %f", canvasOffs.x, canvasOffs.y, canvasSize.x, canvasSize.y);
        const auto charSize = GUI_VAR.charSize;
        if (canvasSize.y < (charSize.y * 5)) {
            ImGui::EndChild();
            return;
        }

        // if (epoch)
        // {
        //     ImGui::Text("%d %d %d %d %d %d %d %d %d %d %d %d", epoch->sigCnoHistTrk[0], epoch->sigCnoHistTrk[1],
        //     epoch->sigCnoHistTrk[2], epoch->sigCnoHistTrk[3], epoch->sigCnoHistTrk[4], epoch->sigCnoHistTrk[5],
        //     epoch->sigCnoHistTrk[6], epoch->sigCnoHistTrk[7], epoch->sigCnoHistTrk[8], epoch->sigCnoHistTrk[9],
        //     epoch->sigCnoHistTrk[10], epoch->sigCnoHistTrk[11]); ImGui::Text("%d %d %d %d %d %d %d %d %d %d %d %d",
        //     epoch->sigCnoHistNav[0], epoch->sigCnoHistNav[1], epoch->sigCnoHistNav[2], epoch->sigCnoHistNav[3],
        //     epoch->sigCnoHistNav[4], epoch->sigCnoHistNav[5], epoch->sigCnoHistNav[6], epoch->sigCnoHistNav[7],
        //     epoch->sigCnoHistNav[8], epoch->sigCnoHistNav[9], epoch->sigCnoHistNav[10], epoch->sigCnoHistNav[11]);
        // }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(canvasOffs, canvasOffs + canvasSize);
        //
        //                +++
        //            +++ +++
        //        +++ +++ +++ +++
        //    +++ +++ +++ +++ +++     +++
        //   ---------------------------------
        //    === === === === === === === ...
        //   ---------------------------------
        //           10      20     30

        // padding between bars and width of bars
        const float padx = 2.0f;
        const float width = (canvasSize.x - ((float)(NumOf<SigCnoHist>() - 1) * padx)) / (float)NumOf<SigCnoHist>();

        // bottom space for x axis labelling
        const float pady = 1.0f + charSize.y + 1.0f + 1.0f + 1.0 + charSize.y + 1.0;

        // scale for signal count (height of bars)
        const float maxTrk = (epoch ? *std::max_element(epoch->sigCnoHistTrk.cbegin(), epoch->sigCnoHistTrk.cend()) : 25.0f);
        const float maxSig = (maxTrk > 25.0f ? 50.0f : 25.0f);
        const float scale = 1.0f / maxSig * (canvasSize.y - pady);

        float x = canvasOffs.x;
        float y = canvasOffs.y + canvasSize.y - pady;

        // draw bars for signals
        if (epoch) {
            for (std::size_t ix = 0; ix < NumOf<SigCnoHist>(); ix++) {
                // tracked signals
                if (epoch->sigCnoHistTrk[ix] > 0) {
                    const float h = (float)epoch->sigCnoHistTrk[ix] * scale;
                    draw->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y - h), C_SIGNAL_UNUSED());
                }
                // signals used
                if (epoch->sigCnoHistNav[ix] > 0) {
                    const float h = (float)epoch->sigCnoHistNav[ix] * scale;
                    draw->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y - h), *(&C_SIGNAL_00_05() + ix));
                }

                x += width + padx;
            }
        }

        x = canvasOffs.x;
        y = canvasOffs.y;

        // y grid
        {
            const float dy = (canvasSize.y - pady) / 5;
            draw->AddLine(ImVec2(x, y), ImVec2(canvasMax.x, y), C_PLOT_GRID_MINOR());
            y += dy;

            draw->AddLine(ImVec2(x, y), ImVec2(canvasMax.x, y), C_PLOT_GRID_MAJOR());
            ImGui::SetCursorScreenPos(ImVec2(x, y + 1.0f));
            y += dy;
            draw->AddLine(ImVec2(x, y), ImVec2(canvasMax.x, y), C_PLOT_GRID_MINOR());
            y += dy;
            ImGui::TextUnformatted(maxSig > 25.0f ? "40" : "20");

            draw->AddLine(ImVec2(x, y), ImVec2(canvasMax.x, y), C_PLOT_GRID_MAJOR());
            ImGui::SetCursorScreenPos(ImVec2(x, y + 1.0f));
            y += dy;
            draw->AddLine(ImVec2(x, y), ImVec2(canvasMax.x, y), C_PLOT_GRID_MINOR());
            y += dy;
            ImGui::TextUnformatted(maxSig > 25.0f ? "20" : "10");
        }

        // x-axis horizontal line
        x = canvasOffs.x;
        y = canvasMax.y - pady + 1.0f;
        draw->AddLine(ImVec2(x, y), ImVec2(canvasMax.x, y), C_PLOT_GRID_MAJOR());

        // x-axis colours and counts
        y += 2.0f;
        for (std::size_t ix = 0; ix < NumOf<SigCnoHist>(); ix++) {
            draw->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + 1.0f + charSize.y), *(&C_SIGNAL_00_05() + ix));
            if (epoch && (width > (2.0f * charSize.x))) {
                const int n = epoch->sigCnoHistNav[ix];
                ImGui::SetCursorScreenPos(ImVec2(x + (0.5f * width) - (n < 10 ? 0.5f * charSize.x : charSize.x), y));

                ImGui::PushStyleColor(ImGuiCol_Text, C_C_BLACK());
                ImGui::Text("%d", n);
                ImGui::PopStyleColor();
            }

            x += width + padx;
        }
        y += 1.0 + charSize.y;

        x = canvasOffs.x;
        y += 1.0;
        draw->AddLine(ImVec2(x, y), ImVec2(canvasMax.x, y), C_PLOT_GRID_MAJOR());

        // x-axis labels
        x = canvasOffs.x + width + padx + width + padx - charSize.x;
        y += 1.0f;

        for (std::size_t ix = 2; ix < NumOf<SigCnoHist>(); ix += 2) {
            ImGui::SetCursorScreenPos(ImVec2(x, y));
            ImGui::Text("%" PRIuMAX, ix * 5);
            x += width + padx + width + padx;
        }

        draw->PopClipRect();

        ImGui::SetCursorScreenPos(canvasOffs);
        ImGui::InvisibleButton("##SigLevPlotTooltop", canvasSize);
        Gui::ItemTooltip("Signal levels (x axis) vs. number of signals used and tracked (y axis)");
    }
    ImGui::EndChild();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
