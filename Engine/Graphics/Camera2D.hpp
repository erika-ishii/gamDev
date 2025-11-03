#pragma once

#include <glm/glm.hpp>

namespace gfx {

    class Camera2D {
    public:
        void SetViewportSize(int width, int height);
        void SetViewHeight(float verticalUnits);
        void SnapTo(const glm::vec2& worldPosition);

        [[nodiscard]] const glm::vec2& Position() const { return focus; }
        [[nodiscard]] float ViewHeight() const { return viewHeight; }
        [[nodiscard]] float AspectRatio() const { return aspect; }

        [[nodiscard]] glm::mat4 ViewMatrix() const;
        [[nodiscard]] glm::mat4 ProjectionMatrix() const;
        [[nodiscard]] glm::mat4 ViewProjectionMatrix() const;

    private:
        glm::vec2 focus{ 0.0f, 0.0f };
        float viewHeight{ 1.5f };
        float aspect{ 16.0f / 9.0f };
    };

} // namespace gfx