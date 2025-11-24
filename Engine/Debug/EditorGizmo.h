#pragma once

#include <glm/mat4x4.hpp>

namespace Framework {
    namespace editor {
        bool IsGizmoActive();
        struct ViewportRect {
            float x{ 0.0f };
            float y{ 0.0f };
            float width{ 0.0f };
            float height{ 0.0f };
        };

        enum class EditorTransformMode { Translate, Rotate, Scale };

#if defined(_DEBUG) || defined(EDITOR)
        EditorTransformMode GetCurrentTransformMode();
        void SetCurrentTransformMode(EditorTransformMode mode);
        const char* TransformModeLabel(EditorTransformMode mode);
        void RenderTransformGizmoForSelection(const glm::mat4& view,
            const glm::mat4& projection,
            const ViewportRect& viewportRect);
#else
        inline EditorTransformMode GetCurrentTransformMode() { return EditorTransformMode::Translate; }
        inline void SetCurrentTransformMode(EditorTransformMode) {}
        inline const char* TransformModeLabel(EditorTransformMode mode)
        {
            switch (mode) {
            case EditorTransformMode::Rotate: return "Rotate";
            case EditorTransformMode::Scale: return "Scale";
            case EditorTransformMode::Translate:
            default: return "Translate";
            }
        }
        inline void RenderTransformGizmoForSelection(const glm::mat4&, const glm::mat4&, const ViewportRect&) {}
#endif

    } // namespace editor
} // namespace Framework