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
#include <fpsdk_common/parser/rtcm3.hpp>
using namespace fpsdk::common::parser::rtcm3;
#include <fpsdk_common/parser/nmea.hpp>
using namespace fpsdk::common::parser::nmea;
#include <ffxx/utils.hpp>
#include <ffxx/stream.hpp>
using namespace ffxx;
//
#include "gui_win_input_corr.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinInputCorr::GuiWinInputCorr(const std::string& name) /* clang-format off */ :
    GuiWinInput(name, { 80, 25, 0, 0 }, ImGuiWindowFlags_None, CreateInput(name, this, InputType_Corr)),
    streamSpec_   { WinName(), GUI_CFG.corrSpecHistory, std::bind(&GuiWinInputCorr::_OnBaudrateChange, this, std::placeholders::_1) }  // clang-format on
{
    DEBUG("GuiWinInputCorr(%s)", WinName().c_str());
    GuiGlobal::LoadObj(WinName() + ".GuiWinInputCorr", cfg_);

    input_->corr_ = std::make_shared<Receiver>(WinName(), input_->srcId_);
    //input_->database_ = std::make_shared<Database>(WinName());

    _Init();
    _UpdateGga();
}

GuiWinInputCorr::~GuiWinInputCorr()
{
    DEBUG("~GuiWinInputCorr(%s)", WinName().c_str());
    GuiGlobal::SaveObj(WinName() + ".GuiWinInputCorr", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputCorr::_ProcessData(const DataPtr& data)
{
    if (data->type_ == DataType::MSG) {
        auto& msg = DataPtrToDataMsg(data);
        status_.Update(msg);

        // Forward data
        for (auto& [name, input] : GetInputs()) {
            if (!input->receiver_) {
                continue;
            }

            // No correction data forward config avail
            auto cfg = cfg_.corrFwd.find(name);
            if (cfg == cfg_.corrFwd.end()) {
                continue;
            }

            const int flags = (int)cfg->second;

            // Forward disabled
            if (!CheckBitsAny(flags, (int)CorrFwdCfg::Enable)) {
                continue;
            }

            bool forward = false;

            // Consider forwarding RTCM3 messages
            if (msg.msg_.proto_ == Protocol::RTCM3) {
                const uint16_t msgType = Rtcm3Type(msg.msg_.Data());
                Rtcm3MsmGnss gnss;
                Rtcm3MsmType msm;

                // MSM messages, biases
                if ((msgType == RTCM3_TYPE1230_MSGID) || Rtcm3TypeToMsm(msgType, gnss, msm)) {
                    forward = CheckBitsAny(flags, (int)CorrFwdCfg::Rtcm3Msm);
                }
                // Station messages
                else if ((msgType == RTCM3_TYPE1005_MSGID) || (msgType == RTCM3_TYPE1032_MSGID)) {
                    forward = CheckBitsAny(flags, (int)CorrFwdCfg::Rtcm3Sta);
                }
                // Other RTCM3 message
                else  {
                    forward = CheckBitsAny(flags, (int)CorrFwdCfg::Rtcm3Other);
                }
            }
            // Consider forwarding SPARTN messages
            else if (msg.msg_.proto_ == Protocol::SPARTN) {
                forward = CheckBitsAny(flags, (int)CorrFwdCfg::Spartn);
            }
            // Any other message is considered noise
            else {
                forward = CheckBitsAny(flags, (int)CorrFwdCfg::Noise);
            }

            // Send message to receiver
            if (forward) {
                // DEBUG("%s < %s %s", name.c_str(), msg.msg_.name_.c_str(), msg.msg_.info_.c_str());
                input->receiver_->Write(msg.msg_);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputCorr::_Loop(const Time& now)
{
    status_.Update(now);

    if (((now - ggaLastSent_) >= ggaInterval_) && (input_->corr_->GetState() == StreamState::CONNECTED)) {
        _UpdateGga();
        if (ggaMsgOk_) {
            input_->corr_->Write(ggaMsg_);
            ggaLastSent_ = now;
        } else {
            ggaLastSent_ = now - Duration::FromSec(1.0); // try again soon
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputCorr::_ClearData()
{
    status_ = {};
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputCorr::_DrawButtons()
{
    ImGui::NewLine();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputCorr::_DrawStatus()
{
    const float tab = GUI_VAR.charSizeX * 11;

    ImGui::TextUnformatted("Status:   ");
    ImGui::SameLine(tab);
    ImGui::TextColored(status_.okSta_ ? C4_C_BRIGHTGREEN() : C4_C_BRIGHTRED(), "Station");
    ImGui::SameLine(tab * 2.0f);
    ImGui::TextColored(status_.okGps_ ? C4_C_BRIGHTGREEN() : C4_C_BRIGHTRED(), "GPS MSM");
    ImGui::SameLine(tab * 3.0f);
    ImGui::TextColored(status_.okGal_ ? C4_C_BRIGHTGREEN() : C4_C_BRIGHTRED(), "GAL MSM");
    ImGui::SameLine(tab * 4.0f);
    ImGui::TextColored(status_.okBds_ ? C4_C_BRIGHTGREEN() : C4_C_BRIGHTRED(), "BDS MSM");
    ImGui::SameLine(tab * 5.0f);
    ImGui::TextColored(status_.okGlo_ ? C4_C_BRIGHTGREEN() : C4_C_BRIGHTRED(), "GLO MSM");
    ImGui::SameLine(tab * 6.0f);
    ImGui::TextColored(status_.okBias_ ? C4_C_BRIGHTGREEN() : C4_C_BRIGHTRED(), "GLO bias");
    ImGui::SameLine(tab * 7.0f);
    ImGui::TextColored(status_.okNoise_ ? C4_C_BRIGHTGREEN() : C4_C_BRIGHTRED(), "Noise");
    ImGui::SameLine(tab * 8.0f);
    ImGui::TextColored(status_.latencyOk_ ? C4_C_BRIGHTGREEN() : C4_C_BRIGHTRED(), "Latency");

    ImGui::TextUnformatted("Count:");
    ImGui::SameLine(tab);
    ImGui::Text("%-8" PRIuMAX " ", status_.nSta_);
    ImGui::SameLine(tab * 2.0f);
    ImGui::Text("%-8" PRIuMAX " ", status_.nGps_);
    ImGui::SameLine(tab * 3.0f);
    ImGui::Text("%-8" PRIuMAX " ", status_.nGal_);
    ImGui::SameLine(tab * 4.0f);
    ImGui::Text("%-8" PRIuMAX " ", status_.nBds_);
    ImGui::SameLine(tab * 5.0f);
    ImGui::Text("%-8" PRIuMAX " ", status_.nGlo_);
    ImGui::SameLine(tab * 6.0f);
    ImGui::Text("%-8" PRIuMAX, status_.nBias_);
    ImGui::SameLine(tab * 7.0f);
    ImGui::Text("%-8" PRIuMAX, status_.nNoise_);
    ImGui::SameLine(tab * 8.0f);
    ImGui::Text("%.1f", status_.latency_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputCorr::_DrawControls()
{
    // Connect/disconnect/abort button
    auto& corr = *input_->corr_;
    const auto streamState = corr.GetState();
    bool allowBaudrate = true;
    switch (streamState) {
        case StreamState::CLOSED: {
            const bool streamSpecOk = streamSpec_.SpecOk();
            ImGui::BeginDisabled(!streamSpecOk);
            if (ImGui::Button(ICON_FK_PLAY "###StartStop", GUI_VAR.iconSize) || (streamSpecOk && triggerConnect_)) {
                const auto spec = streamSpec_.GetSpec();
                if (corr.Start(spec)) {
                    streamSpec_.AddHistory(spec);
                    versionStr_ = corr.GetStream()->GetOpts().disp_;
                    _UpdateWinTitles();
                    ggaLastSent_ = {};
                }
                triggerConnect_ = false;
            }
            ImGui::EndDisabled();
            Gui::ItemTooltip("Connect");
            break;
        }
        case StreamState::CONNECTING: {
            allowBaudrate = false;
            ImGui::PushStyleColor(ImGuiCol_Text, C_C_GREEN());
            if (ImGui::Button(ICON_FK_EJECT "###StartStop", GUI_VAR.iconSize)) {
                corr.Stop();
            }
            ImGui::PopStyleColor();
            Gui::ItemTooltip("Disconnect");
            break;
        }
        case StreamState::CONNECTED: {
            ImGui::PushStyleColor(ImGuiCol_Text, C_C_BRIGHTGREEN());
            if (ImGui::Button(ICON_FK_STOP "###StartStop", GUI_VAR.iconSize)) {
                corr.Stop();
                latestEpoch_.reset();
                versionStr_.clear();
                status_ = {};
                _UpdateWinTitles();
            }
            ImGui::PopStyleColor();
            Gui::ItemTooltip("Disconnect");
            break;
        }
        case StreamState::ERROR: {
            allowBaudrate = false;
            ImGui::PushStyleColor(ImGuiCol_Text, C_C_BRIGHTRED());
            if (ImGui::Button(ICON_FK_EJECT "###StartStop", GUI_VAR.iconSize)) {
                corr.Stop();
            }
            ImGui::PopStyleColor();
            Gui::ItemTooltip("Disconnect");
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
        const uint32_t baudrate = corr.GetBaudrate();
        if (baudrate != streamSpec_.Baudrate()) {
            streamSpec_.SetBaudrate(baudrate);
        }
    }

    // GGA
    ImGui::Separator();

    const float height = (2.0f * GUI_VAR.lineHeight) + (3.0f * GUI_VAR.imguiStyle->SeparatorTextPadding.y);

    auto& inputs = GetInputs();

    if (ImGui::BeginChild("##GgaLeft", ImVec2(GUI_VAR.charSizeX * 30.0f, height), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        const float indent = GUI_VAR.charSizeX * 15.0f;

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("GGA source");
        ImGui::SameLine(indent);
        ImGui::SetNextItemWidth(-FLT_MIN);
        const char* ggaSrcStr = (cfg_.ggaSrc == GgaSrc::OFF ? "Off" :  // clang-format off
                                (cfg_.ggaSrc == GgaSrc::MANUAL ? "Manual" :
                                (inputs.count(cfg_.ggaReceiver) > 0 ? cfg_.ggaReceiver.c_str() : ""))); // clang-format on
        if (ImGui::BeginCombo("##GgaSrc", ggaSrcStr)) {
            if (ImGui::Selectable("Off", cfg_.ggaSrc == GgaSrc::OFF)) {
                cfg_.ggaSrc = GgaSrc::OFF;
                cfg_.ggaReceiver.clear();
                _UpdateGga();
            }
            if (ImGui::Selectable("Manual", cfg_.ggaSrc == GgaSrc::MANUAL)) {
                cfg_.ggaSrc = GgaSrc::MANUAL;
                cfg_.ggaReceiver.clear();
                _UpdateGga();
            }
            for (auto& [name, input] : inputs) {
                if (input->receiver_) {
                    if (ImGui::Selectable(input->name_.c_str(), input->name_ == cfg_.ggaReceiver)) {
                        cfg_.ggaSrc = GgaSrc::RECEIVER;
                        cfg_.ggaReceiver = input->name_;
                        _UpdateGga();
                    }
                }
            }
            ImGui::EndCombo();
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("GGA interval");
        ImGui::SameLine(indent);
        ImGui::BeginDisabled(cfg_.ggaSrc == GgaSrc::OFF);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::DragFloat("##ggaInt", &cfg_.ggaInt, 0.1f, StreamOpts::GGA_PERIOD_MIN, StreamOpts::GGA_PERIOD_MAX, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
            _UpdateGga();
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();

    Gui::VerticalSeparator();

    if (ImGui::BeginChild("##GgaRight", ImVec2(0, height), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

        const float indent = GUI_VAR.charSizeX * 15.0f;

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("GGA message");
        ImGui::SameLine(indent);

        ImGui::BeginDisabled(cfg_.ggaSrc == GgaSrc::OFF);

        // Talker
        static constexpr std::array<const char*, 2> ggaTalkerStr = { { "GN", "GP" } };
        int ggaTalker = EnumToVal(cfg_.ggaTalker);
        ImGui::SetNextItemWidth(GUI_VAR.charSizeX * 6.0f);
        if (ImGui::BeginCombo("##GgaTalker", ggaTalkerStr[ggaTalker])) {
            for (int ix = 0; ix < (int)ggaTalkerStr.size(); ix++) {
                if (ImGui::Selectable(ggaTalkerStr[ix], ix == ggaTalker)) {
                    cfg_.ggaTalker = (GgaTalker)ix;
                    _UpdateGga();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();

        // Number of SV
        ImGui::SetNextItemWidth(GUI_VAR.charSizeX * 4);
        if (ImGui::InputInt("##GgaNumSv", &cfg_.ggaNumSv, 0, 0)) {
            _UpdateGga();
        }
        Gui::ItemTooltip("Number of satellites used");

        ImGui::SameLine();

        // Fix valid
        if (ImGui::Checkbox("##GgaFix", &cfg_.ggaFix)) {
            _UpdateGga();
        }
        Gui::ItemTooltip("Valid position fix");

        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(cfg_.ggaSrc != GgaSrc::MANUAL);

        // Latitude
        ImGui::SetNextItemWidth(GUI_VAR.charSizeX * 12);
        if (ImGui::InputDouble("##GgaLat", &cfg_.ggaHeight, 0.0, 0.0, "%.8f")) {
            _UpdateGga();
        }
        Gui::ItemTooltip("Latitude [deg]");

        ImGui::SameLine();

        // Longitude
        ImGui::SetNextItemWidth(GUI_VAR.charSizeX * 12);
        if (ImGui::InputDouble("##GgaLon", &cfg_.ggaLon, 0.0, 0.0, "%.8f")) {
            _UpdateGga();
        }
        Gui::ItemTooltip("Longitude [deg]");

        ImGui::SameLine();

        // Altitude
        ImGui::SetNextItemWidth(GUI_VAR.charSizeX * 7);
        if (ImGui::InputDouble("##GgaAlt", &cfg_.ggaAlt, 0.0, 0.0, "%.0f")) {
            _UpdateGga();
        }
        Gui::ItemTooltip("Altitude [m]");

        ImGui::EndDisabled();


        ImGui::BeginDisabled((cfg_.ggaSrc == GgaSrc::OFF) || !ggaMsgOk_);
        if (ImGui::Button("Send now")) {
            ggaLastSent_ = {};
        }
        ImGui::EndDisabled();

        ImGui::SameLine(indent);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(ggaInfo_.c_str());
    }
    ImGui::EndChild();

    // Forward data
    for (auto& [name, input] : inputs) {
        if (!input->receiver_ || !input->database_) {
            continue;
        }
        ImGui::Separator();

        auto cfg = cfg_.corrFwd.find(name);
        if (cfg == cfg_.corrFwd.end()) {
            static constexpr CorrFwdCfg def = (CorrFwdCfg)((int)CorrFwdCfg::Enable | (int)CorrFwdCfg::Rtcm3Sta | (int)CorrFwdCfg::Rtcm3Msm | (int)CorrFwdCfg::Spartn);
            cfg = cfg_.corrFwd.emplace(name, def).first;
        }

        ImGui::PushID(name.c_str());

        // clang-format off
        static constexpr struct { const char* label; const char *tooltip; const int flag; } forwards[] = {
            { "Sta",     "RTCM3 station messages",                 (int)CorrFwdCfg::Rtcm3Sta   },
            { "MSM",     "RTCM3 MSM messages (and biases, etc.)",  (int)CorrFwdCfg::Rtcm3Msm   },
            { "Other",   "Other RTCM3 messages (epemerides etc.)", (int)CorrFwdCfg::Rtcm3Other },
            { "SPARTN",  "SPARTN messages",                        (int)CorrFwdCfg::Spartn     },
            { "Noise",   "Any other message",                      (int)CorrFwdCfg::Noise      },
        };
        // clang-format on
        int flags = (int)cfg->second;

        // Master switch
        bool master = CheckBitsAny(flags, (int)CorrFwdCfg::Enable);
        if (ImGui::Checkbox("Enable", &master)) {
            if (master) { SetBits(flags, (int)CorrFwdCfg::Enable); } else { ClearBits(flags, (int)CorrFwdCfg::Enable); }
            cfg->second = (CorrFwdCfg)flags;
        }
        Gui::VerticalSeparator();
        ImGui::BeginDisabled(!master);
        for (const auto& fwd : forwards) {
            bool ena = CheckBitsAny(flags, fwd.flag);
            if (ImGui::Checkbox(fwd.label, &ena)) {
                if (ena) { SetBits(flags, fwd.flag); } else { ClearBits(flags, fwd.flag); }
                cfg->second = (CorrFwdCfg)flags;
            }
            Gui::ItemTooltip(fwd.tooltip);
            ImGui::SameLine();
        }

        ImGui::AlignTextToFramePadding();
        auto rxState = input->receiver_->GetState();
        ImGui::Text(" --> %s (%s)", name.c_str(), rxState == StreamState::CONNECTED ?
            input->database_->LatestRow().fix_str : StreamStateStr(rxState));

        ImGui::EndDisabled();

        ImGui::PopID();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputCorr::_OnBaudrateChange(const uint32_t baudrate)
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

// ---------------------------------------------------------------------------------------------------------------------

GuiWinInputCorr::Status::Status()
{
    latMeas_.fill(0.0);
}

void GuiWinInputCorr::Status::Update(const DataMsg& msg)
{
    const auto& m = msg.msg_;
    bool usefulMsg = false;
    if (m.proto_ == Protocol::RTCM3) {
        Rtcm3MsmHeader msm;
        Time dataTs;
        switch (Rtcm3Type(m.Data())) {
            case RTCM3_TYPE1005_MSGID:
            case RTCM3_TYPE1006_MSGID:
                lastSta_ = msg.time_;
                nSta_++;
                usefulMsg = true;
                break;
            case RTCM3_TYPE1074_MSGID:
            case RTCM3_TYPE1075_MSGID:
            case RTCM3_TYPE1076_MSGID:
            case RTCM3_TYPE1077_MSGID:
                lastGps_ = msg.time_;
                nGps_++;
                usefulMsg = true;
                if (Rtcm3GetMsmHeader(m.Data(), msm) &&
                    dataTs.SetWnoTow({ msg.time_.GetWnoTow(WnoTow::Sys::GPS).wno_, msm.gps_tow_, WnoTow::Sys::GPS })) {
                    latMeas_[latIx_] = (msg.time_ - dataTs).GetSec();
                    // DEBUG("latency %.3f", latMeas_[latIx_]);
                    latIx_++;
                    latIx_ %= latMeas_.size();
                    int nLat = 0;
                    double sLat = 0.0;
                    for (const auto lat : latMeas_) {
                        if (lat != 0.0) {
                            nLat++;
                            sLat += lat;
                        }
                    }
                    if (nLat > 5) {
                        latency_ = sLat / (double)nLat;
                        latencyOk_ = (latency_ < 2.5);
                    } else {
                        latencyOk_ = false;
                    }
                }
                break;
            case RTCM3_TYPE1094_MSGID:
            case RTCM3_TYPE1095_MSGID:
            case RTCM3_TYPE1096_MSGID:
            case RTCM3_TYPE1097_MSGID:
                lastGal_ = msg.time_;
                nGal_++;
                usefulMsg = true;
                break;
            case RTCM3_TYPE1124_MSGID:
            case RTCM3_TYPE1125_MSGID:
            case RTCM3_TYPE1126_MSGID:
            case RTCM3_TYPE1127_MSGID:
                lastBds_ = msg.time_;
                nBds_++;
                usefulMsg = true;
                break;
            case RTCM3_TYPE1084_MSGID:
            case RTCM3_TYPE1085_MSGID:
            case RTCM3_TYPE1086_MSGID:
            case RTCM3_TYPE1087_MSGID:
                lastGlo_ = msg.time_;
                nGlo_++;
                usefulMsg = true;
                break;
            case RTCM3_TYPE1230_MSGID:
                lastBias_ = msg.time_;
                nBias_++;
                usefulMsg = true;
                break;
        }

    }
    if (!usefulMsg) {
        lastNoise_ = msg.time_;
        nNoise_++;
    }
}

static const Duration S10 = Duration::FromSec(10.0);
static const Duration S2 = Duration::FromSec(2.0);

void GuiWinInputCorr::Status::Update(const Time& now)
{
    okSta_ = (!lastSta_.IsZero() && ((now - lastSta_) < S10));
    okGps_ = (!lastGps_.IsZero() && ((now - lastGps_) < S2));
    okGal_ = (!lastGal_.IsZero() && ((now - lastGal_) < S2));
    okBds_ = (!lastBds_.IsZero() && ((now - lastBds_) < S2));
    okGlo_ = (!lastGlo_.IsZero() && ((now - lastGlo_) < S2));
    okBias_ = (!lastBias_.IsZero() && ((now - lastBias_) < S10));
    okNoise_ = (lastNoise_.IsZero() || ((now - lastNoise_) > S10));
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinInputCorr::_UpdateGga()
{
    cfg_.ggaInt = std::clamp<float>(cfg_.ggaInt, StreamOpts::GGA_PERIOD_MIN, StreamOpts::GGA_PERIOD_MAX);
    cfg_.ggaHeight = std::clamp(cfg_.ggaHeight, StreamOpts::GGA_LAT_MIN, StreamOpts::GGA_LAT_MAX);
    cfg_.ggaLon = std::clamp(cfg_.ggaLon, StreamOpts::GGA_LON_MIN, StreamOpts::GGA_LON_MAX);
    cfg_.ggaAlt = std::clamp(cfg_.ggaAlt, StreamOpts::GGA_HEIGHT_MIN, StreamOpts::GGA_HEIGHT_MAX);
    cfg_.ggaNumSv = std::clamp(cfg_.ggaNumSv, 0, 99);

    double lat = 0.0;
    double lon = 0.0;
    double height = 0.0;
    ggaMsgOk_ = false;
    ggaMsg_ = {};
    ggaInterval_.SetSec(cfg_.ggaInt);

    switch (cfg_.ggaSrc) {
        case GgaSrc::RECEIVER: {
            auto& inputs = GetInputs();
            auto input = inputs.find(cfg_.ggaReceiver);
            if (input == inputs.end()) {
                ggaInfo_ = cfg_.ggaReceiver + " not available";
                return;
            }

            const auto& row = input->second->database_->LatestRow();
            if (row.fix_type <= FixType::NOFIX) {
                ggaInfo_ = "no fix from " + cfg_.ggaReceiver + " yet";
                return;
            }

            lat = row.pos_llh_lat_deg;
            lon = row.pos_llh_lon_deg;
            height = row.pos_llh_height;
            break;
        }
        case GgaSrc::MANUAL:
            lat = cfg_.ggaHeight;
            lon = cfg_.ggaLon;
            height = cfg_.ggaAlt;
            break;
        case GgaSrc::OFF:
            ggaInfo_ = "GGA disabled";
            return;
            break;
    }

    const NmeaCoordinates clat(lat);
    const NmeaCoordinates clon(lon);
    const UtcTime utc = Time::FromClockRealtime().GetUtcTime(2);
    char ggaStr[100];
    std::snprintf(ggaStr, sizeof(ggaStr),
        "$%sGGA,%02d%02d%05.2f,%02d%08.5f,%c,%03d%08.5f,%c,%d,%02d,2.00,%.1f,M,0.0,M,,",
        cfg_.ggaTalker == GgaTalker::GN ? "GN" : "GP", utc.hour_, utc.min_, utc.sec_, clat.deg_, clat.min_,
        clat.sign_ ? 'N' : 'S', clon.deg_, clon.min_, clon.sign_ ? 'E' : 'W', cfg_.ggaFix ? 1 : 0, cfg_.ggaNumSv,
        height);

    uint8_t ck = 0;
    const std::size_t len = std::strlen(ggaStr);
    for (std::size_t ix = 1; ix < len; ix++) {
        ck ^= ggaStr[ix];
    }
    std::snprintf(&ggaStr[len], sizeof(ggaStr) - len, "*%02X\r\n", ck);

    ggaParser_.Reset();
    ggaMsg_ = {};
    if (ggaParser_.Add((const uint8_t*)ggaStr, len + 5) && ggaParser_.Process(ggaMsg_) && (ggaMsg_.proto_ == Protocol::NMEA)) {
        ggaMsg_.MakeInfo();
        ggaInfo_ = { &ggaStr[0], &ggaStr[len + 3] };
        ggaMsgOk_ = true;
    } else {
        ggaInfo_ = "failed making GGA message";
        ggaMsg_ = {};
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
