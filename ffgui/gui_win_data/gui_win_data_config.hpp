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

#ifndef __GUI_WIN_DATA_CONFIG_HPP__
#define __GUI_WIN_DATA_CONFIG_HPP__

//
#include "ffgui_inc.hpp"
//
#include <set>
//
#include <ubloxcfg/ubloxcfg.h>
#include <ffxx/ublox.hpp>
//
#include "gui_widget_filter.hpp"
#include "gui_win_data.hpp"
#include "gui_widget_tabbar.hpp"
#include "gui_widget_log.hpp"
// #include "gui_win_filedialog.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinDataConfig : public GuiWinData
{
   public:
    GuiWinDataConfig(const std::string& name, const InputPtr& input);
    ~GuiWinDataConfig();

    private:
    void _ProcessData(const DataPtr& data) final;
    void _Loop(const Time& now) final;
    void _ClearData() final;

    void _DrawToolbar() final;
    void _DrawContent() final;
    void _DrawItems();
    void _DrawEditorTabs();
    void _DrawBatchActionsPopup();

    struct Config
    {
        HistoryEntries history;
        std::set<std::string> selectedItems;
        bool showHidden = false;
        bool showSelected = false;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, history, selectedItems, showHidden, showSelected)
    Config cfg_;
    GuiWidgetTabbar tabbarMain_;
    GuiWidgetTabbar tabbarEdit_;
    GuiWidgetLog log_;
    GuiWidgetFilter filter_;
    UbloxCfgDb db_;

    enum class State {
        IDLE,
        POLL,
        SAVE,
    };
    State state_ = State::IDLE;
    void _SetState(const State state);

    struct Poll
    {
        enum class Step { POLL, WAIT, DONE, FAIL };
        Step step_ = Step::POLL;
        std::size_t layerIx_ = 0;
        std::size_t offs_ = 0;
        Time to_;
    };
    Poll poll_;
    void _PollStart();
    void _PollDone(const bool success);
    void _PollLoop(const Time& now);
    void _PollData(const DataMsg& msg);

    struct Save
    {
        enum class Step { SEND, WAIT, DONE, FAIL };
        Step step_ = Step::SEND;
        Time to_;
        std::size_t ix_ = 0;
        std::vector<ParserMsg> msgs_;
        uint8_t msgId_ = 0;
    };
    Save save_;
    void _SaveStart();
    void _SaveDone(const bool success);
    void _SaveLoop(const Time& now);
    void _SaveData(const DataMsg& msg);

    bool _IsAckCfg(const ParserMsg& msg, const uint8_t msgId) const;
    bool _IsNakCfg(const ParserMsg& msg, const uint8_t msgId) const;

    struct Item
    {
        Item() = delete;
        Item(const UbloxCfgDb::Item& dbItem);

        UbloxCfgDb::Item db_;
        bool dbValid_ = false;

        void UpdateDb(const UbloxCfgDb::Item& dbItem);
        void Sync();

        std::string search_;
        bool show_ = true;
        bool selected_ = false;
        bool filterMatch_ = true;

        enum ValueIx { DEF = 0, FLASH, BBR, RAM }; // must correspond to UbloxCfgDb::LAYERS
        static constexpr std::array<const char*, 4> LAYER_NAMES = {{ "Default", "Flash", "BBR", "RAM" }};
        std::array<UbloxCfgDb::Value, 4> currValues_;
        std::array<UbloxCfgDb::Value, 4> editValues_;

        std::array<bool, 4> valuesEdited_ = { { false, false, false, false } };
        std::size_t numEdited_ = 0;
        std::array<bool, 4> currNonDefault_ = { { false, false, false, false } };
        std::array<bool, 4> editNonDefault_ = { { false, false, false, false } };

        std::string debug_;
    };
    std::vector<Item> itemsAll_;
    std::vector<Item*> itemsAvail_;
    std::vector<Item*> itemsDisp_;
    std::vector<Item*> itemsEdit_;
    std::size_t itemsPending_ = 0;
    bool itemsDirty_ = false;

    struct View {
        const char* title_;
        const char* filter_;
        const char* filter2_;
        Item::ValueIx valIx_;
    };
    static constexpr std::array<View, 13> VIEWS = {{ // clang-format off
        { "All configuration",    "", nullptr, Item::DEF },
        { "GNSS and signals",     "CFG-SIGNAL-,CFG-BDS-,CFG-QZSS-,CFG-SBAS-PRN,CFG-GRP32-IGNORE_GPS_L5_HEALTH", nullptr, Item::DEF },
        { "Navigation engine",    "CFG-RATE-,CFG-NAV2-,CFG-NAVHPG-,CFG-NAVSPG-,CFG-RTCM-,CFG-SBAS-USE", nullptr, Item::DEF },
        { "Antenna supervisor",   "CFG-HW-ANT", nullptr, Item::DEF },
        { "Timepulse config",     "CFG-TP-", nullptr, Item::DEF },
        { "I/O ports",            "CFG-UART1-,CFG-UART1IN,CFG-UART1OUT,"
                                  "CFG-UART2-,CFG-UART2IN,CFG-UAR21OUT,"
                                  "CFG-SPI-,CFG-SPIIN,CFG-SPIOUT,"
                                  "CFG-I2C-,CFG-I2CIN,CFG-I2COUT,"
                                  "CFG-USB-,CFG-USBIN,CFG-USBOUT"
                                  "CFG-TXREADY-", nullptr, Item::DEF },
        { "Output UART1",         "CFG-MSGOUT-,CFG-INFMSG-", "_UART1", Item::DEF },
        { "Output UART2",         "CFG-MSGOUT-,CFG-INFMSG-", "_UART2", Item::DEF },
        { "Output SPI",           "CFG-MSGOUT-,CFG-INFMSG-", "_SPI", Item::DEF },
        { "Output USB",           "CFG-MSGOUT-,CFG-INFMSG-", "_USB", Item::DEF },
        { "Output I2C",           "CFG-MSGOUT-,CFG-INFMSG-", "_I2C", Item::DEF },
        { "Items in Flash layer", "", nullptr, Item::FLASH },
        { "Items in BBR layer", "", nullptr, Item::BBR },
    }}; // clang-format on
    std::size_t viewIx_ = 0;
    void _SetView(const std::size_t viewIx);

    void _DrawHeaderPopup(const Item::ValueIx valIx);
    void _DrawItemEditor(Item& item);
    bool _DrawValueEditor(UbloxCfgDb::Value& dbValue, const UbloxCfgDb::Item& item);
    void _DrawItemPopup(Item& item);

    void _Sync();

    static constexpr std::size_t COL_WIDTH_ITEM = 35;
    static constexpr std::size_t COL_WIDTH_ID = 10;
    static constexpr std::size_t COL_WIDTH_TYPE = 2;
    static constexpr std::size_t COL_WIDTH_UNIT = 7;
    static constexpr std::size_t COL_WIDTH_SCALE = 12;
    static constexpr std::size_t COL_WIDTH_VALUE = 25;
    static constexpr std::size_t COL_WIDTH_DESC = 50;

    static constexpr const char* HELP_TEXT =
        "- Click row to select item for editing\n"
        "- CTRL-click to select and edit multiple items\n"
        "- Right click on Default/Flash/BBR/RAM table heading for layer actions\n"
        "- Right click on rows for item actions\n";

};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_DATA_CONFIG_HPP__
