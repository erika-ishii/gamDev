/*********************************************************************************************
 \file      EditorGizmo.h
 \par       SofaSpuds
 \author    yimo.kong (yimo.kong@digipen.edu) - Primary Author, 100%
 \brief     Declares the in-editor transform gizmo API for manipulating the selected object.
 \details   Responsibilities:
            - Exposes a small interface to:
                * Query whether the gizmo is currently active (being dragged).
                * Describe the viewport rectangle in screen-space where the gizmo is rendered.
                * Select and query the current transform mode (Translate, Rotate, Scale).
                * Render and update the transform gizmo for the currently selected GOC.
            - Gizmo functions are now available in both debug and release builds so editor
              features work consistently regardless of configuration.
            - The implementation lives in EditorGizmo.cpp.
 \copyright
            All content ? 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include <glm/mat4x4.hpp>

namespace Framework {
    namespace editor {

        /*************************************************************************************
         \brief Returns true if any gizmo part is currently being dragged/used.
         \details
            - Useful for editor tools to know when the user is actively manipulating
              transforms so they can avoid conflicting mouse interactions.
        *************************************************************************************/
        bool IsGizmoActive();

        /*************************************************************************************
         \struct ViewportRect
         \brief Describes the on-screen rectangle where the game view is rendered.
         \details
            - All gizmo drawing and mouse hit-testing is performed relative to this region.
            - Coordinates are in screen-space pixels (top-left origin).
        *************************************************************************************/
        struct ViewportRect {
            float x{ 0.0f };       ///< Top-left X position in screen-space.
            float y{ 0.0f };       ///< Top-left Y position in screen-space.
            float width{ 0.0f };   ///< Width of the viewport in pixels.
            float height{ 0.0f };  ///< Height of the viewport in pixels.
        };

        /*************************************************************************************
         \enum EditorTransformMode
         \brief Represents the current type of transform operation for the gizmo.
        *************************************************************************************/
        enum class EditorTransformMode {
            Translate, ///< Move the object (X/Y axes & center handle).
            Rotate,    ///< Rotate the object around its origin.
            Scale      ///< Scale the object (axis & uniform scale handles).
        };

        /*************************************************************************************
         \brief Get the current active transform mode (Translate / Rotate / Scale).
        *************************************************************************************/
        EditorTransformMode GetCurrentTransformMode();

        /*************************************************************************************
         \brief Set the active transform mode used by the editor gizmo.
         \param mode The new transform mode to apply.
        *************************************************************************************/
        void SetCurrentTransformMode(EditorTransformMode mode);

        /*************************************************************************************
         \brief Convert a transform mode enum to a human-readable label.
         \param mode Transform mode to describe.
         \return "Translate", "Rotate", or "Scale".
        *************************************************************************************/
        const char* TransformModeLabel(EditorTransformMode mode);

        /*************************************************************************************
         \brief Render and update the transform gizmo for the selected object.
         \param view         Camera view matrix used to draw the game world.
         \param projection   Projection matrix used for the game world.
         \param viewportRect Screen-space rectangle of the game viewport.
         \details
            - Handles hover detection, drag behavior, and drawing of axis/rotation handles.
            - Applies the resulting transform back to the selected object's TransformComponent.
        *************************************************************************************/
        void RenderTransformGizmoForSelection(const glm::mat4& view,
            const glm::mat4& projection,
            const ViewportRect& viewportRect);

    } // namespace editor
} // namespace Framework
