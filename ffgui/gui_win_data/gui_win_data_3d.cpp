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

//
#include "ffgui_inc.hpp"
//
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
//
#include "GL/gl3w.h"
//
#include "gui_win_data_3d.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::types;

// ---------------------------------------------------------------------------------------------------------------------

// clang-format off
#define SHADER_MODE_GRID     (int32_t)0
#define SHADER_MODE_CUBES    (int32_t)1
#define SHADER_MODE_TRAJ     (int32_t)2

#define SHADER_ATTR_VERTEX_POS    0
#define SHADER_ATTR_VERTEX_NORMAL 1
#define SHADER_ATTR_VERTEX_COLOUR 2
#define SHADER_ATTR_INST_OFFS     3
#define SHADER_ATTR_INST_COLOUR   4
// clang-format on

static constexpr const char* VERTEX_SHADER_SRC = R"glsl(
    #version 330 core

    // Vertex attributes
    layout (location = 0) in vec3 vertexPos;
    layout (location = 1) in vec3 vertexNormal;
    layout (location = 2) in vec4 vertexColor;

    // Instance attributes
    layout (location = 3) in vec3 instanceOffset; // vertex offset (world space)
    layout (location = 4) in vec4 instanceColour;

    uniform int shaderMode;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    out vec3 fragmentNormal;
    out vec4 fragmentColor;

    const int SHADER_MODE_GRID  = 0;
    const int SHADER_MODE_CUBES = 1;
    const int SHADER_MODE_TRAJ  = 2;

    void main()
    {
        if (shaderMode == SHADER_MODE_CUBES)
        {
            gl_Position = projection * view * ((model * vec4(vertexPos, 1.0)) + vec4(instanceOffset, 0.0));
            fragmentColor = vertexColor * instanceColour;
        }
        else if (shaderMode == SHADER_MODE_TRAJ)
        {
            gl_Position = projection * view * model * vec4(vertexPos, 1.0);
            fragmentColor = vertexColor;
        }
        else if (shaderMode == SHADER_MODE_GRID)
        {
            gl_Position = projection * view * model * vec4(vertexPos, 1.0); // x, y, z, w = 1
            fragmentColor = vertexColor;   // pass-through
        }
        fragmentNormal = vertexNormal; // pass-through FIXME: should be * model perhaps?
    }
)glsl";

// ---------------------------------------------------------------------------------------------------------------------

static constexpr const char* FRAGMENT_SHADER_SRC = R"glsl(
    #version 330 core

    in vec3 fragmentNormal;
    in vec4 fragmentColor;

    uniform int shaderMode;        // shared with vertex shader

    uniform vec3 ambientLight;     // ambient light (colour/level)
    uniform vec3 diffuseLight;     // diffuse light (colour/level)
    uniform vec3 diffuseDirection; // diffuse light (direction)

    out vec4 OutColor;

    const int SHADER_MODE_GRID  = 0;
    const int SHADER_MODE_CUBES = 1;
    const int SHADER_MODE_TRAJ  = 2;

    void main()
    {
        if (shaderMode == SHADER_MODE_CUBES)
        {
            // ambient light only
            // OutColor = vec4( ambientLight * fragmentColor.xyz, fragmentColor.w );
            // add diffuse light
            float diffuseFact = max( dot( normalize(fragmentNormal), normalize(diffuseDirection) ), 0.0 );
            vec3 diffuse = diffuseFact * diffuseLight;
            OutColor = vec4( (ambientLight + diffuse) * fragmentColor.xyz, fragmentColor.w );
        }
        else if (shaderMode == SHADER_MODE_TRAJ)
        {
            OutColor = fragmentColor;
        }
        else if (shaderMode == SHADER_MODE_GRID)
        {
            OutColor = fragmentColor;
        }
    }
)glsl";

// ---------------------------------------------------------------------------------------------------------------------

GuiWinData3d::GuiWinData3d(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 80, 25, 0, 0 }, ImGuiWindowFlags_None, input),
    shader_          { VERTEX_SHADER_SRC, FRAGMENT_SHADER_SRC },
    modelIdentity_   { 1.0f },
    modelMarker_     { glm::scale(modelIdentity_, glm::vec3(cfg_.markerSize)) }  // clang-format on
{
    DEBUG("GuiWinData3d(%s)", WinName().c_str());

    toolbarClearEna_ = false;

    GuiGlobal::LoadObj(WinName() + ".GuiWinData3d", cfg_);

    ClearData();

    // Markers
    glGenVertexArrays(1, &markerVertexArray_);
    glGenBuffers(1, &markerVertexBuffer_);
    glGenBuffers(1, &markerInstancesBuffer_);

    // Trajectory
    glGenVertexArrays(1, &trajVertexArray_);
    glGenBuffers(1, &trajVertexBuffer_);

    // Grid
    glGenVertexArrays(1, &gridVertexArray_);
    glGenBuffers(1, &gridVertexBuffer_);

    _UpdatePoints();
    _UpdateGrid();
}

GuiWinData3d::~GuiWinData3d()
{
    DEBUG("~GuiWinData3d(%s)", WinName().c_str());

    glDeleteVertexArrays(1, &markerVertexArray_);
    glDeleteBuffers(1, &markerVertexBuffer_);
    glDeleteBuffers(1, &markerInstancesBuffer_);

    glDeleteVertexArrays(1, &trajVertexArray_);
    glDeleteBuffers(1, &trajVertexBuffer_);

    glDeleteVertexArrays(1, &gridVertexArray_);
    glDeleteBuffers(1, &gridVertexBuffer_);

    GuiGlobal::SaveObj(WinName() + ".GuiWinData3d", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinData3d::_ProcessData(const DataPtr& /*data*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinData3d::_Loop(const Time& /*now*/)
{
    if (input_->database_->Changed(this)) {
        _UpdatePoints();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinData3d::_ClearData()
{
}

// ---------------------------------------------------------------------------------------------------------------------

static constexpr std::array<std::array<float, 3 + 4>, 142> TEST_POINTS = /* clang-format off */ {{
    // East, North, Up, red, green, blue, alpha
    /*000*/ {{ -1.667, -0.964,  0.444,   0.000, 0.851, 0.000, 0.667 }},
    /*001*/ {{ -1.686, -1.002,  0.452,   0.000, 0.851, 0.000, 0.667 }},
    /*002*/ {{ -1.707, -1.039,  0.458,   0.000, 0.851, 0.000, 0.667 }},
    /*003*/ {{ -1.726, -1.069,  0.461,   0.000, 0.851, 0.000, 0.667 }},
    /*004*/ {{ -1.755, -1.110,  0.458,   0.000, 0.851, 0.000, 0.667 }},
    /*005*/ {{ -1.770, -1.143,  0.438,   0.000, 0.851, 0.000, 0.667 }},
    /*006*/ {{ -1.786, -1.173,  0.419,   0.000, 0.851, 0.000, 0.667 }},
    /*007*/ {{ -1.808, -1.208,  0.420,   0.000, 0.851, 0.000, 0.667 }},
    /*008*/ {{ -1.830, -1.244,  0.414,   0.000, 0.851, 0.000, 0.667 }},
    /*009*/ {{ -1.853, -1.278,  0.407,   0.000, 0.851, 0.000, 0.667 }},
    /*010*/ {{ -1.872, -1.305,  0.394,   0.000, 0.851, 0.000, 0.667 }},
    /*011*/ {{ -1.888, -1.333,  0.388,   0.000, 0.851, 0.000, 0.667 }},
    /*012*/ {{ -1.903, -1.362,  0.383,   0.000, 0.851, 0.000, 0.667 }},
    /*013*/ {{ -1.921, -1.392,  0.377,   0.000, 0.851, 0.000, 0.667 }},
    /*014*/ {{ -1.946, -1.420,  0.354,   0.000, 0.851, 0.000, 0.667 }},
    /*015*/ {{ -1.961, -1.439,  0.321,   0.000, 0.851, 0.000, 0.667 }},
    /*016*/ {{ -1.979, -1.457,  0.296,   0.000, 0.851, 0.000, 0.667 }},
    /*017*/ {{ -1.978, -1.456,  0.247,   0.000, 0.851, 0.000, 0.667 }},
    /*018*/ {{ -1.978, -1.463,  0.218,   0.000, 0.851, 0.000, 0.667 }},
    /*019*/ {{ -1.981, -1.481,  0.211,   0.000, 0.851, 0.000, 0.667 }},
    /*020*/ {{ -1.991, -1.504,  0.214,   0.000, 0.851, 0.000, 0.667 }},
    /*021*/ {{ -1.999, -1.520,  0.218,   0.000, 0.851, 0.000, 0.667 }},
    /*022*/ {{ -2.014, -1.538,  0.203,   0.000, 0.851, 0.000, 0.667 }},
    /*023*/ {{ -2.018, -1.551,  0.184,   0.000, 0.851, 0.000, 0.667 }},
    /*024*/ {{ -2.011, -1.549,  0.181,   0.000, 0.851, 0.000, 0.667 }},
    /*025*/ {{ -2.020, -1.562,  0.173,   0.000, 0.851, 0.000, 0.667 }},
    /*026*/ {{ -2.030, -1.573,  0.154,   0.000, 0.851, 0.000, 0.667 }},
    /*027*/ {{ -2.040, -1.581,  0.156,   0.000, 0.851, 0.000, 0.667 }},
    /*028*/ {{ -2.052, -1.595,  0.148,   0.000, 0.851, 0.000, 0.667 }},
    /*029*/ {{ -2.060, -1.604,  0.168,   0.000, 0.851, 0.000, 0.667 }},
    /*030*/ {{ -2.049, -1.596,  0.162,   0.000, 0.851, 0.000, 0.667 }},
    /*031*/ {{ -2.039, -1.594,  0.161,   0.000, 0.851, 0.000, 0.667 }},
    /*032*/ {{ -2.042, -1.604,  0.170,   0.000, 0.851, 0.000, 0.667 }},
    /*033*/ {{ -2.042, -1.610,  0.180,   0.000, 0.851, 0.000, 0.667 }},
    /*034*/ {{ -2.048, -1.621,  0.202,   0.000, 0.851, 0.000, 0.667 }},
    /*035*/ {{ -2.054, -1.631,  0.217,   0.000, 0.851, 0.000, 0.667 }},
    /*036*/ {{ -2.053, -1.638,  0.229,   0.000, 0.851, 0.000, 0.667 }},
    /*037*/ {{ -2.044, -1.636,  0.227,   0.000, 0.851, 0.000, 0.667 }},
    /*038*/ {{ -2.034, -1.633,  0.221,   0.000, 0.851, 0.000, 0.667 }},
    /*039*/ {{ -2.035, -1.643,  0.234,   0.000, 0.851, 0.000, 0.667 }},
    /*040*/ {{ -2.041, -1.660,  0.232,   0.000, 0.851, 0.000, 0.667 }},
    /*041*/ {{ -2.040, -1.662,  0.233,   0.000, 0.851, 0.000, 0.667 }},
    /*042*/ {{ -2.040, -1.660,  0.246,   0.000, 0.851, 0.000, 0.667 }},
    /*043*/ {{ -2.045, -1.660,  0.265,   0.000, 0.851, 0.000, 0.667 }},
    /*044*/ {{ -2.043, -1.655,  0.271,   0.000, 0.851, 0.000, 0.667 }},
    /*045*/ {{ -2.034, -1.645,  0.267,   0.000, 0.851, 0.000, 0.667 }},
    /*046*/ {{ -2.035, -1.644,  0.254,   0.000, 0.851, 0.000, 0.667 }},
    /*047*/ {{ -2.034, -1.649,  0.259,   0.000, 0.851, 0.000, 0.667 }},
    /*048*/ {{ -2.042, -1.651,  0.262,   0.000, 0.851, 0.000, 0.667 }},
    /*049*/ {{ -2.056, -1.658,  0.272,   0.000, 0.851, 0.000, 0.667 }},
    /*050*/ {{ -2.074, -1.680,  0.258,   0.000, 0.851, 0.000, 0.667 }},
    /*051*/ {{ -2.081, -1.692,  0.253,   0.000, 0.851, 0.000, 0.667 }},
    /*052*/ {{ -2.075, -1.697,  0.233,   0.000, 0.851, 0.000, 0.667 }},
    /*053*/ {{ -2.076, -1.705,  0.206,   0.000, 0.851, 0.000, 0.667 }},
    /*054*/ {{ -2.072, -1.708,  0.168,   0.000, 0.851, 0.000, 0.667 }},
    /*055*/ {{ -2.080, -1.714,  0.144,   0.000, 0.851, 0.000, 0.667 }},
    /*056*/ {{ -2.092, -1.720,  0.126,   0.000, 0.851, 0.000, 0.667 }},
    /*057*/ {{ -2.100, -1.726,  0.105,   0.000, 0.851, 0.000, 0.667 }},
    /*058*/ {{ -2.100, -1.728,  0.071,   0.000, 0.851, 0.000, 0.667 }},
    /*059*/ {{ -2.098, -1.729,  0.058,   0.000, 0.851, 0.000, 0.667 }},
    /*060*/ {{ -2.102, -1.733,  0.048,   0.000, 0.851, 0.000, 0.667 }},
    /*061*/ {{ -2.111, -1.738,  0.031,   0.000, 0.851, 0.000, 0.667 }},
    /*062*/ {{ -2.128, -1.749,  0.023,   0.000, 0.851, 0.000, 0.667 }},
    /*063*/ {{ -2.139, -1.759,  0.018,   0.000, 0.851, 0.000, 0.667 }},
    /*064*/ {{ -2.146, -1.769, -0.002,   0.000, 0.851, 0.000, 0.667 }},
    /*065*/ {{ -2.151, -1.768, -0.004,   0.000, 0.851, 0.000, 0.667 }},
    /*066*/ {{ -2.160, -1.767, -0.017,   0.000, 0.851, 0.000, 0.667 }},
    /*067*/ {{ -2.177, -1.767, -0.022,   0.000, 0.851, 0.000, 0.667 }},
    /*068*/ {{ -2.186, -1.762, -0.018,   0.000, 0.851, 0.000, 0.667 }},
    /*069*/ {{ -2.191, -1.752, -0.017,   0.000, 0.851, 0.000, 0.667 }},
    /*070*/ {{ -2.202, -1.753, -0.010,   0.000, 0.851, 0.000, 0.667 }},
    /*071*/ {{ -2.220, -1.755,  0.006,   0.000, 0.851, 0.000, 0.667 }},
    /*072*/ {{ -2.224, -1.749,  0.013,   0.000, 0.851, 0.000, 0.667 }},
    /*073*/ {{ -2.228, -1.744,  0.001,   0.000, 0.851, 0.000, 0.667 }},
    /*074*/ {{ -2.236, -1.743,  0.013,   0.000, 0.851, 0.000, 0.667 }},
    /*075*/ {{ -2.241, -1.742,  0.008,   0.000, 0.851, 0.000, 0.667 }},
    /*076*/ {{ -2.265, -1.765,  0.015,   0.000, 0.851, 0.000, 0.667 }},
    /*077*/ {{ -2.275, -1.769,  0.018,   0.000, 0.851, 0.000, 0.667 }},
    /*078*/ {{ -2.285, -1.771,  0.011,   0.000, 0.851, 0.000, 0.667 }},
    /*079*/ {{ -2.285, -1.765, -0.007,   0.000, 0.851, 0.000, 0.667 }},
    /*080*/ {{ -2.284, -1.758, -0.035,   0.000, 0.851, 0.000, 0.667 }},
    /*081*/ {{ -2.289, -1.757, -0.060,   0.000, 0.851, 0.000, 0.667 }},
    /*082*/ {{ -2.301, -1.762, -0.063,   0.000, 0.851, 0.000, 0.667 }},
    /*083*/ {{ -2.301, -1.752, -0.077,   0.000, 0.851, 0.000, 0.667 }},
    /*084*/ {{ -2.312, -1.752, -0.083,   0.000, 0.851, 0.000, 0.667 }},
    /*085*/ {{ -2.315, -1.746, -0.088,   0.000, 0.851, 0.000, 0.667 }},
    /*086*/ {{ -2.303, -1.732, -0.101,   0.000, 0.851, 0.000, 0.667 }},
    /*087*/ {{ -2.287, -1.716, -0.127,   0.000, 0.851, 0.000, 0.667 }},
    /*088*/ {{ -2.262, -1.708, -0.199,   0.000, 0.851, 0.000, 0.667 }},
    /*089*/ {{ -2.235, -1.696, -0.274,   0.000, 0.851, 0.000, 0.667 }},
    /*090*/ {{ -2.206, -1.679, -0.356,   0.000, 0.851, 0.000, 0.667 }},
    /*091*/ {{ -2.177, -1.666, -0.426,   0.000, 0.851, 0.000, 0.667 }},
    /*092*/ {{ -2.151, -1.661, -0.485,   0.000, 0.851, 0.000, 0.667 }},
    /*093*/ {{ -2.110, -1.640, -0.561,   0.000, 0.851, 0.000, 0.667 }},
    /*094*/ {{ -2.075, -1.625, -0.622,   0.000, 0.851, 0.000, 0.667 }},
    /*095*/ {{ -2.048, -1.615, -0.678,   0.000, 0.851, 0.000, 0.667 }},
    /*096*/ {{ -2.008, -1.605, -0.747,   0.000, 0.851, 0.000, 0.667 }},
    /*097*/ {{ -1.967, -1.595, -0.794,   0.000, 0.851, 0.000, 0.667 }},
    /*098*/ {{ -1.935, -1.554, -0.813,   0.000, 0.851, 0.000, 0.667 }},
    /*099*/ {{ -1.896, -1.507, -0.831,   0.000, 0.851, 0.000, 0.667 }},
    /*100*/ {{ -1.869, -1.470, -0.830,   0.000, 0.851, 0.000, 0.667 }},
    /*101*/ {{ -1.847, -1.433, -0.829,   0.000, 0.851, 0.000, 0.667 }},
    /*102*/ {{ -1.809, -1.393, -0.835,   0.000, 0.851, 0.000, 0.667 }},
    /*103*/ {{ -1.770, -1.345, -0.849,   0.000, 0.851, 0.000, 0.667 }},
    /*104*/ {{ -1.730, -1.296, -0.860,   0.000, 0.851, 0.000, 0.667 }},
    /*105*/ {{ -1.699, -1.258, -0.868,   0.000, 0.851, 0.000, 0.667 }},
    /*106*/ {{ -1.664, -1.214, -0.854,   0.000, 0.851, 0.000, 0.667 }},
    /*107*/ {{ -1.634, -1.173, -0.826,   0.000, 0.851, 0.000, 0.667 }},
    /*108*/ {{ -1.575, -1.103, -0.654,   0.000, 0.851, 0.000, 0.667 }},
    /*109*/ {{ -1.499, -1.023, -0.501,   0.000, 0.851, 0.000, 0.667 }},
    /*110*/ {{ -1.421, -0.941, -0.354,   0.000, 0.851, 0.000, 0.667 }},
    /*111*/ {{ -1.354, -0.874, -0.211,   0.000, 0.851, 0.000, 0.667 }},
    /*112*/ {{ -1.446, -1.463,  0.844,   0.000, 0.851, 0.000, 0.667 }},
    /*113*/ {{ -0.938, -1.348,  1.516,   1.000, 1.000, 0.000, 0.667 }},
    /*114*/ {{ -0.555, -1.143,  1.765,   1.000, 1.000, 0.000, 0.667 }},
    /*115*/ {{ -0.018, -0.576,  2.124,   1.000, 1.000, 0.000, 0.667 }},
    /*116*/ {{  0.382, -0.213,  2.287,   1.000, 1.000, 0.000, 0.667 }},
    /*117*/ {{  0.991,  0.353,  2.370,   1.000, 1.000, 0.000, 0.667 }},
    /*118*/ {{  1.486,  0.763,  2.348,   1.000, 1.000, 0.000, 0.667 }},
    /*119*/ {{  2.281,  1.417,  2.207,   1.000, 1.000, 0.000, 0.667 }},
    /*120*/ {{  2.868,  1.892,  2.028,   1.000, 1.000, 0.000, 0.667 }},
    /*121*/ {{  3.843,  2.721,  1.818,   1.000, 1.000, 0.000, 0.667 }},
    /*122*/ {{  4.557,  3.326,  1.587,   1.000, 1.000, 0.000, 0.667 }},
    /*123*/ {{  5.469,  4.127,  1.279,   1.000, 1.000, 0.000, 0.667 }},
    /*124*/ {{  6.139,  4.719,  0.985,   1.000, 1.000, 0.000, 0.667 }},
    /*125*/ {{  6.970,  5.420,  0.664,   1.000, 1.000, 0.000, 0.667 }},
    /*126*/ {{  7.594,  5.947,  0.387,   1.000, 1.000, 0.000, 0.667 }},
    /*127*/ {{  8.408,  6.629,  0.061,   1.000, 1.000, 0.000, 0.667 }},
    /*128*/ {{  9.022,  7.144, -0.207,   1.000, 1.000, 0.000, 0.667 }},
    /*129*/ {{  9.763,  7.760, -0.494,   1.000, 1.000, 0.000, 0.667 }},
    /*130*/ {{ 10.323,  8.226, -0.737,   1.000, 1.000, 0.000, 0.667 }},
    /*131*/ {{ 11.021,  8.803, -0.943,   1.000, 1.000, 0.000, 0.667 }},
    /*132*/ {{ 11.552,  9.242, -1.116,   1.000, 1.000, 0.000, 0.667 }},
    /*133*/ {{ 12.065,  9.656, -1.206,   1.000, 1.000, 0.000, 0.667 }},
    /*134*/ {{ 12.476,  9.982, -1.300,   1.000, 1.000, 0.000, 0.667 }},
    /*135*/ {{ 13.097, 10.427, -1.654,   1.000, 1.000, 0.000, 0.667 }},
    /*136*/ {{ 13.567, 10.762, -1.931,   1.000, 1.000, 0.000, 0.667 }},
    /*137*/ {{ 14.167, 11.210, -2.251,   1.000, 1.000, 0.000, 0.667 }},
    /*138*/ {{ 14.620, 11.541, -2.504,   1.000, 1.000, 0.000, 0.667 }},
    /*139*/ {{ 15.103, 11.905, -2.765,   1.000, 1.000, 0.000, 0.667 }},
    /*140*/ {{ 15.474, 12.185, -2.980,   1.000, 1.000, 0.000, 0.667 }},
    /*141*/ {{ 15.934, 12.517, -3.203,   1.000, 1.000, 0.000, 0.667 }},
}};  // clang-format on

/*static*/  // const std::vector<OpenGL::Vertex> GuiWinData3d::CUBE_VERTICES =
static constexpr std::array<OpenGL::Vertex, 6 * 6> CUBE_VERTICES = /* clang-format off */ {{
    // Fronts are CCW

    // Back face
    { {  0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

    // Front face
    { { -0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

    // Left face
    { { -0.5f,  0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f,  0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f, -0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f, -0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f, -0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f,  0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

    // Right face
    { {  0.5f, -0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f,  0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f,  0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f,  0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f, -0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f, -0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

    // Bottom face
    { { -0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

    // Top face
    { {  0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
}};         // clang-format on

void GuiWinData3d::_UpdatePoints()
{
    // One maker
    //markerVertices_.clear();
    //markerVertices_ = CUBE_VERTICES;
    glBindBuffer(GL_ARRAY_BUFFER, markerVertexBuffer_);
    //glBufferData(GL_ARRAY_BUFFER, markerVertices_.size() * OpenGL::Vertex::SIZE, markerVertices_.data(), GL_STATIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, CUBE_VERTICES.size() * OpenGL::Vertex::SIZE, CUBE_VERTICES.data(), GL_STATIC_DRAW);

    // Instances (= markers) and trajectory
    markerInstances_.clear();
    trajVertices_.clear();
#if 0
    markerInstances_.emplace_back(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    markerInstances_.emplace_back(glm::vec3(1.0f, 0.0f, -1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    markerInstances_.emplace_back(glm::vec3(2.0f, 0.0f, -2.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    for (std::size_t ix = 0; ix < TEST_POINTS.size(); ix++) {
        const auto& t = TEST_POINTS[ix];  // E/N/U: X/Y/Z = E/U/-N
        markerInstances_.emplace_back(glm::vec3(t[0], t[2], -t[1]), glm::vec4(t[3], t[4], t[5], t[6]));
        trajVertices_.push_back({ { t[0], t[2], -t[1] }, { 0.0f, 0.0f, 0.0f }, { t[3], t[4], t[5], t[6] } });
    }
#else
    input_->database_->ProcRows([&](const Database::Row& row) {
        if (!std::isnan(row.pos_enu_ref_east))
        {
            const float east  = row.pos_enu_ref_east;
            const float north = row.pos_enu_ref_north;
            const float up    = row.pos_enu_ref_up;
            const ImVec4 &col = FIX_COLOUR4(row.fix_type, row.fix_ok);
            // E/N/U: X/Y/Z = E/U/-N
            markerInstances_.emplace_back(glm::vec3(east, up, -north), glm::vec4(col.x, col.y, col.z, col.w));
            trajVertices_.push_back({ { east, up, -north }, { 0.0f, 0.0f, 0.0f }, { col.x, col.y, col.z, col.w } });
        }
        return true;
    });
#endif
    glBindBuffer(GL_ARRAY_BUFFER, markerInstancesBuffer_);
    glBufferData(GL_ARRAY_BUFFER, markerInstances_.size() * sizeof(markerInstances_[0]), markerInstances_.data(), GL_STATIC_DRAW);

    // Trajectory

    glBindBuffer(GL_ARRAY_BUFFER, trajVertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, trajVertices_.size() * sizeof(trajVertices_[0]), trajVertices_.data(), GL_STATIC_DRAW);

    forceRender_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinData3d::_UpdateGrid()
{
    gridVertices_.clear();
    // clang-format off

    // Calculate grid lines
    const int gridSize = std::clamp(cfg_.gridSize, 2, 200);
    const int xz1 = gridSize / 2;
    const int xz0 = -xz1;
    const glm::vec4 gridMinor(C4_PLOT_GRID_MINOR().x, C4_PLOT_GRID_MINOR().y, C4_PLOT_GRID_MINOR().z, C4_PLOT_GRID_MINOR().w);
    const glm::vec4 gridMajor(C4_PLOT_GRID_MAJOR().x, C4_PLOT_GRID_MAJOR().y, C4_PLOT_GRID_MAJOR().z, C4_PLOT_GRID_MAJOR().w);

    for (int xz = xz0; xz <= xz1; xz++)
    {
        gridVertices_.push_back({ { (float)xz0, 0.0f, (float)xz }, { 0.0f, 1.0f, 0.0f }, xz == 0 ? gridMajor : gridMinor });
        gridVertices_.push_back({ { (float)xz1, 0.0f, (float)xz }, { 0.0f, 1.0f, 0.0f }, xz == 0 ? gridMajor : gridMinor });

        gridVertices_.push_back({ { (float)xz, 0.0f, (float)xz0 }, { 0.0f, 1.0f, 0.0f }, xz == 0 ? gridMajor : gridMinor });
        gridVertices_.push_back({ { (float)xz, 0.0f, (float)xz1 }, { 0.0f, 1.0f, 0.0f }, xz == 0 ? gridMajor : gridMinor });
    }

    gridVertices_.push_back({ { 0.0f, (float)xz0, 0.0f }, { 0.0f, 0.0f, 1.0f }, gridMajor });
    gridVertices_.push_back({ { 0.0f, (float)xz1, 0.0f }, { 0.0f, 0.0f, 1.0f }, gridMajor });

    const float N[][2][2] = {
        { { 0.2f, 0.2f }, { 0.2f, 0.6f } },
        { { 0.2f, 0.6f }, { 0.4f, 0.2f } },
        { { 0.4f, 0.2f }, { 0.4f, 0.6f } },
    };
    for (std::size_t ix = 0; ix < NumOf(N); ix++) {
        gridVertices_.push_back({ { N[ix][0][0], 0.0f, -1.0f - N[ix][0][1] }, { 0.0f, 0.0f, 1.0f }, gridMajor });
        gridVertices_.push_back({ { N[ix][1][0], 0.0f, -1.0f - N[ix][1][1] }, { 0.0f, 0.0f, 1.0f }, gridMajor });
    }
    const float E[][2][2] = {
        { { 0.2f, 0.2f }, { 0.4f, 0.2f } },
        { { 0.2f, 0.2f }, { 0.2f, 0.6f } },
        { { 0.2f, 0.6f }, { 0.4f, 0.6f } },
        { { 0.2f, 0.4f }, { 0.4f, 0.4f } },
    };
    for (std::size_t ix = 0; ix < NumOf(E); ix++) {
        gridVertices_.push_back({ { 1.0f + E[ix][0][0], 0.0f, -E[ix][0][1] }, { 0.0f, 0.0f, 1.0f }, gridMajor });
        gridVertices_.push_back({ { 1.0f + E[ix][1][0], 0.0f, -E[ix][1][1] }, { 0.0f, 0.0f, 1.0f }, gridMajor });
    }

    // Copy data to GPU
    glBindVertexArray(gridVertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, gridVertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, gridVertices_.size() * sizeof(gridVertices_[0]), gridVertices_.data(), GL_STATIC_DRAW);

    forceRender_ = true;

    // clang-format on
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinData3d::_DrawToolbar()
{
    if (Gui::ToggleButton(ICON_FK_CIRCLE "##DrawMarker", nullptr, &cfg_.drawMarker, "Drawing markers",
            "Not drawing markers", GUI_VAR.iconSize)) {
        forceRender_ = true;
    }

    ImGui::SameLine();

    if (Gui::ToggleButton(ICON_FK_LINE_CHART "##cfg_.drawTraj", nullptr, &cfg_.drawTraj, "Drawing trajectory",
            "Not drawing trajectory", GUI_VAR.iconSize)) {
        forceRender_ = true;
    }

    ImGui::SameLine();

    if (Gui::ToggleButton(ICON_FK_PLUS_SQUARE "##DrawGrid", nullptr, &cfg_.drawGrid, "Drawing grid", "Not drawing grid",
            GUI_VAR.iconSize)) {
        forceRender_ = true;
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(!cfg_.drawMarker);
    ImGui::PushItemWidth(GUI_VAR.charSizeX * 20);
    if (ImGui::SliderFloat("##MarkerSize", &cfg_.markerSize, 0.001f, 1.0f)) {
        modelMarker_ = glm::scale(modelIdentity_, glm::vec3(cfg_.markerSize));
        forceRender_ = true;
    }
    ImGui::PopItemWidth();
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(!cfg_.drawGrid);
    ImGui::PushItemWidth(GUI_VAR.charSizeX * 20);
    if (ImGui::SliderInt("##GridSize", &cfg_.gridSize, 2.0f, 200.0f)) {
        _UpdateGrid();
    }
    ImGui::PopItemWidth();
    ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinData3d::_DrawContent()
{
    // const ImVec2 canvasSize = Vec2f(ImGui::GetContentRegionAvail()) - Vec2f(0, 30);
    if (!glWidget_.BeginDraw(ImVec2(), forceRender_)) {
        return;
    }

    // Render
    if (glWidget_.StuffChanged() && shader_.Ready()) {
        forceRender_ = false;

        // Enable shader
        shader_.Use();

        // Configure transformations
        shader_.SetUniform("projection", glWidget_.GetProjectionMatrix());
        shader_.SetUniform("view", glWidget_.GetViewMatrix());

        // Render grid
        if (cfg_.drawGrid) {  // clang-format off
            shader_.SetUniform("shaderMode", SHADER_MODE_GRID);
            shader_.SetUniform("model", modelIdentity_);
            glBindVertexArray(gridVertexArray_);
            glBindBuffer(GL_ARRAY_BUFFER, gridVertexBuffer_);
            glEnableVertexAttribArray(SHADER_ATTR_VERTEX_POS);
            glVertexAttribPointer(    SHADER_ATTR_VERTEX_POS,    OpenGL::Vertex::POS_NUM,    GL_FLOAT, GL_FALSE, OpenGL::Vertex::SIZE, (void *)OpenGL::Vertex::POS_OFFS);
            glEnableVertexAttribArray(SHADER_ATTR_VERTEX_NORMAL);
            glVertexAttribPointer(    SHADER_ATTR_VERTEX_NORMAL, OpenGL::Vertex::NORMAL_NUM, GL_FLOAT, GL_FALSE, OpenGL::Vertex::SIZE, (void *)OpenGL::Vertex::NORMAL_OFFS);
            glEnableVertexAttribArray(SHADER_ATTR_VERTEX_COLOUR);
            glVertexAttribPointer(    SHADER_ATTR_VERTEX_COLOUR, OpenGL::Vertex::COLOUR_NUM, GL_FLOAT, GL_FALSE, OpenGL::Vertex::SIZE, (void *)OpenGL::Vertex::COLOUR_OFFS);
            glDrawArrays(GL_LINES, 0, gridVertices_.size());
        }  // clang-format on

        // Render trajectory
        if (cfg_.drawTraj) {  // clang-format off
            shader_.SetUniform("shaderMode", SHADER_MODE_TRAJ);
            shader_.SetUniform("model", modelIdentity_);
            glBindVertexArray(trajVertexArray_);
            glBindBuffer(GL_ARRAY_BUFFER, trajVertexBuffer_);
            glEnableVertexAttribArray(SHADER_ATTR_VERTEX_POS);
            glVertexAttribPointer(    SHADER_ATTR_VERTEX_POS,    OpenGL::Vertex::POS_NUM,    GL_FLOAT, GL_FALSE, OpenGL::Vertex::SIZE, (void *)OpenGL::Vertex::POS_OFFS);
            glEnableVertexAttribArray(SHADER_ATTR_VERTEX_NORMAL);
            glVertexAttribPointer(    SHADER_ATTR_VERTEX_NORMAL, OpenGL::Vertex::NORMAL_NUM, GL_FLOAT, GL_FALSE, OpenGL::Vertex::SIZE, (void *)OpenGL::Vertex::NORMAL_OFFS);
            glEnableVertexAttribArray(SHADER_ATTR_VERTEX_COLOUR);
            glVertexAttribPointer(    SHADER_ATTR_VERTEX_COLOUR, OpenGL::Vertex::COLOUR_NUM, GL_FLOAT, GL_FALSE, OpenGL::Vertex::SIZE, (void *)OpenGL::Vertex::COLOUR_OFFS);
            glDrawArrays(GL_LINE_STRIP, 0, trajVertices_.size());
        }  // clang-format on

        // Render markers
        if (cfg_.drawMarker) {  // clang-format off
            shader_.SetUniform("shaderMode", SHADER_MODE_CUBES);
            shader_.SetUniform("model", modelMarker_);
            shader_.SetUniform("ambientLight", glWidget_.GetAmbientLight());
            shader_.SetUniform("diffuseLight", glWidget_.GetDiffuseLight());
            shader_.SetUniform("diffuseDirection", glWidget_.GetDiffuseDirection());
            glBindVertexArray(markerVertexArray_);
            glBindBuffer(GL_ARRAY_BUFFER, markerVertexBuffer_);
            glEnableVertexAttribArray(SHADER_ATTR_VERTEX_POS);
            glVertexAttribPointer(    SHADER_ATTR_VERTEX_POS,    OpenGL::Vertex::POS_NUM,    GL_FLOAT, GL_FALSE, OpenGL::Vertex::SIZE, (void *)OpenGL::Vertex::POS_OFFS);
            glEnableVertexAttribArray(SHADER_ATTR_VERTEX_NORMAL);
            glVertexAttribPointer(    SHADER_ATTR_VERTEX_NORMAL, OpenGL::Vertex::NORMAL_NUM, GL_FLOAT, GL_FALSE, OpenGL::Vertex::SIZE, (void *)OpenGL::Vertex::NORMAL_OFFS);
            glEnableVertexAttribArray(SHADER_ATTR_VERTEX_COLOUR);
            glVertexAttribPointer(    SHADER_ATTR_VERTEX_COLOUR, OpenGL::Vertex::COLOUR_NUM, GL_FLOAT, GL_FALSE, OpenGL::Vertex::SIZE, (void *)OpenGL::Vertex::COLOUR_OFFS);
            glBindBuffer(GL_ARRAY_BUFFER, markerInstancesBuffer_);
            glEnableVertexAttribArray(SHADER_ATTR_INST_OFFS);
            glVertexAttribPointer(    SHADER_ATTR_INST_OFFS,     MarkerInstance::TRANS_NUM,  GL_FLOAT, GL_FALSE, MarkerInstance::SIZE, (void*)MarkerInstance::TRANS_OFFS);
            glVertexAttribDivisor(    SHADER_ATTR_INST_OFFS, 1);
            glEnableVertexAttribArray(SHADER_ATTR_INST_COLOUR);
            glVertexAttribPointer(    SHADER_ATTR_INST_COLOUR,   MarkerInstance::COLOUR_NUM, GL_FLOAT, GL_FALSE, MarkerInstance::SIZE, (void*)MarkerInstance::COLOUR_OFFS);
            glVertexAttribDivisor(    SHADER_ATTR_INST_COLOUR, 1);
            glDrawArraysInstanced(GL_TRIANGLES, 0, CUBE_VERTICES.size(), markerInstances_.size());
        }  // clang-format on
    }

    glWidget_.EndDraw();
}

/* ****************************************************************************************************************** */
} // ffgui
