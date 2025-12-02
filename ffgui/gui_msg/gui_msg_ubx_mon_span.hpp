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

#ifndef __GUI_MSG_UBX_MON_SPAN_HPP__
#define __GUI_MSG_UBX_MON_SPAN_HPP__

//
#include "ffgui_inc.hpp"
//
#include <fpsdk_common/parser/ubx.hpp>
using namespace fpsdk::common::parser::ubx;
//
#include "gui_widget_tabbar.hpp"
#include "gui_msg.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsgUbxMonSpan : public GuiMsg
{
   public:
    GuiMsgUbxMonSpan(const std::string& viewName, const InputPtr& input);

    void Update(const DataMsgPtr& msg) final;
    void Update(const Time& now) final;
    void Clear() final;

    void DrawToolbar() final;
    void DrawMsg() final;

   private:
    GuiWidgetTabbar tabbar_;

    struct Label
    {
        double freq_ = 0.0;
        const char* id_ = nullptr;
        const char* title_ = nullptr;
    };

    static constexpr std::array<Label, 11> FREQ_LABELS =  // clang-format off
    {{
        // L1 (E1)
        { 1575.420000, "##11",   "L1: GPS/SBAS L1CA, GAL E1, BDS B1C/I, QZSS L1CA/S" },
        { 1561.098000, "##12",   "L1: BDS B1I" },
      //{ 1602.000000, "##13",   "L1: GLO L1OF" }, // -7*0.5625=-3.9375 .. +6*0.5625=3.375 ==> 1598.0625 .. 1605.375
        { 1598.062500, "##13lo", "L1: GLO L1OF lo" },
        { 1605.375000, "##13hi", "L1: GLO L1OF hi" },
        // E6
        { 1268.520000, "##61",   "E6: BDS B3I" },
        { 1278.750000, "##62",   "E6: GAL E6"  },
        // L2
        { 1207.140000, "##21",   "L2: BDS B2I, GAL E5b" },
        { 1227.600000, "##22",   "L2: GPS/QZSS L2C" },
      //{ 1246.000000, "##23",   "L2: GLO L2OF" }, // -7*0.4375=-3.0625 .. +6*0.4375=2.625 ==> 1242.9375 .. 1248.625
        { 1242.937500, "##23lo", "L2: GLO L2OF (-7)" },
        { 1248.625000, "##23hi", "L2: GLO L2OF (+6)" },
        // L5
        { 1176.450000, "##51",   "L5: GPS/QZSS L5, GAL E5a, BDS B2a, NavIC L5a" },
    }};  // clang-format on

    static constexpr std::size_t NUM_HEAT = 180;
    static constexpr std::size_t NUM_BINS = UBX_MON_SPAN_V0_NUM_BINS;
    struct SpectData
    {
        bool valid_ = false;
        std::array<float, NUM_BINS> freq_;
        std::array<float, NUM_BINS> ampl_;
        std::array<float, NUM_BINS> min_;
        std::array<float, NUM_BINS> max_;
        std::array<float, NUM_BINS> mean_;
        float centre_ = 0.0;
        float span_ = 0.0;
        float res_ = 0.0;
        float pga_ = 0.0;
        float count_ = 0.0;
        std::string tab_;
        std::string title_;
        std::array<float, NUM_BINS * NUM_HEAT> heat_;
    };

    std::array<SpectData, 4> spects_;
    std::size_t numSpects_ = 0;
    bool resetPlotRange_ = true;
    void _DrawSpect(const SpectData& spect, const ImVec2 size);
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_UBX_MON_SPAN_HPP__
