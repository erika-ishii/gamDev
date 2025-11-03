#include "Camera2D.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace gfx {

    void Camera2D::SetViewportSize(int width, int height)
    {
        if (width > 0 && height > 0)
        {
            aspect = static_cast<float>(width) / static_cast<float>(height);
        }
    }

    void Camera2D::SetViewHeight(float verticalUnits)
    {
        constexpr float kMinHeight = 0.1f;
        constexpr float kMaxHeight = 10.0f;
        viewHeight = std::clamp(verticalUnits, kMinHeight, kMaxHeight);
    }

    void Camera2D::SnapTo(const glm::vec2& worldPosition)
    {
        focus = worldPosition;
    }

    glm::mat4 Camera2D::ViewMatrix() const
    {
        return glm::translate(glm::mat4(1.0f), glm::vec3(-focus.x, -focus.y, 0.0f));
    }

    glm::mat4 Camera2D::ProjectionMatrix() const
    {
        const float halfHeight = viewHeight * 0.5f;
        const float halfWidth = halfHeight * aspect;
        return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, -1.0f, 1.0f);
    }

    glm::mat4 Camera2D::ViewProjectionMatrix() const
    {
        return ProjectionMatrix() * ViewMatrix();
    }

} // namespace gfx