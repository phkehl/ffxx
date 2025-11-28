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

#ifndef __GUI_WIDGET_OPENGL_HPP__
#define __GUI_WIDGET_OPENGL_HPP__

//
#include "ffgui_inc.hpp"
//
#include <glm/glm.hpp>
//
#include "opengl.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWidgetOpenGl
{
   public:
    GuiWidgetOpenGl();
    ~GuiWidgetOpenGl();

    // 1. Setup and bind OpenGL frambuffer that you can draw into, returns true if ready for drawing
    bool BeginDraw(const ImVec2& size = ImVec2(0, 0), const bool forceRender = false);

    // 2. if BeginDraw() returned true, optionally do:

    // Framebuffer changed and must be drawn again. We can skip redrawing if this returns false.
    bool StuffChanged();

    // Get framebuffer size
    ImVec2 GetSize();

    const glm::mat4& GetViewMatrix();
    const glm::mat4& GetProjectionMatrix();
    const glm::vec3& GetAmbientLight();
    const glm::vec3& GetDiffuseLight();
    const glm::vec3& GetDiffuseDirection();

    // Clear framebuffer (automatically called when framebuffer changed)
    void Clear(const float r = 0.0f, const float g = 0.0f, const float b = 0.0f, const float a = 0.0f);

    // 3. if BeginDraw() returned true: put it on screen, unbind framebuffer, switch back to default one
    void EndDraw();

   protected:
    Vec2f size_;
    Vec2f pos0_;
    OpenGL::FrameBuffer framebuffer_;

    bool stuffChanged_;

    bool controlsDebug_;

    float cameraFieldOfView_;  // [deg]

    glm::vec3 cameraLookAt_;
    float cameraAzim_;  // 0 = -z direction
    float cameraElev_;  // 0 = horizon, >0 up
    float cameraDist_;
    float cameraRoll_;  // 180 = horizontal

    glm::mat4 view_;        // Camera (view, eye) matrix: world -> view
    glm::mat4 projection_;  // Projection matrix: view -> clip
    enum Projection_e
    {
        PERSPECTIVE = 0,
        ORTHOGRAPHIC = 1
    };
    int projType_;
    float cullNear_;
    float cullFar_;

    OpenGL::State glState_;
    bool forceRender_;
    bool _StateEnumCombo(const char* label, uint32_t& value, const std::vector<OpenGL::Enum> enums);

    // clang-format off
        static constexpr float FIELD_OF_VIEW_MIN =  10.0f;
        static constexpr float FIELD_OF_VIEW_MAX = 100.0f;
        static constexpr float CAMERA_AZIM_MIN  =    0.0f;
        static constexpr float CAMERA_AZIM_MAX  =  360.0f;
        static constexpr float CAMERA_ELEV_MIN  =  -90.0f;
        static constexpr float CAMERA_ELEV_MAX  =   90.0f;
        static constexpr float CAMERA_ROLL_MIN  = -180.0f;
        static constexpr float CAMERA_ROLL_MAX  =  180.0f;
        static constexpr float CAMERA_DIST_MIN  =    1.0f;
        static constexpr float CAMERA_DIST_MAX  =  100.0f;
    // clang-format on

    int isDragging_;
    Vec2f dragLastDelta_;

    glm::vec3 ambientLight_;
    glm::vec3 diffuseLight_;
    glm::vec3 diffuseDirection_;

    bool _HandleMouse();
    bool _DrawDebugControls();
    void _UpdateViewAndProjection();
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIDGET_OPENGL_HPP__
