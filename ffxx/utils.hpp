/**
 * \verbatim
 * flipflip's c++ library (ffxx)
 *
 * Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
 * https://oinkzwurgl.org/projaeggd/ffxx/
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 * \endverbatim
 *
 * @file
 * @brief Utilities
 *
 * @page FFXX_UTILS Utilities
 *
 */
#ifndef __FFXX_UTILS_HPP__
#define __FFXX_UTILS_HPP__

/* LIBC/STL */

/* EXTERNAL */
#include <fpsdk_common/parser/types.hpp>

/* PACKAGE */

namespace ffxx {
/* ****************************************************************************************************************** */

/**
 * @brief Get version string
 *
 * @returns the version string (e.g. "0.0.0-heads/feature/something-0-gf61ae50-dirty")
 */
const char* GetVersionString();

/**
 * @brief Get copyright string
 *
 * @returns the copyright string
 */
const char* GetCopyrightString();

/**
 * @brief Get license string
 *
 * @returns the license string
 */
const char* GetLicenseString();

/**
 * @brief Get a HTTP User-Agent string
 *
 * @returns a string suitable as the value for the HTTP User-Agent header
 */
std::string GetUserAgentStr();

// ---------------------------------------------------------------------------------------------------------------------

enum class CommonMessage
{
    UBX_MON_VER,
    FP_A_VERSION,
    FP_B_VERSION,

    // u-blox receiver resets
    UBX_RESET_HOT,           // Hotstart (like u-center)                             (UBX-CFG-RST)
    UBX_RESET_WARM,          // Warmstart (like u-center)                            (UBX-CFG-RST)
    UBX_RESET_COLD,          // Coldstart (like u-center)                            (UBX-CFG-RST)
    UBX_RESET_SOFT,          // Controlled software reset                            (UBX-CFG-RST)
    UBX_RESET_HARD,          // Controlled hardware reset                            (UBX-CFG-RST)
    UBX_RESET_GNSS_STOP,     // Stop GNSS (stop navigation)                          (UBX-CFG-RST)
    UBX_RESET_GNSS_START,    // Start GNSS (start navigation)                        (UBX-CFG-RST)
    UBX_RESET_GNSS_RESTART,  // Restart GNSS (restart navigation)                    (UBX-CFG-RST)
    UBX_RESET_DEFAULT_1,     // Revert config to default, keep nav data          1/2 (UBX-CFG-CFG)
    UBX_RESET_DEFAULT_2,     //                 "                                2/2 (UBX-CFG-RST)
    UBX_RESET_FACTORY_1,     // Revert config to default and coldstart           1/2 (UBX-CFG-CFG)
    UBX_RESET_FACTORY_2,     //                 "                                2/2 (UBX-CFG-RST)
    UBX_RESET_SAFEBOOT,      // Safeboot mode                                        (UBX-CFG-RST)
    UBX_TRAINING,            // Training sequence

    // Quectel commands
    QUECTEL_LC29H_HOT,
    QUECTEL_LC29H_WARM,
    QUECTEL_LC29H_COLD,
    QUECTEL_LC29H_REBOOT,
    QUECTEL_LC29H_VERNO,
    QUECTEL_LG290P_HOT,
    QUECTEL_LG290P_WARM,
    QUECTEL_LG290P_COLD,
    QUECTEL_LG290P_REBOOT,
    QUECTEL_LG290P_VERNO,
};

const fpsdk::common::parser::ParserMsg& GetCommonMessage(CommonMessage which);

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_UTILS_HPP__
