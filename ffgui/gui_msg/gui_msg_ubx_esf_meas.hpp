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

#ifndef __GUI_MSG_USB_ESF_MEAS_HPP__
#define __GUI_MSG_USB_ESF_MEAS_HPP__

#include <deque>
#include <memory>
#include "ffgui_inc.hpp"
#include "gui_msg.hpp"
#include "gui_widget_table.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsgUbxEsfMeas : public GuiMsg
{
   public:
    GuiMsgUbxEsfMeas(const std::string& viewName, const InputPtr& input);

    void Update(const DataMsgPtr& msg) final;
    void Update(const Time& now) final;
    void Clear() final;

    void DrawToolbar() final;
    void DrawMsg() final;

    struct MeasDef
    {
        const char* name_;
        enum Type_e
        {
            UNSIGNED,
            SIGNED,
            TICK
        };
        enum Type_e type_;
        const char* fmt_;
        double scale_;
        const char* unit_;
    };

    // Sources:
    // - u-center, see tools/make_fake_ubxesfmeas.c and tools/make_fake_ubxesfstatus.c
    // - ZED-F9R integration manual (UBX-20039643)
    static constexpr std::array<MeasDef, 19> MEAS_DEFS = // clang-format off
    {{
        /*  0 */ { "No data",                MeasDef::UNSIGNED, "", 0.0,  "" },
        /*  1 */ { "Front wheel angle",      MeasDef::SIGNED,   "%+.3f",  1e-3, "deg" },
        /*  2 */ { "Rear Wheel angle",       MeasDef::SIGNED,   "%+.3f",  1e-3, "deg" },
        /*  3 */ { "Pitch",                  MeasDef::SIGNED,   "%+.3f",  1e-3, "deg" },
        /*  4 */ { "Steering wheel angle",   MeasDef::SIGNED,   "%+.3f",  1e-3, "deg" },
        /*  5 */ { "Gyroscope Z",            MeasDef::SIGNED,   "%+.4f",  0x1p-12 /*perl -e ' printf "%a", 2**-12' */, "deg/s" },
        /*  6 */ { "Wheel tick FL",          MeasDef::TICK,     "%.0f",   1.0,  "ticks" },
        /*  7 */ { "Wheel tick FR",          MeasDef::TICK,     "%.0f",   1.0,  "ticks" },
        /*  8 */ { "Wheel tick RL",          MeasDef::TICK,     "%.0f",   1.0,  "ticks" },
        /*  9 */ { "Wheel tick RR",          MeasDef::TICK,     "%.0f",   1.0,  "ticks" },
        /* 10 */ { "Single tick",            MeasDef::TICK,     "%.0f",   1.0,  "ticks" },
        /* 11 */ { "Speed",                  MeasDef::SIGNED,   "%+.3f",  1e-3, "m/s" },
        /* 12 */ { "Gyroscope temp.",        MeasDef::SIGNED,   "%.2f",   1e-2, "C" },
        /* 13 */ { "Gyroscope Y",            MeasDef::SIGNED,   "%+.4f",  0x1p-12 /*perl -e ' printf "%a", 2**-12' */, "deg/s" },
        /* 14 */ { "Gyroscope X",            MeasDef::SIGNED,   "%+.4f",  0x1p-12 /*perl -e ' printf "%a", 2**-12' */, "deg/s" },
        /* 15 */ { nullptr,                  MeasDef::UNSIGNED, "",       0.0, nullptr },
        /* 16 */ { "Accelerometer X",        MeasDef::SIGNED,   "%+8.4f", 0x1p-10 /*perl -e ' printf "%a", 2**-10' */, "m/s^2" },
        /* 17 */ { "Accelerometer Y",        MeasDef::SIGNED,   "%+8.4f", 0x1p-10 /*perl -e ' printf "%a", 2**-10' */, "m/s^2" },
        /* 18 */ { "Accelerometer Z",        MeasDef::SIGNED,   "%+8.4f", 0x1p-10 /*perl -e ' printf "%a", 2**-10' */, "m/s^2" },
    }}; // clang-format on

   private:
    GuiWidgetTable table_;

    static constexpr int MAX_PLOT_DATA = 1000;

    struct MeasInfo
    {
        Time lastTime_;
        uint32_t nMeas_ = 0;
        int type_;
        std::string name_;
        std::string rawHex_;
        std::string value_;
        std::string ttagSens_;
        std::string ttagRx_;
        // std::string source_;
        std::string provider_;

        std::deque<double> plotData_;
    };

    std::map<uint32_t, MeasInfo> measInfos_;
    bool resetPlotRange_ = true;
    bool autoPlotRange_ = true;
    Time now_;
    bool dirty_ = false;

};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_USB_ESF_MEAS_HPP__
