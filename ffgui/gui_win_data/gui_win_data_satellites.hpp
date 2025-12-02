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

#ifndef __GUI_WIN_DATA_SATELLITES_HPP__
#define __GUI_WIN_DATA_SATELLITES_HPP__

//
#include "ffgui_inc.hpp"
//
#include "gui_widget_tabbar.hpp"
#include "gui_widget_table.hpp"
#include "gui_win_data.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinDataSatellites : public GuiWinData
{
   public:
    GuiWinDataSatellites(const std::string& name, const InputPtr& input);
    ~GuiWinDataSatellites();

   private:
    void _ProcessData(const DataPtr& data) final;
    void _Loop(const Time& now) final;
    void _ClearData() final;

    void _DrawToolbar() final;
    void _DrawContent() final;

    static void _DrawSignalLevelCb(const void* arg);

    // Config
    struct Config
    {
        bool sizingMax = false;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, sizingMax)
    Config cfg_;

    GuiWidgetTabbar tabbarMode_;
    GuiWidgetTabbar tabbarFilter_;
    GuiWidgetTable table_;
    DataEpochPtr epoch_;
    bool newEpochAvail_ = false;

    bool doList_ = false;
    bool doSky_ = false;

    void _DrawSky();
    void _UpdateSatellites();

    struct Count
    {
        int num_ = 0;
        int used_ = 0;
        int visible_ = 0;
        std::string labelList_;
        std::string labelSky_;
    };

    // clang-format off
    static constexpr struct { Gnss gnss; int ix; const char* label; } FILTERS[] = {
        { Gnss::UNKNOWN, 0, "All"   },
        { Gnss::GPS,     1, "GPS"   },
        { Gnss::SBAS,    2, "SBAS"  },
        { Gnss::GAL,     3, "GAL"   },
        { Gnss::BDS,     4, "BDS"   },
        { Gnss::QZSS,    5, "QZSS"  },
        { Gnss::GLO,     6, "GLO"   },
        { Gnss::NAVIC,   7, "NAVIC" },
    };  // clang-format on
    static constexpr std::size_t NUM_FILTERS = NumOf(FILTERS);
    std::array<Count, NUM_FILTERS> counts_;
    int filterIx_ = 0;  // config saved by imgui via selected tab

    struct SkySat
    {
        const SatInfo* satInfo_ = nullptr;
        const SigInfo* sigL1_ = nullptr;
        const SigInfo* sigE6_ = nullptr;
        const SigInfo* sigL2_ = nullptr;
        const SigInfo* sigL5_ = nullptr;
        Vec2f xy_;
    };
    std::vector<SkySat> sats_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_DATA_SATELLITES_HPP__
