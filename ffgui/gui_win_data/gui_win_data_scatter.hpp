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

#ifndef __GUI_WIN_DATA_SCATTER_HPP__
#define __GUI_WIN_DATA_SCATTER_HPP__

//
#include "ffgui_inc.hpp"
//
#include "gui_win_data.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinDataScatter : public GuiWinData
{
   public:
    GuiWinDataScatter(const std::string& name, const InputPtr& input);
    ~GuiWinDataScatter();

   private:
    void _ProcessData(const DataPtr& data) final;
    void _Loop(const Time& now) final;
    void _ClearData() final;

    void _DrawToolbar() final;
    void _DrawContent() final;

    static constexpr std::size_t NUM_SIGMA = 6;

    // Config
    struct Config
    {
        float plotRadius = 10.0f;
        bool showStats = false;
        bool showHistogram = false;
        bool showAccEst = true;
        std::array<bool, NUM_SIGMA> sigmaEna = { { false, false, false, false, false, false } };
        bool sizingMax = false;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, plotRadius, showStats, showHistogram, showAccEst, sigmaEna, sizingMax)
    Config cfg_;

    Database::Info dbInfo_;

    // Plot
    // For normal one-dimensional data 1, 2 and 3 sigma are the famous 68, 95 and 99.7%
    // For normal two-dimensional data 1, 2 and 3 sigma are 38.5%, 86.5%, 98.9%
    // Other scale factors for normal two-dimensional data: 2.4477 = 95%, 3.0349 = 99%, 3.7169 = 99.9%
    // See http://johnthemathguy.blogspot.com/2017/11/statistics-of-multi-dimensional-data.html,
    //     https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0118537
    // clang-format off
    static constexpr std::array<float, NUM_SIGMA>       SIGMA_SCALES = { {  1.0,              2.0,              2.4477,         3.0,              3.0349,         3.7169 } };
    static constexpr std::array<const char*, NUM_SIGMA> SIGMA_LABELS = { { "1.0    (38.5%)", "2.0    (86.5%)", "2.4477 (95%)", "3.0    (98.9%)", "3.0349 (99%)", "3.7169 (99.9%)" } };
    // clang-format on

    // clang-format off
    static constexpr std::size_t NUM_HIST = 50;
    std::array<int, NUM_HIST> histogramN_;
    std::array<int, NUM_HIST> histogramE_;
    // clang-format on

    int histNumPoints_;
    bool showingErrorEll_ = false;
    bool triggerSnapRadius_ = false;
    Eigen::Vector3d refPosLlhDragStart_;
    Eigen::Vector3d refPosXyzDragStart_;

    void _Update();
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_DATA_SCATTER_HPP__
