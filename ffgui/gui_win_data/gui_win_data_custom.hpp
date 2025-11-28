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

#ifndef __GUI_WIN_DATA_CUSTOM_HPP__
#define __GUI_WIN_DATA_CUSTOM_HPP__

//
#include "ffgui_inc.hpp"
//
#include <map>
//
#include <fpsdk_common/parser.hpp>
using namespace fpsdk::common::parser;
//
#include "gui_widget_tabbar.hpp"
#include "gui_widget_hexdump.hpp"
#include "gui_win_data.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinDataCustom : public GuiWinData
{
   public:
    GuiWinDataCustom(const std::string& name, const InputPtr& input);
    ~GuiWinDataCustom();

   private:
    void _ProcessData(const DataPtr& data) final;
    void _Loop(const Time& now) final;
    void _ClearData() final;

    void _DrawToolbar() final;
    void _DrawContent() final;

    // Config
    struct ConfigHex
    {
        std::string note;
        std::string payload;

        inline bool operator<(const ConfigHex& rhs) const { return note < rhs.note; }
        inline bool operator==(const ConfigHex& other) const
        {
            return (note == other.note) && (payload == other.payload);
        }
        inline bool operator!=(const ConfigHex& other) const
        {
            return !(*this == other);
        }
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ConfigHex, note, payload)
    struct ConfigText
    {
        std::string note;
        std::string payload;
        bool crlf = false;

        inline bool operator<(const ConfigText& rhs) const { return note < rhs.note; }
        inline bool operator==(const ConfigText& other) const
        {
            return (note == other.note) && (payload == other.payload) && (crlf == other.crlf);
        }
        inline bool operator!=(const ConfigText& other) const
        {
            return !(*this == other);
        }
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ConfigText, note, payload, crlf)
    struct ConfigNmea
    {
        std::string note;
        std::string talker;
        std::string formatter;
        std::string checksum;
        std::string payload;
        int talkerIx = -1;
        int formatterIx = -1;
        bool ckUser = false;

        inline bool operator<(const ConfigNmea& rhs) const { return note < rhs.note; }
        inline bool operator==(const ConfigNmea& other) const
        {
            return (note == other.note) && (talker == other.talker) && (formatter == other.formatter) &&
                   (checksum == other.checksum) && (payload == other.payload) && (ckUser == other.ckUser);
        }
        inline bool operator!=(const ConfigNmea& other) const
        {
            return !(*this == other);
        }
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        ConfigNmea, note, talker, formatter, checksum, payload, ckUser)
    struct ConfigUbx
    {
        std::string note;
        uint8_t clsId = 0;
        uint8_t msgId = 0;
        uint16_t size = 0;
        uint16_t checksum = 0;
        std::string payload;
        std::string message;
        bool ckUser = false;
        bool sizeUser = false;

        inline bool operator<(const ConfigUbx& rhs) const { return note < rhs.note; }
        inline bool operator==(const ConfigUbx& other) const
        {
            return (note == other.note) && (clsId == other.clsId) && (msgId == other.msgId) &&
                   (size == other.size) && (checksum == other.checksum) && (payload == other.payload) &&
                   (sizeUser == other.sizeUser) && (ckUser == other.ckUser);
        }
        inline bool operator!=(const ConfigUbx& other) const
        {
            return !(*this == other);
        }
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        ConfigUbx, note, message, clsId, msgId, size, checksum, payload, ckUser, sizeUser, ckUser)

    struct Config
    {
        ConfigHex hex;
        ConfigText text;
        ConfigNmea nmea;
        ConfigUbx ubx;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, hex, text, nmea, ubx)

    struct SavedConfig
    {
        std::vector<ConfigHex> hex;
        std::vector<ConfigText> text;
        std::vector<ConfigNmea> nmea;
        std::vector<ConfigUbx> ubx;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SavedConfig, hex, text, nmea, ubx)

    Config cfg_;
    static SavedConfig savedCfg_;
    GuiWidgetTabbar tabbar_;
    GuiWidgetHexdump hexdump_;

    // Hex
    bool rawHexOk_ = true;

    // Nmea
    bool nmeaCkOk_ = true;
    bool nmeaPayloadOk_ = true;

    // UBX
    bool ubxCkOk_ = true;
    bool ubxSizeOk_ = true;
    bool ubxPayloadOk_ = true;

    // _SetData()
    Parser parser_;
    ParserMsg dataMsg_;

    bool doHex_ = false;
    bool doText_ = false;
    bool doUbx_ = false;
    bool doNmea_ = false;
    bool dirty_ = false;

    void _DrawHex();
    void _DrawText();
    void _DrawUbx();
    void _DrawNmea();

    void _SetData(const std::vector<uint8_t>& data);
    std::vector<uint8_t> _HexToBin(const std::string& hex) const;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_DATA_CUSTOM_HPP__
