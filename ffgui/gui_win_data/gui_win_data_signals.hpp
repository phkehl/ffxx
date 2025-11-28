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

#ifndef __GUI_WIN_DATA_SIGNALS_HPP__
#define __GUI_WIN_DATA_SIGNALS_HPP__

//
#include "ffgui_inc.hpp"
//
#include "gui_widget_tabbar.hpp"
#include "gui_widget_table.hpp"
#include "gui_win_data.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinDataSignals : public GuiWinData
{
   public:
    GuiWinDataSignals(const std::string& name, const InputPtr& input);
    ~GuiWinDataSignals();

    static void DrawSignalLevelBar(const SigInfo& sig);

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
        bool onlyTracked = true;
        bool sizingMax = false;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, onlyTracked, sizingMax)
    Config cfg_;

    GuiWidgetTabbar tabbarMode_;
    GuiWidgetTabbar tabbarFilter_;
    GuiWidgetTable table_;
    DataEpochPtr epoch_;
    bool newEpochAvail_ = false;

    bool doSky_ = false;
    bool doList_ = false;
    void _DrawSky();
    void _UpdateSignals();

    struct Count
    {
        int tot_ = 0;
        int trk_ = 0;
        int nav_ = 0;
        std::string label_;
    };

    // clang-format off
    static constexpr struct { Gnss gnss; Band band; int ix; const char* label; } FILTERS[] = {
        { Gnss::UNKNOWN, Band::UNKNOWN,  0, "All"   },
        { Gnss::GPS,     Band::UNKNOWN,  1, "GPS"   },
        { Gnss::SBAS,    Band::UNKNOWN,  2, "SBAS"  },
        { Gnss::GAL,     Band::UNKNOWN,  3, "GAL"   },
        { Gnss::BDS,     Band::UNKNOWN,  4, "BDS"   },
        { Gnss::QZSS,    Band::UNKNOWN,  5, "QZSS"  },
        { Gnss::GLO,     Band::UNKNOWN,  6, "GLO"   },
        { Gnss::NAVIC,   Band::UNKNOWN,  7, "NAVIC" },
        { Gnss::UNKNOWN, Band::L1,       8, "L1"    },
        { Gnss::UNKNOWN, Band::E6,       9, "E6"    },
        { Gnss::UNKNOWN, Band::L2,      10, "L2"    },
        { Gnss::UNKNOWN, Band::L5,      11, "L5"    },
    };  // clang-format on
    static constexpr std::size_t NUM_FILTERS = NumOf(FILTERS);
    std::array<Count, NUM_FILTERS> counts_;
    int filterIx_ = 0;  // config saved by imgui via selected tab

    struct CnoBin
    {
        void Add(const SigInfo& sig);
        const std::string& Tooltip(const int ix);

        // CNo statistics
        std::array<int, NUM_FILTERS> count_;
        std::array<float, NUM_FILTERS> min_;
        std::array<float, NUM_FILTERS> max_;
        std::array<float, NUM_FILTERS> mean_;

        // Bin region in polar coordinates
        float r0_;  // Min radius [unit] (0..1)
        float r1_;  // Max radius [unit] (0..1)
        float a0_;  // Min angle [rad] (0..2pi)
        float a1_;  // Max angle [rad] (0..2pi)
        Vec2f xy_;  // Position of bin centre [unit, unit] (0..1, 0..1)
        // std::string uid_;
        std::array<std::string, NUM_FILTERS> tooltips_;
    };

    // Number of divisions in azimuth
    static constexpr std::size_t SKY_NUM_BINS_AZ = 24;  // 360 / 24 = 15 deg
    // Number of divisions in elevation
    static constexpr std::size_t SKY_NUM_BINS_EL = 9;  // 90 / 9 = 10 deg
    // Size ("width") of bin in azimuth [deg]
    static constexpr float SKY_AZ_WIDTH = 360.0f / (float)SKY_NUM_BINS_AZ; // 15
    // Size ("width") of bin in elevation [deg]
    static constexpr float SKY_EL_WIDTH = 90.0f / (float)SKY_NUM_BINS_EL; // 10
    // Number of bins
    static constexpr std::size_t SKY_NUM_BINS = SKY_NUM_BINS_AZ * SKY_NUM_BINS_EL;

    // Determine the bin this should go into
    static constexpr std::size_t AZEL_TO_BIN(const int az, const int el)
    {
        const int elevIx = el / SKY_EL_WIDTH;                  // "circle"
        const int azimIx = ((az + 270) % 360) / SKY_AZ_WIDTH;  // "sector"
        return (elevIx * SKY_NUM_BINS_AZ) + azimIx;
    };

    struct CnoSky
    {
        CnoSky();
        void Clear();
        std::array<CnoBin, SKY_NUM_BINS> bins_;    // Data
        std::array<float, SKY_NUM_BINS_EL> rs_;    // Precalculated radii for grid
        std::array<Vec2f, SKY_NUM_BINS_AZ> axys_;  // Precalculated cos() and sin() of angles for grid
    };

    CnoSky cnoSky_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_DATA_SIGNALS_HPP__
