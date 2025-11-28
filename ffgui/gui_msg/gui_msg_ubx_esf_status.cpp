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
//
#include "gui_msg_ubx_esf_meas.hpp"
#include "gui_msg_ubx_esf_status.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiMsgUbxEsfStatus::GuiMsgUbxEsfStatus(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input),
    table_   { viewName_ }  // clang-format on
{
    table_.AddColumn("Sensor");
    table_.AddColumn("Used");
    table_.AddColumn("Ready");
    table_.AddColumn("Calibration");
    table_.AddColumn("Time");
    table_.AddColumn("Freq");
    table_.AddColumn("Faults");
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxEsfStatus::Update(const DataMsgPtr& msg)
{
    Clear();

    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    if ((UBX_ESF_STATUS_VERSION(data) != UBX_ESF_STATUS_V2_VERSION) || (UBX_ESF_STATUS_V2_SIZE(data) != size)) {
        return;
    }

    UBX_ESF_STATUS_V2_GROUP0 status;
    std::memcpy(&status, &data[UBX_HEAD_SIZE], sizeof(status));

    valid_ = true;
    iTow_ = Sprintf("%10.3f", (double)status.iTOW * UBX_ESF_STATUS_V2_ITOW_SCALE);
    wtInitStatus_ = UBX_ESF_STATUS_V2_INITSTATUS1_WTINITSTATUS(status.initStatus1);
    mntAlgStatus_ = UBX_ESF_STATUS_V2_INITSTATUS1_MNTALGSTATUS(status.initStatus1);
    insInitStatus_ = UBX_ESF_STATUS_V2_INITSTATUS1_INSINITSTATUS(status.initStatus1);
    imuInitStatus_ = UBX_ESF_STATUS_V2_INITSTATUS2_IMUINITSTATUS(status.initStatus2);
    fusionMode_ = status.fusionMode;

    std::vector<Sensor> sensors_;
    for (std::size_t sensIx = 0, offs = UBX_HEAD_SIZE + sizeof(UBX_ESF_STATUS_V2_GROUP0); sensIx < status.numSens;
        sensIx++, offs += (int)sizeof(UBX_ESF_STATUS_V2_GROUP1)) {
        sensors_.emplace_back(Sensor(&data[offs]));
    }
    std::sort(sensors_.begin(), sensors_.end());

    for (const auto& sensor : sensors_) {
        table_.AddCellText(sensor.type_);
        table_.AddCellText(sensor.used_ ? "yes" : "no");
        table_.AddCellText(sensor.ready_ ? "yes" : "no");
        table_.AddCellText(sensor.calibStatus_);
        if (!sensor.calibrated_) {
            table_.SetCellColour(C_TEXT_WARNING());
        }
        table_.AddCellText(sensor.timeStatus_);
        table_.AddCellText(sensor.freq_);
        table_.AddCellText(sensor.faults_);
        if (sensor.used_ && sensor.calibrated_) {
            table_.SetRowColour(C_TEXT_OK());
        } else if (!sensor.faults_.empty()) {
            table_.SetRowColour(C_TEXT_ERROR());
        } else if (!sensor.ready_) {
            table_.SetRowColour(C_TEXT_WARNING());
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxEsfStatus::Update(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxEsfStatus::Clear()
{
    valid_ = false;
    table_.ClearRows();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxEsfStatus::DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxEsfStatus::DrawMsg()
{
    if (!valid_) {
        return;
    }

    const ImVec2 topSize = _CalcTopSize(6);
    const float dataOffs = 25 * GUI_VAR.charSizeX;

    if (ImGui::BeginChild("##Status", topSize)) {
        _RenderStatusText("GPS time", iTow_, dataOffs);
        _RenderStatusFlag(FUSION_MODE_FLAGS, fusionMode_, "Fusion mode", dataOffs);
        _RenderStatusFlag(STATUS_FLAGS_0123, imuInitStatus_, "IMU status", dataOffs);
        _RenderStatusFlag(STATUS_FLAGS_012, wtInitStatus_, "Wheel-ticks status", dataOffs);
        _RenderStatusFlag(STATUS_FLAGS_0123, insInitStatus_, "INS status", dataOffs);
        _RenderStatusFlag(STATUS_FLAGS_0123, mntAlgStatus_, "Mount align status", dataOffs);
    }
    ImGui::EndChild();

    table_.DrawTable();
}

GuiMsgUbxEsfStatus::Sensor::Sensor(const uint8_t* groupData)
{
    UBX_ESF_STATUS_V2_GROUP1 sensor;
    std::memcpy(&sensor, groupData, sizeof(sensor));

    const std::size_t sensorType = UBX_ESF_STATUS_V2_SENSSTATUS1_TYPE(sensor.sensStatus1);
    if ((sensorType < GuiMsgUbxEsfMeas::MEAS_DEFS.size()) && GuiMsgUbxEsfMeas::MEAS_DEFS[sensorType].name_) {
        type_ = Sprintf("%s##%p", GuiMsgUbxEsfMeas::MEAS_DEFS[sensorType].name_, groupData);
    } else {
        type_ = Sprintf("Unknown (type %" PRIuMAX ")##%p", sensorType, groupData);
    }

    used_ = CheckBitsAll(sensor.sensStatus1, UBX_ESF_STATUS_V2_SENSSTATUS1_USED);
    ready_ = CheckBitsAll(sensor.sensStatus1, UBX_ESF_STATUS_V2_SENSSTATUS1_READY);

    constexpr std::array<const char*, 4> calibStatusStrs = { { "not calibrated", "calibrating", "calibrated",
        "calibrated!" } };
    const std::size_t cIx = UBX_ESF_STATUS_V2_SENSSTATUS2_CALIBSTATUS(sensor.sensStatus2);
    calibStatus_ = (cIx < calibStatusStrs.size() ? calibStatusStrs[cIx] : "?");
    calibrated_ = (cIx > 1);

    constexpr std::array<const char*, 4> timeStatusStrs = { { "no data", "first byte", "event input", "time tag" } };
    const std::size_t tIx = UBX_ESF_STATUS_V2_SENSSTATUS2_TIMESTATUS(sensor.sensStatus2);
    timeStatus_ = (tIx < timeStatusStrs.size() ? timeStatusStrs[tIx] : "?");

    freq_ = Sprintf("%d", sensor.freq);

    faults_ = " ";
    if (CheckBitsAll(sensor.faults, UBX_ESF_STATUS_V2_FAULTS_BADMEAS)) {
        faults_ += " meas";
    }
    if (CheckBitsAll(sensor.faults, UBX_ESF_STATUS_V2_FAULTS_BADTTAG)) {
        faults_ += " ttag";
    }
    if (CheckBitsAll(sensor.faults, UBX_ESF_STATUS_V2_FAULTS_MISSINGMEAS)) {
        faults_ += " missing";
    }
    if (CheckBitsAll(sensor.faults, UBX_ESF_STATUS_V2_FAULTS_NOISYMEAS)) {
        faults_ += " noisy";
    }
    faults_ = faults_.substr(1);
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
