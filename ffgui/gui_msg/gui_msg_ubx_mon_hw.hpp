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

#ifndef __GUI_MSG_UBX_MON_HW_HPP__
#define __GUI_MSG_UBX_MON_HW_HPP__

//
#include "ffgui_inc.hpp"
//
#include <fpsdk_common/parser/ubx.hpp>
using namespace fpsdk::common::parser::ubx;
//
#include "gui_msg.hpp"
#include "gui_widget_table.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsgUbxMonHw : public GuiMsg
{
   public:
    GuiMsgUbxMonHw(const std::string& viewName, const InputPtr& input);

    void Update(const DataMsgPtr& msg) final;
    void Update(const Time& now) final;
    void Clear() final;

    void DrawToolbar() final;
    void DrawMsg() final;

    // As seen in u-center... (by feeding it fake UBX-MON-HW messages...)
    static constexpr std::array<const char*, 71> VIRT_FUNCS =  // clang-format off
    {{
        "UART1_RX", "UART1_TX", "DDC_PER_SCL", "DDC_PER_SDA", "DDC_MEM_SCL", "DDC_MEM_SDA", "SPI_MOSI", "SPI_MISO", // 0-7
        "SPI_SCK", "SPI_SS", "DSPI2_MOSI", "DSPI2_MISO", nullptr, nullptr, "DSPI2_SCK", "DSPI2_CS", // 8-15
        "TIMEPULSE1", "TIMEPULSE2", "EXTINT0", "EXTINT1", "UART2_RX", "UART2_TX", nullptr, "USER00", "USER01", // 16-24
        "USER02", "USER03", "USER04", "USER05", "USER06", "USER08", "USER09", "USER10", "USER11", "USER12", // 25-34
        "USER13", "USER14", "USER15", "USER16", "USER17", "USER18", "USER19", "USER20", "USER21", "USER22", // 35-44
        "USER23", "USER24", "USER25", "USER26", "USER27", "USER28", "ANT_DETECT", "ANT_SWITCH_N", "ANT_SHORT_N", // 45-53
        "TXREADY", "CONF0", "CONF1", "CONF2", "CONF3", nullptr, nullptr, "D_SEL", "SWI2C_SCL", "SWI2C_SDA", // 54-63
        "SINGLE_WT", "DRIVE_DIR", "GEOFENCE", nullptr, nullptr, nullptr, "NAV_STATUS" // 64-70
    }};  // clang-format on

   private:
    GuiWidgetTable table_;
    bool valid_ = false;
    UBX_MON_HW_V0_GROUP0 hw_;

    static constexpr std::array<const char*, 17> PIN_NAMES =  // clang-format off
    {{
        " 0", " 1", " 2", " 3", " 4", " 5", " 6", " 7", " 8", " 9", "10", "11", "12", "13", "14", "15", "16"
    }};  // clang-format on

    static constexpr std::array<const char*, NumOf(PIN_NAMES)> PIO_NAMES =  // clang-format off
    {{
        "PIO00", "PIO01", "PIO02", "PIO03", "PIO04", "PIO05", "PIO06", "PIO07", "PIO08", "PIO09", "PIO10", "PIO11",
        "PIO12", "PIO13", "PIO14", "PIO15", "PIO16"
    }};  // clang-format on

    // As seen in u-center... (see above)
    static constexpr std::array<const char*, NumOf(PIN_NAMES)> PERIPHA_FUNCS =  // clang-format off
    {{
        "N/A", "UART_H_RX", "UART_H_TX", "I2C_SDA", "I2C_SCK", "TIMEPULSE1", "TIMEPULSE2", "N/A",
        "DIGIO_CS", "UART_S_RX", "UART_S_TX", "SPI_M_CLK", "SPI_M_MOSI", "SPI_M_MISO", "SPI_M_CS", "DIGIO_CLK",
        "DIGIO_CS"
    }};  // clang-format on
    static constexpr std::array<const char*, NumOf(PIN_NAMES)> PERIPHB_FUNCS =  // clang-format off
    {{
        "PWM", "SPI_D_MOSI", "SPI_D_MISO", "SPI_D_CS", "SPI_D_CLK", "I2C_S_CLK", "UART_H_TX", "UART_H_RX",
        "I2C_S_SDA", "I2C_S_SCL", "I2C_S_SDA", "I2C_S_SCL", "I2C_S_SDA", "N/A", "UART_S_TX", "UART_S_RX",
        "TIMEPULSE3"
    }};                                                                         // clang-format on

    static constexpr std::array<StatusFlag, 2> RTC_FLAGS =  // clang-format off
    {{
        { true,      "calibrated",         CIX_TEXT_OK      },
        { false,     "uncalibrated",       CIX_TEXT_WARNING },
    }};  // clang-format on

    static constexpr std::array<StatusFlag, 2> XTAL_FLAGS =  // clang-format off
    {{
        { true,      "absent",             CIX_TEXT_OK },
        { false,     "ok",                 CIX_AUTO    },
    }};  // clang-format on

    static constexpr std::array<StatusFlag, 5> ASTATUS_FLAGS =  // clang-format off
    {{
        { UBX_MON_HW_V0_ASTATUS_INIT,     "init",           CIX_AUTO         },
        { UBX_MON_HW_V0_ASTATUS_UNKNOWN,  "unknown",        CIX_AUTO         },
        { UBX_MON_HW_V0_ASTATUS_OK,       "OK",             CIX_TEXT_OK      },
        { UBX_MON_HW_V0_ASTATUS_SHORT,    "short",          CIX_TEXT_WARNING },
        { UBX_MON_HW_V0_ASTATUS_OPEN,     "open",           CIX_TEXT_WARNING },
    }};  // clang-format on

    static constexpr std::array<StatusFlag, 3> APOWER_FLAGS =  // clang-format off
    {{
        { UBX_MON_HW_V0_APOWER_OFF,       "off",            CIX_AUTO    },
        { UBX_MON_HW_V0_APOWER_ON,        "on",             CIX_TEXT_OK },
        { UBX_MON_HW_V0_APOWER_UNKNOWN,   "unknown",        CIX_AUTO    },
    }};  // clang-format on

    static constexpr std::array<StatusFlag, 2> SAFEBOOT_FLAGS =  // clang-format off
    {{
        { true,                           "active",         CIX_TEXT_WARNING },
        { false,                          "inactive",       CIX_AUTO         },

    }};  // clang-format on

    static constexpr std::array<StatusFlag, 4> JAMMING_FLAGS =  // clang-format off
    {{
        { UBX_MON_HW_V0_FLAGS_JAMMINGSTATE_UNKNOWN,   "unknown",   CIX_AUTO         },
        { UBX_MON_HW_V0_FLAGS_JAMMINGSTATE_OK,        "ok",        CIX_TEXT_OK      },
        { UBX_MON_HW_V0_FLAGS_JAMMINGSTATE_WARNING,   "warning",   CIX_TEXT_WARNING },
        { UBX_MON_HW_V0_FLAGS_JAMMINGSTATE_CRITICAL,  "critical",  CIX_TEXT_ERROR   },
    }};  // clang-format on
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_UBX_MON_HW_HPP__
