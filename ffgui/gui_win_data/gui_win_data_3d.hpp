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

#ifndef __GUI_WIN_DATA_THREED_HPP__
#define __GUI_WIN_DATA_THREED_HPP__

//
#include "ffgui_inc.hpp"
//
#include <glm/glm.hpp>
//
#include "gui_widget_opengl.hpp"
#include "gui_win_data.hpp"
#include "opengl.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinData3d : public GuiWinData
{
   public:
    GuiWinData3d(const std::string& name, const InputPtr& input);
    ~GuiWinData3d();

   private:
    void _ProcessData(const DataPtr& data) final;
    void _Loop(const Time& now) final;
    void _ClearData() final;

    void _DrawToolbar() final;
    void _DrawContent() final;

    struct Config
    {
        bool drawGrid = true;
        bool drawMarker = true;
        bool drawTraj = true;
        int gridSize = 50;
        float markerSize = 0.05f;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, drawGrid, drawMarker, drawTraj)
    Config cfg_;

    GuiWidgetOpenGl glWidget_;
    OpenGL::Shader shader_;
    bool forceRender_ = false;

    // North-East grid
    std::vector<OpenGL::Vertex> gridVertices_;
    unsigned int gridVertexArray_;
    unsigned int gridVertexBuffer_;
    void _UpdateGrid();

    // Markers
    struct MarkerInstance
    {
        MarkerInstance(const glm::vec3& _translate, const glm::vec4& _colour)
            : translate{ _translate }, colour{ _colour }
        {
        }
        glm::vec3 translate;
        glm::vec4 colour;
        static constexpr uint32_t TRANS_OFFS = 0;
        static constexpr int32_t TRANS_NUM = 3;
        static constexpr uint32_t COLOUR_OFFS = sizeof(translate);
        static constexpr int32_t COLOUR_NUM = 4;
        static constexpr uint32_t SIZE = sizeof(translate) + sizeof(colour);
    };
    //std::vector<OpenGL::Vertex> markerVertices_;
    std::vector<MarkerInstance> markerInstances_;
    unsigned int markerVertexArray_;
    unsigned int markerVertexBuffer_;
    unsigned int markerInstancesBuffer_;

    // Trajectory
    std::vector<OpenGL::Vertex> trajVertices_;
    unsigned int trajVertexArray_;
    unsigned int trajVertexBuffer_;

    void _UpdatePoints();

    glm::mat4 modelIdentity_;
    glm::mat4 modelMarker_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_DATA_THREED_HPP__
