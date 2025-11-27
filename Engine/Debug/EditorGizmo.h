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
            - In non-editor builds, all gizmo functions are compiled down to no-ops so the
              game runtime remains lightweight and free of editor-only dependencies.
            - The implementation lives in EditorGizmo.cpp and is guarded by _DEBUG/EDITOR
              so that production builds do not include ImGui or editor behavior.
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
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

#if defined(_DEBUG) || defined(EDITOR)

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

#else  // !(_DEBUG || EDITOR) — runtime builds get no-op versions

        /*************************************************************************************
         \brief In non-editor builds, always returns Translate as a safe default.
        *************************************************************************************/
        inline EditorTransformMode GetCurrentTransformMode() { return EditorTransformMode::Translate; }

        /*************************************************************************************
         \brief In non-editor builds, this is a no-op setter for the transform mode.
        *************************************************************************************/
        inline void SetCurrentTransformMode(EditorTransformMode) {}

        /*************************************************************************************
         \brief In non-editor builds, still returns a human-readable label (for logging/UI).
         \param mode Transform mode to describe.
         \return "Translate", "Rotate", or "Scale".
        *************************************************************************************/
        inline const char* TransformModeLabel(EditorTransformMode mode)
        {
            switch (mode) {
            case EditorTransformMode::Rotate:    return "Rotate";
            case EditorTransformMode::Scale:     return "Scale";
            case EditorTransformMode::Translate:
            default:                             return "Translate";
            }
        }

        /*************************************************************************************
         \brief In non-editor builds, the gizmo render function is a no-op.
         \details
            - Keeps the runtime free of ImGui/editor dependencies while preserving calls.
        *************************************************************************************/
        inline void RenderTransformGizmoForSelection(const glm::mat4&, const glm::mat4&, const ViewportRect&) {}

#endif // defined(_DEBUG) || defined(EDITOR)

    } // namespace editor
} // namespace Framework
