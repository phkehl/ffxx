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
#include <algorithm>
//
#include <glm/gtc/matrix_transform.hpp>
// #include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
//
#include "GL/gl3w.h"
//
#include "gui_widget_opengl.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWidgetOpenGl::GuiWidgetOpenGl() /* clang-format off */ :
    stuffChanged_      { true },
    controlsDebug_     { false },
    cameraFieldOfView_ { 45.0f },
    cameraLookAt_      { 0.0f, 0.0f, 0.0f },
    cameraAzim_        { 25.0f },
    cameraElev_        { 45.0f },
    cameraDist_        { 20.0f },
    cameraRoll_        {  0.0f },
    projType_          { PERSPECTIVE },
    cullNear_          { 0.01 },
    cullFar_           { 1000.0 },
    forceRender_       { false },
    isDragging_        { ImGuiMouseButton_COUNT },
    ambientLight_      { 0.5f, 0.5f, 0.5f },
    diffuseLight_      { 0.5f, 0.5f, 0.5f },
    diffuseDirection_  { -0.5f, 0.8f, 0.0f }  // clang-format on
{
    _UpdateViewAndProjection();
}

GuiWidgetOpenGl::~GuiWidgetOpenGl()
{
}

// ---------------------------------------------------------------------------------------------------------------------

ImVec2 GuiWidgetOpenGl::GetSize()
{
    return size_;
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetOpenGl::BeginDraw(const ImVec2& size, const bool forceRender)
{
    // Use user-supplied size or get available space
    ImVec2 newSize = size;
    if ((newSize.x <= 0) || (newSize.y <= 0)) {
        const ImVec2 sizeAvail = ImGui::GetContentRegionAvail();
        if (newSize.x <= 0) {
            newSize.x = sizeAvail.x;
        }
        if (newSize.y <= 0) {
            newSize.y = sizeAvail.y;
        }
    }
    pos0_ = ImGui::GetCursorScreenPos();
    if ((newSize.x != size_.x) || (newSize.y != size.y)) {
        size_ = newSize;
        _UpdateViewAndProjection();
        stuffChanged_ = true;
    }
    if (forceRender_ || forceRender) {
        stuffChanged_ = true;
        // forceRender_ = false;
    }

    // Activate OpenGL framebuffer to draw into
    if (framebuffer_.Begin(size_.x, size_.y)) {
        // ImGui child window where we'll place the rendered framebuffer into
        ImGui::BeginChild(ImGui::GetID(this), size_, false, ImGuiWindowFlags_NoScrollbar);

        // Draw debug controls
        if (controlsDebug_ && _DrawDebugControls()) {
            stuffChanged_ = true;
        }

        // Handle mouse interactions
        if (_HandleMouse()) {
            stuffChanged_ = true;
        }

        // Update stuff if stuff changed :)
        if (stuffChanged_) {
            _UpdateViewAndProjection();
            Clear();
        }

        // Setup OpenGL drawing
        glState_.Apply();

        ImGui::SetCursorScreenPos(pos0_);

        return true;
    } else {
        return false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetOpenGl::EndDraw()
{
    framebuffer_.End();
    stuffChanged_ = false;

    // Place rendered texture into ImGui draw list
    ImGui::GetWindowDrawList()->AddImage({ (ImTextureID)framebuffer_.GetTexture() }, pos0_, pos0_ + size_);

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetOpenGl::StuffChanged()
{
    return stuffChanged_;
}

// ---------------------------------------------------------------------------------------------------------------------

const glm::mat4& GuiWidgetOpenGl::GetViewMatrix()
{
    return view_;
}

// ---------------------------------------------------------------------------------------------------------------------

const glm::mat4& GuiWidgetOpenGl::GetProjectionMatrix()
{
    return projection_;
}

// ---------------------------------------------------------------------------------------------------------------------

const glm::vec3& GuiWidgetOpenGl::GetAmbientLight()
{
    return ambientLight_;
}

// ---------------------------------------------------------------------------------------------------------------------

const glm::vec3& GuiWidgetOpenGl::GetDiffuseLight()
{
    return diffuseLight_;
}

// ---------------------------------------------------------------------------------------------------------------------

const glm::vec3& GuiWidgetOpenGl::GetDiffuseDirection()
{
    return diffuseDirection_;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetOpenGl::Clear(const float r, const float g, const float b, const float a)
{
    framebuffer_.Clear(r, g, b, a);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetOpenGl::_UpdateViewAndProjection()
{
    // Sanitise values
    while (cameraAzim_ > CAMERA_AZIM_MAX) {
        cameraAzim_ -= 360.0f;
    }
    while (cameraAzim_ < CAMERA_AZIM_MIN) {
        cameraAzim_ += 360.0f;
    }
    cameraAzim_ = std::clamp(cameraAzim_, CAMERA_AZIM_MIN, CAMERA_AZIM_MAX);
    cameraElev_ = std::clamp(cameraElev_, CAMERA_ELEV_MIN, CAMERA_ELEV_MAX);
    cameraRoll_ = std::clamp(cameraRoll_, CAMERA_ROLL_MIN, CAMERA_ROLL_MAX);
    cameraDist_ = std::clamp(cameraDist_, CAMERA_DIST_MIN, CAMERA_DIST_MAX);

    // Calculate view matrix, https://stannum.io/blog/0UaG8R
    glm::mat4 view{ 1.0f };
    view = glm::translate(view, cameraLookAt_);
    view = glm::rotate(view, glm::radians(cameraRoll_), glm::vec3(0.0f, 0.0f, 1.0f));
    view = glm::rotate(view, glm::radians(cameraAzim_), glm::vec3(0.0f, 1.0f, 0.0f));
    view = glm::rotate(view, glm::radians(cameraElev_), glm::vec3(1.0f, 0.0f, 0.0f));

    diffuseDirection_ = glm::normalize(view * glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));  // TODO... hmmm...

    view = glm::translate(view, glm::vec3(0.0f, 0.0f, cameraDist_));
    view_ = glm::inverse(view);

    // Projection matrix
    const float aspect = size_.y > 0.0f ? size_.x / size_.y : 1.0f;
    switch (projType_) {
        case PERSPECTIVE:
            projection_ = glm::perspective(glm::radians(cameraFieldOfView_), aspect, cullNear_, cullFar_);
            break;
        case ORTHOGRAPHIC:
            projection_ =
                glm::ortho(-cameraDist_ * aspect, cameraDist_ * aspect, -cameraDist_, cameraDist_, cullNear_, cullFar_);
            break;
    }

    // Light
    diffuseDirection_ = glm::normalize(diffuseDirection_);
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetOpenGl::_HandleMouse()
{
    bool changed = false;

    // Make canvas area interactive
    ImGui::SetCursorScreenPos(pos0_);
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("canvas", size_, ImGuiButtonFlags_MouseButtonLeft);
    const bool isHovered = ImGui::IsItemHovered();

    // Debug popup
    if (ImGui::BeginPopupContextItem("GuiWidgetOpenGl")) {
        ImGui::Checkbox("Debug", &controlsDebug_);
        ImGui::EndPopup();
    }

    auto& io = ImGui::GetIO();
    if (isHovered) {
        // Not dragging
        if (isDragging_ == ImGuiMouseButton_COUNT) {
            // mouse wheel: forward/backwards
            if (io.MouseWheel != 0.0) {
                const float step = io.KeyShift ? 0.1 : (io.KeyCtrl ? 1.0 : 0.5);
                // DEBUG("wheel %.1f %.1f", io.MouseWheel, step);
                cameraDist_ -= (step * io.MouseWheel);
                changed = true;
            }

            // start of dragging
            else if (io.MouseClicked[ImGuiMouseButton_Left]) {
                isDragging_ = ImGuiMouseButton_Left;
                dragLastDelta_ = Vec2f(0, 0);
            } else if (io.MouseClicked[ImGuiMouseButton_Middle]) {
                isDragging_ = ImGuiMouseButton_Right;
                dragLastDelta_ = Vec2f(0, 0);
            } else if (io.MouseClicked[ImGuiMouseButton_Right]) {
                isDragging_ = ImGuiMouseButton_Right;
                dragLastDelta_ = Vec2f(0, 0);
            }
        }
        // Dragging
        else {
            if (isDragging_ == ImGuiMouseButton_Left) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                const Vec2f totDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                const Vec2f delta = totDelta - dragLastDelta_;
                dragLastDelta_ = totDelta;

                if (delta.x != 0.0f) {
                    float step = io.KeyShift ? 0.1 : (io.KeyCtrl ? 2.0 : 1.0);
                    cameraAzim_ -= 180 * delta.x / size_.x * step;
                    changed = true;
                }
                if (delta.y != 0.0f) {
                    float step = io.KeyShift ? 0.1 : (io.KeyCtrl ? 2.0 : 1.0);
                    cameraElev_ += 90 * delta.y / size_.y * step;
                    changed = true;
                }
            }

            else if (isDragging_ == ImGuiMouseButton_Right) {
#if 0
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                const Vec2f totDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
                const Vec2f delta = totDelta - dragLastDelta_;
                dragLastDelta_ = totDelta;
                if ( (delta.x != 0.0f) || (delta.y != 0.0f) )
                {
                    //DEBUG("drag right %.1f %.1f", delta.x, delta.y);
                    changed = true;
                    float step = io.KeyShift ? 0.05f : (io.KeyCtrl ? 0.5f : 0.1f);
                    const float azim = glm::radians(360 - cameraAzim_);
                    const float cosAzim = std::cos(azim);
                    const float sinAzim = std::sin(azim);
                    const float dx = cosAzim * delta.x + sinAzim * delta.y;
                    const float dy = sinAzim * delta.x + cosAzim * delta.y;
                    cameraLookAt_.x -= step * dx;
                    cameraLookAt_.z -= step * dy;
                }
#endif
            }
            // End dragging
            if (io.MouseReleased[ImGuiMouseButton_Left] || io.MouseReleased[ImGuiMouseButton_Right]) {
                isDragging_ = ImGuiMouseButton_COUNT;
            }
        }
    }

    return changed;
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetOpenGl::_DrawDebugControls()
{
    bool changed = false;

    ImGui::SetCursorScreenPos(pos0_);

    const float controlWidth = GUI_VAR.charSizeX * 15;

    ImGui::TextUnformatted("Camera:");

    ImGui::PushItemWidth(3 * controlWidth);
    if (ImGui::SliderFloat3("Look at", glm::value_ptr(cameraLookAt_), -25.0f, 25.0f, "%.1f")) {
        changed = true;
    }
    ImGui::PopItemWidth();

    ImGui::PushItemWidth(controlWidth);
    if (ImGui::SliderFloat("FoV", &cameraFieldOfView_, FIELD_OF_VIEW_MIN, FIELD_OF_VIEW_MAX, "%.1f")) {
        changed = true;
    }
    if (ImGui::SliderFloat("Azim", &cameraAzim_, CAMERA_AZIM_MIN, CAMERA_AZIM_MAX, "%.1f")) {
        changed = true;
    }
    if (ImGui::SliderFloat("Elev", &cameraElev_, CAMERA_ELEV_MIN, CAMERA_ELEV_MAX, "%.1f")) {
        changed = true;
    }
    if (ImGui::SliderFloat("Roll", &cameraRoll_, CAMERA_ROLL_MIN, CAMERA_ROLL_MAX, "%.1f")) {
        changed = true;
    }
    if (ImGui::SliderFloat("Dist", &cameraDist_, CAMERA_DIST_MIN, CAMERA_DIST_MAX, "%.1f")) {
        changed = true;
    }
    ImGui::PopItemWidth();

    ImGui::TextUnformatted("Projection:");
    ImGui::PushItemWidth(controlWidth);
    if (ImGui::Combo("##ProjType", &projType_, "Perspective\0Orhometric\0")) {
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::SliderFloat("Near", &cullNear_, 0.01, 1.0, "%.2f")) {
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::SliderFloat("Far", &cullFar_, 100.0, 2000.0, "%.0f")) {
        changed = true;
    }
    ImGui::PopItemWidth();

    ImGui::TextUnformatted("Light:");

    if (ImGui::ColorEdit3("Ambient", glm::value_ptr(ambientLight_), ImGuiColorEditFlags_NoInputs)) {
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::ColorEdit3("Diffuse", glm::value_ptr(diffuseLight_), ImGuiColorEditFlags_NoInputs)) {
        changed = true;
    }
    // ImGui::PushItemWidth(3 * controlWidth);
    // if (ImGui::SliderFloat3("Direction", glm::value_ptr(diffuseDirection_), -1.0f, 1.0f, "%.3f"))
    // {
    //     changed = true;
    // }
    // ImGui::PopItemWidth();
    ImGui::Text("%.3f %.3f %.3f", diffuseDirection_.x, diffuseDirection_.y, diffuseDirection_.z);

    ImGui::TextUnformatted("Flags:");

    if (ImGui::Checkbox("Depth test", &glState_.depthTestEnable)) {
        changed = true;
    }
    ImGui::BeginDisabled(!glState_.depthTestEnable);
    ImGui::SameLine();
    if (_StateEnumCombo("##depthFunc", glState_.depthFunc, OpenGL::State::DEPTH_FUNC)) {
        changed = true;
    }
    ImGui::EndDisabled();

    if (ImGui::Checkbox("Cull face", &glState_.cullFaceEnable)) {
        changed = true;
    }
    ImGui::BeginDisabled(!glState_.cullFaceEnable);
    ImGui::SameLine();
    if (_StateEnumCombo("##cullFace", glState_.cullFace, OpenGL::State::CULL_FACE)) {
        changed = true;
    }
    ImGui::SameLine();
    if (_StateEnumCombo("##cullFrontFace", glState_.cullFrontFace, OpenGL::State::CULL_FRONTFACE)) {
        changed = true;
    }
    ImGui::EndDisabled();

    if (_StateEnumCombo("Polygon mode", glState_.polygonMode, OpenGL::State::POLYGONMODE_MODE)) {
        changed = true;
    }

    if (ImGui::Checkbox("Blend", &glState_.blendEnable)) {
        changed = true;
    }
    ImGui::BeginDisabled(!glState_.blendEnable);
    ImGui::SameLine();
    if (_StateEnumCombo("##blendSrcRgb", glState_.blendSrcRgb, OpenGL::State::BLENDFUNC_FACTOR)) {
        changed = true;
    }
    ImGui::SameLine();
    if (_StateEnumCombo("##blendDstRgb", glState_.blendDstRgb, OpenGL::State::BLENDFUNC_FACTOR)) {
        changed = true;
    }
    ImGui::SameLine();
    if (_StateEnumCombo("##blendSrcAlpha", glState_.blendSrcAlpha, OpenGL::State::BLENDFUNC_FACTOR)) {
        changed = true;
    }
    ImGui::SameLine();
    if (_StateEnumCombo("##blendDstAlpha", glState_.blendDstAlpha, OpenGL::State::BLENDFUNC_FACTOR)) {
        changed = true;
    }
    ImGui::SameLine();
    if (_StateEnumCombo("##blendEqModeRgb", glState_.blendEqModeRgb, OpenGL::State::BLENDEQ_MODE)) {
        changed = true;
    }
    ImGui::SameLine();
    if (_StateEnumCombo("##blendEqModeAlpha", glState_.blendEqModeAlpha, OpenGL::State::BLENDEQ_MODE)) {
        changed = true;
    }
    ImGui::EndDisabled();

    ImGui::Checkbox("Force render", &forceRender_);

    return changed;
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetOpenGl::_StateEnumCombo(const char* label, uint32_t& value, const std::vector<OpenGL::Enum> enums)
{
    const char* preview = "?";
    for (auto& e : enums) {
        if (e.value == value) {
            preview = e.label;
            break;
        }
    }

    bool res = false;
    ImGui::PushItemWidth(GUI_VAR.charSizeX * 12);
    if (ImGui::BeginCombo(label, preview)) {
        for (auto& e : enums) {
            if (ImGui::Selectable(e.label, e.value == value)) {
                value = e.value;
                res = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    return res;
}

/* ****************************************************************************************************************** */
}  // namespace ffgui
