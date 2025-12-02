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

#ifndef __GUI_WIN_DATA_PLOT_HPP__
#define __GUI_WIN_DATA_PLOT_HPP__

//
#include "ffgui_inc.hpp"
//
#include <unordered_map>
//
#include "gui_win_data.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinDataPlot : public GuiWinData
{
   public:
    GuiWinDataPlot(const std::string& name, const InputPtr& input);
    ~GuiWinDataPlot();

   private:
    void _ProcessData(const DataPtr& data) final;
    void _Loop(const Time& now) final;
    void _ClearData() final;

    void _DrawToolbar() final;
    void _DrawContent() final;

    enum class FitMode
    {
        NONE,
        AUTOFIT,
        FOLLOW_X
    };

    struct ConfigVar
    {
        ImAxis axis;
        std::string name;
        inline bool operator<(const ConfigVar& rhs) const { return name == rhs.name ? axis < rhs.axis : name < rhs.name; }
        inline bool operator==(const ConfigVar& rhs) const { return (axis == rhs.axis) && (name == rhs.name); }
        inline bool operator!=(const ConfigVar& rhs) const { return !(*this == rhs); }
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ConfigVar, axis, name)

    struct Config
    {
        std::string note;
        bool showLegend = true;
        FitMode fitMode = FitMode::NONE;
        ImPlotColormap colormap = ImPlotColormap_Deep;
        std::vector<ConfigVar> vars;

        inline bool operator<(const Config& rhs) const { return note < rhs.note; }
        inline bool operator==(const Config& rhs) const
        {
            return (note == rhs.note) && (showLegend == rhs.showLegend) && (fitMode == rhs.fitMode) &&
                   (colormap == rhs.colormap) && (vars == rhs.vars);
        }
        inline bool operator!=(const Config& rhs) const { return !(*this == rhs); }
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, note, showLegend, fitMode, colormap, vars)
    Config cfg_;

    struct SavedConfig
    {
        std::vector<Config> plots;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SavedConfig, plots)
    static SavedConfig savedCfg_;


    void _ResetPlot();
    void _SetConfig(const Config& cfg);

    struct DragDrop
    {
        enum Target : int
        {
            UNKNOWN = -99,
            TRASH = -4,
            LIST = -3,
            PLOT = -2,
            LEGEND = -1,
            X = ImAxis_X1,
            Y1 = ImAxis_Y1,
            Y2 = ImAxis_Y2,
            Y3 = ImAxis_Y3
        };

        DragDrop(const Database::FieldDef& _field, const Target _src);
        DragDrop(const Database::FieldDef& _field, const Target _src, const Target _dst);
        DragDrop(const ImGuiPayload* imPayload, const Target _dst);
        Target src;
        Target dst;
        Database::FieldDef field;
    };

    std::unique_ptr<DragDrop> _CheckDrop(const ImAxis axis);
    void _HandleDrop(const DragDrop& dragdrop);
    void _UpdateVars();

    char dndType_[32];  // unique name for drag and drop functionality, see ctor
    bool dndHovered_ = false;

    struct PlotVar
    {
        bool valid = false;
        ImAxis axis = ImAxis_COUNT;
        Database::FieldDef field;
        inline bool operator==(const PlotVar& rhs) const { return field.field == rhs.field.field; }
    };

    PlotVar xVar_;
    std::vector<PlotVar> yVars_;
    std::array<std::string, ImAxis_COUNT> axisLabels_;
    std::array<bool,ImAxis_COUNT> axisUsed_ = {{ false, false, false, false, false, false }};
    std::string axisTooltips_[ImAxis_COUNT];
    std::unordered_map<const char*, bool> usedFields_;

    static constexpr ImPlotRect PLOT_LIMITS_DEF = { 0.0f, 100.0f, 0.0f, 10.0f };
    ImPlotRect plotLimitsY1_ = PLOT_LIMITS_DEF;
    ImPlotRect plotLimitsY2_ = PLOT_LIMITS_DEF;
    ImPlotRect plotLimitsY3_ = PLOT_LIMITS_DEF;
    double followXmax_ = 0.0;
    bool setLimitsX_ = true;
    bool setLimitsY_ = true;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_DATA_INF_HPP__
