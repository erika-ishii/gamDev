/*********************************************************************************************
 \file      Camera2D.cpp
 \par       SofaSpuds
 \author    yimo.kong ( yimo.kong@digipen.edu) - Primary Author, 100%
 \brief     Orthographic 2D camera with focus, view height, and aspect-aware projection.
 \details   Maintains a focus point in world space and an orthographic frustum sized by
            viewHeight and aspect (set via viewport size). Provides view, projection, and
            combined view-projection matrices for 2D rendering pipelines.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "Camera2D.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace gfx {

    /*****************************************************************************************
      \brief Update camera aspect ratio from the current viewport size.
      \param width   Back-buffer width in pixels.
      \param height  Back-buffer height in pixels.
    *****************************************************************************************/
    void Camera2D::SetViewportSize(int width, int height)
    {
        if (width > 0 && height > 0)
        {
            aspect = static_cast<float>(width) / static_cast<float>(height);
        }
    }

    /*****************************************************************************************
      \brief Set vertical size of the orthographic view volume, clamped to a sane range.
      \param verticalUnits Desired vertical extent in world units.
    *****************************************************************************************/
    void Camera2D::SetViewHeight(float verticalUnits)
    {
        constexpr float kMinHeight = 0.1f;
        constexpr float kMaxHeight = 10.0f;
        viewHeight = std::clamp(verticalUnits, kMinHeight, kMaxHeight);
    }

    /*****************************************************************************************
      \brief Move the camera focus directly to a world-space position.
      \param worldPosition Target focus point (camera centers on this).
    *****************************************************************************************/
    void Camera2D::SnapTo(const glm::vec2& worldPosition)
    {
        focus = worldPosition;
    }

    /*****************************************************************************************
      \brief Build the view matrix that translates world by -focus.
      \return glm::mat4 View matrix.
    *****************************************************************************************/
    glm::mat4 Camera2D::ViewMatrix() const
    {
        return glm::translate(glm::mat4(1.0f), glm::vec3(-focus.x, -focus.y, 0.0f));
    }

    /*****************************************************************************************
      \brief Build an orthographic projection from viewHeight and aspect.
      \return glm::mat4 Projection matrix (near=-1, far=+1).
    *****************************************************************************************/
    glm::mat4 Camera2D::ProjectionMatrix() const
    {
        const float halfHeight = viewHeight * 0.5f;
        const float halfWidth = halfHeight * aspect;
        return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, -1.0f, 1.0f);
    }

    /*****************************************************************************************
      \brief Compute Projection * View for convenience.
      \return glm::mat4 View-projection matrix.
    *****************************************************************************************/
    glm::mat4 Camera2D::ViewProjectionMatrix() const
    {
        return ProjectionMatrix() * ViewMatrix();
    }

} // namespace gfx
