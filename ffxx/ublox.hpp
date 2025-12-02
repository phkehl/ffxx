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
 * @brief u-blox receiver stuff
 *
 * @page FFXX_UBLOX u-blox receiver stuff
 *
 */
#ifndef __FFXX_UBLOX_HPP__
#define __FFXX_UBLOX_HPP__

/* LIBC/STL */
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/* EXTERNAL */
#include <ubloxcfg/ubloxcfg.h>
#include <fpsdk_common/types.hpp>
#include <fpsdk_common/parser/types.hpp>

/* PACKAGE */

namespace ffxx {
/* ****************************************************************************************************************** */

class UbloxCfgDb
{
   public:
    UbloxCfgDb(const std::string& name);
    ~UbloxCfgDb();

    static constexpr std::array<UBLOXCFG_LAYER_t, 4> LAYERS = { { UBLOXCFG_LAYER_DEFAULT, UBLOXCFG_LAYER_FLASH,
        UBLOXCFG_LAYER_BBR, UBLOXCFG_LAYER_RAM } };

    struct Value
    {  // clang-format off
        bool             valid_ = false;        //!< Data is valid
        UBLOXCFG_VALUE_t val_ = { ._raw = 0 };  //!< The value
        std::string      str_;                  //!< The value as a string
        inline bool operator==(const Value& other) const { return (valid_ == other.valid_) && (val_._raw == other.val_._raw); }
        inline bool operator!=(const Value& other) const { return !(*this == other); }
    };  // clang-format on

    struct Item
    {  // clang-format off
        Item() = default;
        Item(const uint32_t id);
        uint32_t               id_ = 0;            //!< 0x00000000
        std::string            name_;              //!< CFG-FOO-BAR
        std::string            group_;             //!< CFG-FOO
        uint32_t               groupId_ = 0;       //!< 0x00xx0000
        UBLOXCFG_SIZE_t        size_;              //!< Value (storage) size
        UBLOXCFG_TYPE_t        type_;              //!< Value (storage) type
        std::string            typeStr_;           //!< Type as string ("U1", "E2", "I4", "X8", etc.)
        const UBLOXCFG_ITEM_t* def_ = nullptr;     //!< Pointer to definition, if known
        std::string            title_;             //!< Title (short description)
        std::string            unit_;              //!< Unit ("s", "m", "m/s", etc.) or empty
        std::string            scale_;             //!< Scale ("0.001", "1e3", etc.) or empty
        Value valRam_;
        Value valBbr_;
        Value valFlash_;
        Value valDefault_;
        void SetValue(const UBLOXCFG_LAYER_t layer, const UBLOXCFG_VALUE_t& val);
        //inline bool operator<(const Item& other) const { return id_ < other.id_; }
        inline bool operator==(const Item& other) const { return id_ == other.id_; }
        inline bool operator!=(const Item& other) const { return !(*this == other); }
    };  // clang-format on

    using Items = std::vector<Item>;

    void Clear();
    Items& GetItems();

    void StartValGetResp();
    std::size_t AddValGetResp(const fpsdk::common::parser::ParserMsg& msg);
    void EndValGetResp();

   private:
    std::string name_;
    Items items_;
};

// ---------------------------------------------------------------------------------------------------------------------


std::optional<fpsdk::common::parser::ParserMsg> MakeUbxCfgValgetAll(const UBLOXCFG_LAYER_t layer, const std::size_t offs);
std::vector<fpsdk::common::parser::ParserMsg> MakeUbxCfgValdel(const std::vector<UBLOXCFG_LAYER_t>& layers, const std::vector<uint32_t>& ids);
std::vector<fpsdk::common::parser::ParserMsg> MakeUbxCfgValset(const std::vector<UBLOXCFG_LAYER_t>& layers, const std::vector<UBLOXCFG_KEYVAL_t>& kvs);
std::optional<fpsdk::common::parser::ParserMsg> MakeUbxParserMsg(const uint8_t clsId, const uint8_t msgId, const std::vector<uint8_t>& payload, const bool makeInfo = true);

/* ****************************************************************************************************************** */
}  // namespace ffxx
#endif  // __FFXX_UBLOX_HPP__
