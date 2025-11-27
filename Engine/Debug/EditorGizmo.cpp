/*********************************************************************************************
 \file      EditorGizmo.cpp
 \par       SofaSpuds
 \author    yimo.kong (yimo.kong@digipen.edu) - Primary Author, 100%
 \brief     Implements the in-editor transform gizmo for translating, rotating, and scaling
            the currently selected GameObject in the game viewport.
 \details   Responsibilities:
            - Draws world-space transform handles (arrows, squares, rotation ring) in screen-
              space using ImGui’s foreground draw list.
            - Converts mouse positions between screen-space and world-space using the current
              view/projection matrices and a viewport rectangle.
            - Supports three transform modes:
                * Translate (center box + X/Y axes)
                * Rotate (outer ring)
                * Scale (axis handles + uniform scale handle)
            - Tracks drag state across frames with a persistent GizmoState.
            - Integrates with the UndoStack by capturing a TransformSnapshot when a drag
              begins and recording the delta when the mouse is released.
            - Only compiled in debug/editor builds (guarded by _DEBUG/EDITOR macros).
 *********************************************************************************************/

#include "EditorGizmo.h"

#if defined(_DEBUG) || defined(EDITOR)

#include "Component/TransformComponent.h"
#include "Debug/Selection.h"
#include "Factory/Factory.h"
#include "Debug/UndoStack.h"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace Framework {
    namespace editor {

        namespace {

            /*************************************************************************************
             \enum GizmoPart
             \brief Identifies which part of the gizmo is currently hovered or being dragged.
            *************************************************************************************/
            enum class GizmoPart {
                None,
                TranslateXY,   ///< Center handle: move in X and Y simultaneously.
                TranslateX,    ///< Axis handle: move along local X axis.
                TranslateY,    ///< Axis handle: move along local Y axis.
                Rotate,        ///< Outer ring: rotate around object origin.
                ScaleX,        ///< Axis handle: scale along local X axis.
                ScaleY,        ///< Axis handle: scale along local Y axis.
                ScaleUniform   ///< Corner handle: uniform scaling on both axes.
            };

            /*************************************************************************************
             \struct GizmoState
             \brief Stores persistent state for an active gizmo drag (start transform, etc.).
            *************************************************************************************/
            struct GizmoState {
                GizmoPart activePart = GizmoPart::None; ///< Which part is currently active.
                glm::vec2 startPos{ 0.0f, 0.0f };       ///< Starting position at drag begin.
                glm::vec2 startScale{ 1.0f, 1.0f };     ///< Starting scale at drag begin.
                float     startRot = 0.0f;              ///< Starting rotation at drag begin.
                glm::vec2 grabWorld{ 0.0f, 0.0f };      ///< World-space mouse grab point.
                float     startAxisProjX = 0.0f;        ///< Projection of grab along local X axis.
                float     startAxisProjY = 0.0f;        ///< Projection of grab along local Y axis.
                float     startAngle = 0.0f;            ///< Angle from center at drag begin (rotate).
            };

            // Global, editor-only gizmo state and undo bookkeeping.
            GizmoState gGizmoState{};
            EditorTransformMode gCurrentTransformMode = EditorTransformMode::Translate;
            mygame::editor::TransformSnapshot gUndoStart{};
            Framework::GOCId gUndoObjectId = 0;
            bool gHasPendingUndo = false;

            /*************************************************************************************
             \brief Check if a screen-space mouse position is inside a viewport rectangle.
             \param rect  Viewport rectangle in screen-space coordinates.
             \param pos   Mouse position in screen-space.
             \return True if pos lies within rect.
            *************************************************************************************/
            bool MouseInRect(const ViewportRect& rect, const ImVec2& pos)
            {
                return pos.x >= rect.x && pos.x <= rect.x + rect.width &&
                    pos.y >= rect.y && pos.y <= rect.y + rect.height;
            }

            /*************************************************************************************
             \brief Project a 2D world-space point into screen-space inside a viewport.

             \param world  World-space position (x,y).
             \param view   View matrix.
             \param proj   Projection matrix.
             \param rect   Viewport rectangle in screen-space.

             \return ImVec2 representing the point in screen coordinates.
            *************************************************************************************/
            ImVec2 WorldToScreen(const glm::vec2& world,
                const glm::mat4& view,
                const glm::mat4& proj,
                const ViewportRect& rect)
            {
                glm::vec4 clip = proj * view * glm::vec4(world, 0.0f, 1.0f);
                if (clip.w != 0.0f)
                    clip /= clip.w;

                const float sx = rect.x + (clip.x * 0.5f + 0.5f) * rect.width;
                const float sy = rect.y + (1.0f - (clip.y * 0.5f + 0.5f)) * rect.height;
                return ImVec2(sx, sy);
            }

            /*************************************************************************************
             \brief Convert a screen-space point back into world-space using view/projection.

             \param screen Screen-space position (e.g., mouse cursor).
             \param view   View matrix.
             \param proj   Projection matrix.
             \param rect   Viewport rectangle in screen-space.

             \return World-space position corresponding to the given screen location.
            *************************************************************************************/
            glm::vec2 ScreenToWorld(const ImVec2& screen,
                const glm::mat4& view,
                const glm::mat4& proj,
                const ViewportRect& rect)
            {
                const float ndcX = ((screen.x - rect.x) / rect.width) * 2.0f - 1.0f;
                const float ndcY = 1.0f - ((screen.y - rect.y) / rect.height) * 2.0f;

                const glm::mat4 invVP = glm::inverse(proj * view);
                glm::vec4 world = invVP * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
                if (world.w != 0.0f)
                    world /= world.w;

                return glm::vec2(world.x, world.y);
            }

            /*************************************************************************************
             \brief Convert a desired on-screen pixel length into a world-space distance along
                    a given direction vector.

             \param origin        World-space origin of the gizmo.
             \param dir           Direction along which we want to measure.
             \param desiredPixels Desired on-screen length in pixels.
             \param view          View matrix.
             \param proj          Projection matrix.
             \param rect          Viewport rectangle in screen-space.

             \return World-space distance corresponding to desiredPixels along dir.
            *************************************************************************************/
            float PixelsToWorldAlong(const glm::vec2& origin,
                const glm::vec2& dir,
                float desiredPixels,
                const glm::mat4& view,
                const glm::mat4& proj,
                const ViewportRect& rect)
            {
                glm::vec2 normDir = dir;
                if (glm::length(normDir) <= 0.0001f)
                    return 0.0f;
                normDir = glm::normalize(normDir);

                const ImVec2 originScreen = WorldToScreen(origin, view, proj, rect);
                const ImVec2 offsetScreen = WorldToScreen(origin + normDir, view, proj, rect);
                const float pixelsPerUnit = std::sqrt(std::pow(offsetScreen.x - originScreen.x, 2.0f) +
                    std::pow(offsetScreen.y - originScreen.y, 2.0f));

                if (pixelsPerUnit <= 0.0001f)
                    return 0.0f;

                return desiredPixels / pixelsPerUnit;
            }

            /*************************************************************************************
             \brief Compute the shortest distance from a point to a finite line segment.

             \param p Query point (screen-space).
             \param a Segment start (screen-space).
             \param b Segment end (screen-space).

             \return Distance from p to segment [a,b] in pixels.
            *************************************************************************************/
            float DistanceToSegment(const ImVec2& p, const ImVec2& a, const ImVec2& b)
            {
                const float vx = b.x - a.x;
                const float vy = b.y - a.y;
                const float wx = p.x - a.x;
                const float wy = p.y - a.y;

                const float c1 = vx * wx + vy * wy;
                const float c2 = vx * vx + vy * vy;
                const float t = c2 > 0.0f ? std::clamp(c1 / c2, 0.0f, 1.0f) : 0.0f;

                const float projX = a.x + t * vx;
                const float projY = a.y + t * vy;
                const float dx = p.x - projX;
                const float dy = p.y - projY;
                return std::sqrt(dx * dx + dy * dy);
            }

            /*************************************************************************************
             \brief Brighten a base color when the gizmo part is active/hovered.

             \param base   Base ImU32 color.
             \param active True if the gizmo part is currently active or hovered.

             \return Adjusted color with higher brightness when active is true.
            *************************************************************************************/
            ImU32 ColorWithActive(ImU32 base, bool active)
            {
                if (!active)
                    return base;
                ImVec4 c = ImGui::ColorConvertU32ToFloat4(base);
                c.x = std::min(1.0f, c.x + 0.25f);
                c.y = std::min(1.0f, c.y + 0.25f);
                c.z = std::min(1.0f, c.z + 0.25f);
                return ImGui::GetColorU32(c);
            }

        } // namespace (internal helpers)

        /*************************************************************************************
         \brief Returns true if any gizmo part is currently being dragged/active.
        *************************************************************************************/
        bool IsGizmoActive()
        {
            return gGizmoState.activePart != GizmoPart::None;
        }

        /*************************************************************************************
         \brief Get the current transform mode (Translate / Rotate / Scale).
        *************************************************************************************/
        EditorTransformMode GetCurrentTransformMode()
        {
            return gCurrentTransformMode;
        }

        /*************************************************************************************
         \brief Set the active transform mode used when interacting with the gizmo.
        *************************************************************************************/
        void SetCurrentTransformMode(EditorTransformMode mode)
        {
            gCurrentTransformMode = mode;
        }

        /*************************************************************************************
         \brief Convert a transform mode enum into a user-facing label.
         \return "Translate", "Rotate", or "Scale".
        *************************************************************************************/
        const char* TransformModeLabel(EditorTransformMode mode)
        {
            switch (mode) {
            case EditorTransformMode::Rotate:    return "Rotate";
            case EditorTransformMode::Scale:     return "Scale";
            case EditorTransformMode::Translate:
            default:                             return "Translate";
            }
        }

        /*************************************************************************************
         \brief Render and update the transform gizmo for the currently selected object.

         \param view         View matrix of the game camera.
         \param projection   Projection matrix used for rendering the game world.
         \param viewportRect Rectangle describing the game viewport region in screen-space.

         \details
            - Early-out if there is no selection, no transform component, or viewport is invalid.
            - Computes world→screen projection for gizmo origin and axes.
            - Detects hover for each gizmo part based on mouse position.
            - On mouse press, captures the starting transform and sets active gizmo part.
            - While mouse is held, applies translation/rotation/scale to the TransformComponent.
            - On mouse release, pushes an Undo operation recording the transform delta.
            - Draws gizmo lines, squares, and ring using ImGui’s foreground draw list.
        *************************************************************************************/
        void RenderTransformGizmoForSelection(const glm::mat4& view,
            const glm::mat4& projection,
            const ViewportRect& viewportRect)
        {
            if (!FACTORY)
                return;
            if (viewportRect.width <= 1.0f || viewportRect.height <= 1.0f)
                return;

            const auto selectedId = mygame::GetSelectedObjectId();
            if (selectedId == 0)
                return;

            Framework::GOC* obj = FACTORY->GetObjectWithId(selectedId);
            if (!obj)
                return;

            auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                Framework::ComponentTypeId::CT_TransformComponent);
            if (!tr)
                return;

            const glm::vec2 position(tr->x, tr->y);
            const float     rotation = tr->rot;
            const glm::vec2 scale(tr->scaleX, tr->scaleY);

            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            ImGuiIO& io = ImGui::GetIO();
            const ImVec2 mouse = io.MousePos;
            const bool mouseInViewport = MouseInRect(viewportRect, mouse);
           // const bool wantCapture = io.WantCaptureMouse; // (reserved; currently not used)

            const bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            const bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

            const ImVec2 originScreen = WorldToScreen(position, view, projection, viewportRect);

            // Local axes from rotation
            const glm::vec2 axisX(std::cos(rotation), std::sin(rotation));
            const glm::vec2 axisY(-std::sin(rotation), std::cos(rotation));

            // Visual parameters (in pixels)
            constexpr float axisPixels = 72.0f;
            constexpr float handleBox = 8.0f;
            constexpr float axisHit = 10.0f;
            constexpr float ringPixels = 60.0f;
            constexpr float ringHit = 7.0f;
            constexpr float minScale = 0.01f;

            // Convert axis lengths from pixels to world units
            const float axisWorldX = PixelsToWorldAlong(position, axisX, axisPixels, view, projection, viewportRect);
            const float axisWorldY = PixelsToWorldAlong(position, axisY, axisPixels, view, projection, viewportRect);

            const ImVec2 xEnd = WorldToScreen(position + axisX * axisWorldX, view, projection, viewportRect);
            const ImVec2 yEnd = WorldToScreen(position + axisY * axisWorldY, view, projection, viewportRect);

            glm::vec2 uniformDir = axisX + axisY;
            if (glm::length(uniformDir) <= 0.0001f)
                uniformDir = glm::vec2(1.0f, 0.0f);
            uniformDir = glm::normalize(uniformDir);
            const float uniformWorld = PixelsToWorldAlong(position, uniformDir, axisPixels * 0.65f,
                view, projection, viewportRect);
            const ImVec2 uniformEnd = WorldToScreen(position + uniformDir * uniformWorld,
                view, projection, viewportRect);

            const float ringWorld = PixelsToWorldAlong(position, axisX, ringPixels, view, projection, viewportRect);

            const float distToCenter = std::sqrt(std::pow(mouse.x - originScreen.x, 2.0f) +
                std::pow(mouse.y - originScreen.y, 2.0f));

            // Hover detection for each gizmo part
            const bool hoverTranslateX = (gCurrentTransformMode == EditorTransformMode::Translate) &&
                (DistanceToSegment(mouse, originScreen, xEnd) <= axisHit);
            const bool hoverTranslateY = (gCurrentTransformMode == EditorTransformMode::Translate) &&
                (DistanceToSegment(mouse, originScreen, yEnd) <= axisHit);
            const bool hoverTranslateCenter = (gCurrentTransformMode == EditorTransformMode::Translate) &&
                (std::fabs(mouse.x - originScreen.x) <= handleBox * 1.3f) &&
                (std::fabs(mouse.y - originScreen.y) <= handleBox * 1.3f);

            const bool hoverScaleX = (gCurrentTransformMode == EditorTransformMode::Scale) &&
                (std::sqrt(std::pow(mouse.x - xEnd.x, 2.0f) +
                    std::pow(mouse.y - xEnd.y, 2.0f)) <= handleBox * 1.8f);
            const bool hoverScaleY = (gCurrentTransformMode == EditorTransformMode::Scale) &&
                (std::sqrt(std::pow(mouse.x - yEnd.x, 2.0f) +
                    std::pow(mouse.y - yEnd.y, 2.0f)) <= handleBox * 1.8f);
            const bool hoverScaleUniform = (gCurrentTransformMode == EditorTransformMode::Scale) &&
                (std::sqrt(std::pow(mouse.x - uniformEnd.x, 2.0f) +
                    std::pow(mouse.y - uniformEnd.y, 2.0f)) <= handleBox * 1.8f);

            const bool hoverRotate = (gCurrentTransformMode == EditorTransformMode::Rotate) &&
                (std::fabs(distToCenter - ringPixels) <= ringHit);

            // Reset active part when mouse is not held
            if (!mouseDown && gGizmoState.activePart != GizmoPart::None)
                gGizmoState.activePart = GizmoPart::None;

            const bool  allowInteraction = mouseInViewport;
            const glm::vec2 mouseWorld = ScreenToWorld(mouse, view, projection, viewportRect);

            if (mouseReleased)
                gGizmoState.activePart = GizmoPart::None;

            // Begin interaction: determine which part we grabbed this frame.
            if (gGizmoState.activePart == GizmoPart::None && mouseClicked && allowInteraction)
            {
                if (hoverTranslateCenter)
                {
                    gGizmoState.activePart = GizmoPart::TranslateXY;
                }
                else if (hoverTranslateX)
                {
                    gGizmoState.activePart = GizmoPart::TranslateX;
                }
                else if (hoverTranslateY)
                {
                    gGizmoState.activePart = GizmoPart::TranslateY;
                }
                else if (hoverRotate)
                {
                    gGizmoState.activePart = GizmoPart::Rotate;
                    gGizmoState.startAngle = std::atan2(mouseWorld.y - position.y,
                        mouseWorld.x - position.x);
                }
                else if (hoverScaleX)
                {
                    gGizmoState.activePart = GizmoPart::ScaleX;
                }
                else if (hoverScaleY)
                {
                    gGizmoState.activePart = GizmoPart::ScaleY;
                }
                else if (hoverScaleUniform)
                {
                    gGizmoState.activePart = GizmoPart::ScaleUniform;
                }

                // If we started a drag, capture starting transform and schedule undo.
                if (gGizmoState.activePart != GizmoPart::None)
                {
                    gUndoStart = mygame::editor::CaptureTransformSnapshot(*obj);
                    gUndoObjectId = obj->GetId();
                    gHasPendingUndo = true;

                    gGizmoState.startPos = position;
                    gGizmoState.startScale = scale;
                    gGizmoState.startRot = rotation;
                    gGizmoState.grabWorld = mouseWorld;
                    gGizmoState.startAxisProjX = glm::dot(mouseWorld - position, axisX);
                    gGizmoState.startAxisProjY = glm::dot(mouseWorld - position, axisY);
                }
            }

            // While dragging: apply transforms based on active gizmo part.
            if (gGizmoState.activePart != GizmoPart::None && mouseDown)
            {
                switch (gGizmoState.activePart)
                {
                case GizmoPart::TranslateXY:
                {
                    const glm::vec2 delta = mouseWorld - gGizmoState.grabWorld;
                    tr->x = gGizmoState.startPos.x + delta.x;
                    tr->y = gGizmoState.startPos.y + delta.y;
                    break;
                }
                case GizmoPart::TranslateX:
                {
                    const float delta = glm::dot(mouseWorld - gGizmoState.grabWorld, axisX);
                    const glm::vec2 move = axisX * delta;
                    tr->x = gGizmoState.startPos.x + move.x;
                    tr->y = gGizmoState.startPos.y + move.y;
                    break;
                }
                case GizmoPart::TranslateY:
                {
                    const float delta = glm::dot(mouseWorld - gGizmoState.grabWorld, axisY);
                    const glm::vec2 move = axisY * delta;
                    tr->x = gGizmoState.startPos.x + move.x;
                    tr->y = gGizmoState.startPos.y + move.y;
                    break;
                }
                case GizmoPart::Rotate:
                {
                    const float currentAngle =
                        std::atan2(mouseWorld.y - position.y, mouseWorld.x - position.x);
                    const float delta = currentAngle - gGizmoState.startAngle;
                    tr->rot = gGizmoState.startRot + delta;
                    break;
                }
                case GizmoPart::ScaleX:
                {
                    const float proj = glm::dot(mouseWorld - position, axisX);
                    const float delta = proj - gGizmoState.startAxisProjX;
                    tr->scaleX = std::max(minScale, gGizmoState.startScale.x + delta);
                    break;
                }
                case GizmoPart::ScaleY:
                {
                    const float proj = glm::dot(mouseWorld - position, axisY);
                    const float delta = proj - gGizmoState.startAxisProjY;
                    tr->scaleY = std::max(minScale, gGizmoState.startScale.y + delta);
                    break;
                }
                case GizmoPart::ScaleUniform:
                {
                    const float projX = glm::dot(mouseWorld - position, axisX);
                    const float projY = glm::dot(mouseWorld - position, axisY);
                    const float startAvg = 0.5f * (gGizmoState.startAxisProjX + gGizmoState.startAxisProjY);
                    const float currentAvg = 0.5f * (projX + projY);
                    const float delta = currentAvg - startAvg;
                    const float newScale = std::max(minScale, gGizmoState.startScale.x + delta);
                    tr->scaleX = newScale;
                    tr->scaleY = newScale;
                    break;
                }
                default:
                    break;
                }
            }

            // On mouse release: if we started an undo, finalize it.
            if (!mouseDown && gHasPendingUndo)
            {
                if (FACTORY && gUndoObjectId != 0)
                {
                    if (auto* target = FACTORY->GetObjectWithId(gUndoObjectId))
                    {
                        mygame::editor::RecordTransformChange(*target, gUndoStart);
                    }
                }
                gHasPendingUndo = false;
                gUndoObjectId = 0;
            }

            // ------------------------------
            // Visual (drawing) configuration
            // ------------------------------
            const ImU32 translateXColor = ImGui::GetColorU32(ImVec4(0.94f, 0.32f, 0.32f, 1.0f));
            const ImU32 translateYColor = ImGui::GetColorU32(ImVec4(0.32f, 0.86f, 0.32f, 1.0f));
            const ImU32 scaleColor = ImGui::GetColorU32(ImVec4(0.26f, 0.7f, 0.95f, 1.0f));
            const ImU32 rotationColor = ImGui::GetColorU32(ImVec4(0.95f, 0.78f, 0.26f, 1.0f));

            // Draw gizmo based on current mode.
            if (gCurrentTransformMode == EditorTransformMode::Translate)
            {
                const ImU32 xColor = ColorWithActive(
                    translateXColor,
                    hoverTranslateX || gGizmoState.activePart == GizmoPart::TranslateX);
                const ImU32 yColor = ColorWithActive(
                    translateYColor,
                    hoverTranslateY || gGizmoState.activePart == GizmoPart::TranslateY);

                // X axis arrow
                drawList->AddLine(originScreen, xEnd, xColor, 3.0f);
                drawList->AddTriangleFilled(
                    ImVec2(xEnd.x, xEnd.y),
                    ImVec2(xEnd.x - axisHit, xEnd.y - axisHit * 0.75f),
                    ImVec2(xEnd.x - axisHit, xEnd.y + axisHit * 0.75f),
                    xColor);

                // Y axis arrow
                drawList->AddLine(originScreen, yEnd, yColor, 3.0f);
                drawList->AddTriangleFilled(
                    ImVec2(yEnd.x, yEnd.y),
                    ImVec2(yEnd.x - axisHit * 0.75f, yEnd.y - axisHit),
                    ImVec2(yEnd.x + axisHit * 0.75f, yEnd.y - axisHit),
                    yColor);

                // Center box for TranslateXY
                const ImU32 centerColor = ColorWithActive(
                    ImGui::GetColorU32(ImGuiCol_TextSelectedBg),
                    hoverTranslateCenter || gGizmoState.activePart == GizmoPart::TranslateXY);
                drawList->AddRectFilled(
                    ImVec2(originScreen.x - handleBox, originScreen.y - handleBox),
                    ImVec2(originScreen.x + handleBox, originScreen.y + handleBox),
                    centerColor);
            }
            else if (gCurrentTransformMode == EditorTransformMode::Scale)
            {
                const ImU32 xColor = ColorWithActive(
                    scaleColor,
                    hoverScaleX || gGizmoState.activePart == GizmoPart::ScaleX);
                const ImU32 yColor = ColorWithActive(
                    scaleColor,
                    hoverScaleY || gGizmoState.activePart == GizmoPart::ScaleY);
                const ImU32 uColor = ColorWithActive(
                    scaleColor,
                    hoverScaleUniform || gGizmoState.activePart == GizmoPart::ScaleUniform);

                // X scale handle
                drawList->AddLine(originScreen, xEnd, xColor, 2.5f);
                drawList->AddRectFilled(
                    ImVec2(xEnd.x - handleBox, xEnd.y - handleBox),
                    ImVec2(xEnd.x + handleBox, xEnd.y + handleBox),
                    xColor);

                // Y scale handle
                drawList->AddLine(originScreen, yEnd, yColor, 2.5f);
                drawList->AddRectFilled(
                    ImVec2(yEnd.x - handleBox, yEnd.y - handleBox),
                    ImVec2(yEnd.x + handleBox, yEnd.y + handleBox),
                    yColor);

                // Uniform scale handle
                drawList->AddRectFilled(
                    ImVec2(uniformEnd.x - handleBox * 1.1f, uniformEnd.y - handleBox * 1.1f),
                    ImVec2(uniformEnd.x + handleBox * 1.1f, uniformEnd.y + handleBox * 1.1f),
                    uColor);
            }
            else if (gCurrentTransformMode == EditorTransformMode::Rotate)
            {
                const ImU32 ringColor = ColorWithActive(
                    rotationColor,
                    hoverRotate || gGizmoState.activePart == GizmoPart::Rotate);

                drawList->AddCircle(originScreen, ringPixels, ringColor, 64, 2.5f);

                if (ringWorld > 0.0f)
                {
                    const ImVec2 pointer =
                        WorldToScreen(position + axisX * ringWorld, view, projection, viewportRect);
                    drawList->AddLine(originScreen, pointer, ringColor, 2.0f);
                }
            }

            // Transform mode label in the corner of the viewport.
            drawList->AddText(
                ImVec2(viewportRect.x + 8.0f, viewportRect.y + 8.0f),
                ImGui::GetColorU32(ImGuiCol_Text),
                TransformModeLabel(gCurrentTransformMode));
        }

    } // namespace editor
} // namespace Framework

#endif // defined(_DEBUG) || defined(EDITOR)
