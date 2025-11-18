/*********************************************************************************************
 \file      Camera2D.hpp
 \par       SofaSpuds
 \author    yimo.kong ( yimo.kong@digipen.edu) - Primary Author, 100%
 \brief     Orthographic 2D camera with focus, view height, and aspect-aware projection.
 \details   Tracks a world-space focus point and builds view/projection matrices sized by
            viewHeight and aspect (set from the viewport). Exposes the combined matrix for
            convenience in 2D render pipelines.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once

#include <glm/glm.hpp>

namespace gfx {

    /**
     * \class Camera2D
     * \brief Simple orthographic camera for 2D scenes.
     */
    class Camera2D {
    public:
        // Update aspect ratio from the current back-buffer size.
        void SetViewportSize(int width, int height);

        //Set vertical size of the ortho frustum (clamped in the .cpp).
        void SetViewHeight(float verticalUnits);

        // Center the camera on a world-space position.
        void SnapTo(const glm::vec2& worldPosition);

        // Current world-space focus.
        [[nodiscard]] const glm::vec2& Position() const { return focus; }

        // Current vertical size of the view volume.
        [[nodiscard]] float ViewHeight() const { return viewHeight; }

        // Current aspect ratio (width / height).
        [[nodiscard]] float AspectRatio() const { return aspect; }

        // View matrix translating by -focus.
        [[nodiscard]] glm::mat4 ViewMatrix() const;

        // Orthographic projection from viewHeight and aspect.
        [[nodiscard]] glm::mat4 ProjectionMatrix() const;

        // Projection * View.
        [[nodiscard]] glm::mat4 ViewProjectionMatrix() const;

    private:
        glm::vec2 focus{ 0.0f, 0.0f };
        float     viewHeight{ 1.5f };
        float     aspect{ 16.0f / 9.0f };
    };

} // namespace gfx
