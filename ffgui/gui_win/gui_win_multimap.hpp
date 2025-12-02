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

#ifndef __GUI_WIN_MULTIMAP_HPP__
#define __GUI_WIN_MULTIMAP_HPP__

//
#include "ffgui_inc.hpp"
//
#include "gui_widget_map.hpp"
#include "gui_widget_tabbar.hpp"
#include "database.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinMultimap : public GuiWin
{
   public:
    GuiWinMultimap(const std::string& name);
    ~GuiWinMultimap();

    void Loop(const Time& now) final;
    void DrawWindow() final;

   private:
    static constexpr std::array<std::size_t, 7> COLOURS = { { CIX_AUTO, CIX_C_BRIGHTRED, CIX_C_BRIGHTGREEN,
        CIX_C_BRIGHTBLUE, CIX_C_BRIGHTCYAN, CIX_C_BRIGHTMAGENTA, CIX_C_BRIGHTORANGE } };

    struct ConfigInput {
        bool used = false;
        int colour = 0;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ConfigInput, used, colour)

    struct Config
    {
        std::map<std::string, ConfigInput> inputs;
        int numPoints = 10000;
        bool baseline = true;
    };

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, inputs)
    Config cfg_;

    GuiWidgetTabbar tabbar_;
    GuiWidgetMap map_;

    void _Clear();

    struct State : private NoCopyNoMove
    {
        State(const ConfigInput& cfg) : cfg_ { cfg } { }
        ConfigInput cfg_;
        DataSrcDst dbSrcId_ = 0;
        bool dbDirty_ = false;
    };
    std::map<std::string, State> states_;

    struct Point {
        Point(const Database::Row& row) : fixType_ { row.fix_type }, fixOk_ { row.fix_ok }, lat_ { row.pos_llh_lat }, lon_ { row.pos_llh_lon } { }
        FixType fixType_ = FixType::UNKNOWN;
        bool fixOk_ = false;
        double lat_ = 0.0;
        double lon_ = 0.0;
    };

    using PointMap = std::map<State*, Point>;
    std::map<uint64_t, PointMap> points_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_MULTIMAP_HPP__
