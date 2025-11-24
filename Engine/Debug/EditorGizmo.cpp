#include "EditorGizmo.h"

#if defined(_DEBUG) || defined(EDITOR)

#include "Component/TransformComponent.h"
#include "Debug/Selection.h"
#include "Factory/Factory.h"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Debug/UndoStack.h"

#include <algorithm>
#include <cmath>

namespace Framework {
    namespace editor {

        namespace {

            enum class GizmoPart {
                None,
                TranslateXY,
                TranslateX,
                TranslateY,
                Rotate,
                ScaleX,
                ScaleY,
                ScaleUniform
            };

            struct GizmoState {
                GizmoPart activePart = GizmoPart::None;
                glm::vec2 startPos{ 0.0f, 0.0f };
                glm::vec2 startScale{ 1.0f, 1.0f };
                float startRot = 0.0f;
                glm::vec2 grabWorld{ 0.0f, 0.0f };
                float startAxisProjX = 0.0f;
                float startAxisProjY = 0.0f;
                float startAngle = 0.0f;
            };

            GizmoState gGizmoState{};
            EditorTransformMode gCurrentTransformMode = EditorTransformMode::Translate;
            mygame::editor::TransformSnapshot gUndoStart{};
            Framework::GOCId gUndoObjectId = 0;
            bool gHasPendingUndo = false;
            

            bool MouseInRect(const ViewportRect& rect, const ImVec2& pos)
            {
                return pos.x >= rect.x && pos.x <= rect.x + rect.width &&
                    pos.y >= rect.y && pos.y <= rect.y + rect.height;
            }

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

        } // namespace
        bool IsGizmoActive()
        {
            return gGizmoState.activePart != GizmoPart::None;
        }

        EditorTransformMode GetCurrentTransformMode()
        {
            return gCurrentTransformMode;
        }

        void SetCurrentTransformMode(EditorTransformMode mode)
        {
            gCurrentTransformMode = mode;
        }

        const char* TransformModeLabel(EditorTransformMode mode)
        {
            switch (mode) {
            case EditorTransformMode::Rotate: return "Rotate";
            case EditorTransformMode::Scale: return "Scale";
            case EditorTransformMode::Translate:
            default: return "Translate";
            }
        }

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
            const float rotation = tr->rot;
            const glm::vec2 scale(tr->scaleX, tr->scaleY);

            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            ImGuiIO& io = ImGui::GetIO();
            const ImVec2 mouse = io.MousePos;
            const bool mouseInViewport = MouseInRect(viewportRect, mouse);
            const bool wantCapture = io.WantCaptureMouse;

            const bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            const bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

            const ImVec2 originScreen = WorldToScreen(position, view, projection, viewportRect);
            const glm::vec2 axisX(std::cos(rotation), std::sin(rotation));
            const glm::vec2 axisY(-std::sin(rotation), std::cos(rotation));

            constexpr float axisPixels = 72.0f;
            constexpr float handleBox = 8.0f;
            constexpr float axisHit = 10.0f;
            constexpr float ringPixels = 60.0f;
            constexpr float ringHit = 7.0f;
            constexpr float minScale = 0.01f;

            const float axisWorldX = PixelsToWorldAlong(position, axisX, axisPixels, view, projection, viewportRect);
            const float axisWorldY = PixelsToWorldAlong(position, axisY, axisPixels, view, projection, viewportRect);

            const ImVec2 xEnd = WorldToScreen(position + axisX * axisWorldX, view, projection, viewportRect);
            const ImVec2 yEnd = WorldToScreen(position + axisY * axisWorldY, view, projection, viewportRect);

            glm::vec2 uniformDir = axisX + axisY;
            if (glm::length(uniformDir) <= 0.0001f)
                uniformDir = glm::vec2(1.0f, 0.0f);
            uniformDir = glm::normalize(uniformDir);
            const float uniformWorld = PixelsToWorldAlong(position, uniformDir, axisPixels * 0.65f, view, projection, viewportRect);
            const ImVec2 uniformEnd = WorldToScreen(position + uniformDir * uniformWorld, view, projection, viewportRect);

            const float ringWorld = PixelsToWorldAlong(position, axisX, ringPixels, view, projection, viewportRect);

            const float distToCenter = std::sqrt(std::pow(mouse.x - originScreen.x, 2.0f) +
                std::pow(mouse.y - originScreen.y, 2.0f));

            const bool hoverTranslateX = (gCurrentTransformMode == EditorTransformMode::Translate) &&
                (DistanceToSegment(mouse, originScreen, xEnd) <= axisHit);
            const bool hoverTranslateY = (gCurrentTransformMode == EditorTransformMode::Translate) &&
                (DistanceToSegment(mouse, originScreen, yEnd) <= axisHit);
            const bool hoverTranslateCenter = (gCurrentTransformMode == EditorTransformMode::Translate) &&
                (std::fabs(mouse.x - originScreen.x) <= handleBox * 1.3f) &&
                (std::fabs(mouse.y - originScreen.y) <= handleBox * 1.3f);

            const bool hoverScaleX = (gCurrentTransformMode == EditorTransformMode::Scale) &&
                (std::sqrt(std::pow(mouse.x - xEnd.x, 2.0f) + std::pow(mouse.y - xEnd.y, 2.0f)) <= handleBox * 1.8f);
            const bool hoverScaleY = (gCurrentTransformMode == EditorTransformMode::Scale) &&
                (std::sqrt(std::pow(mouse.x - yEnd.x, 2.0f) + std::pow(mouse.y - yEnd.y, 2.0f)) <= handleBox * 1.8f);
            const bool hoverScaleUniform = (gCurrentTransformMode == EditorTransformMode::Scale) &&
                (std::sqrt(std::pow(mouse.x - uniformEnd.x, 2.0f) + std::pow(mouse.y - uniformEnd.y, 2.0f)) <= handleBox * 1.8f);

            const bool hoverRotate = (gCurrentTransformMode == EditorTransformMode::Rotate) &&
                (std::fabs(distToCenter - ringPixels) <= ringHit);

            if (!mouseDown && gGizmoState.activePart != GizmoPart::None)
                gGizmoState.activePart = GizmoPart::None;

            const bool allowInteraction = mouseInViewport;
            const glm::vec2 mouseWorld = ScreenToWorld(mouse, view, projection, viewportRect);

            if (mouseReleased)
                gGizmoState.activePart = GizmoPart::None;

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
                    gGizmoState.startAngle = std::atan2(mouseWorld.y - position.y, mouseWorld.x - position.x);
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
                    const float currentAngle = std::atan2(mouseWorld.y - position.y, mouseWorld.x - position.x);
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

            const ImU32 translateXColor = ImGui::GetColorU32(ImVec4(0.94f, 0.32f, 0.32f, 1.0f));
            const ImU32 translateYColor = ImGui::GetColorU32(ImVec4(0.32f, 0.86f, 0.32f, 1.0f));
            const ImU32 scaleColor = ImGui::GetColorU32(ImVec4(0.26f, 0.7f, 0.95f, 1.0f));
            const ImU32 rotationColor = ImGui::GetColorU32(ImVec4(0.95f, 0.78f, 0.26f, 1.0f));

            if (gCurrentTransformMode == EditorTransformMode::Translate)
            {
                const ImU32 xColor = ColorWithActive(translateXColor, hoverTranslateX || gGizmoState.activePart == GizmoPart::TranslateX);
                const ImU32 yColor = ColorWithActive(translateYColor, hoverTranslateY || gGizmoState.activePart == GizmoPart::TranslateY);

                drawList->AddLine(originScreen, xEnd, xColor, 3.0f);
                drawList->AddTriangleFilled(
                    ImVec2(xEnd.x, xEnd.y),
                    ImVec2(xEnd.x - axisHit, xEnd.y - axisHit * 0.75f),
                    ImVec2(xEnd.x - axisHit, xEnd.y + axisHit * 0.75f), xColor);

                drawList->AddLine(originScreen, yEnd, yColor, 3.0f);
                drawList->AddTriangleFilled(
                    ImVec2(yEnd.x, yEnd.y),
                    ImVec2(yEnd.x - axisHit * 0.75f, yEnd.y - axisHit),
                    ImVec2(yEnd.x + axisHit * 0.75f, yEnd.y - axisHit), yColor);

                const ImU32 centerColor = ColorWithActive(ImGui::GetColorU32(ImGuiCol_TextSelectedBg),
                    hoverTranslateCenter || gGizmoState.activePart == GizmoPart::TranslateXY);
                drawList->AddRectFilled(
                    ImVec2(originScreen.x - handleBox, originScreen.y - handleBox),
                    ImVec2(originScreen.x + handleBox, originScreen.y + handleBox), centerColor);
            }
            else if (gCurrentTransformMode == EditorTransformMode::Scale)
            {
                const ImU32 xColor = ColorWithActive(scaleColor, hoverScaleX || gGizmoState.activePart == GizmoPart::ScaleX);
                const ImU32 yColor = ColorWithActive(scaleColor, hoverScaleY || gGizmoState.activePart == GizmoPart::ScaleY);
                const ImU32 uColor = ColorWithActive(scaleColor, hoverScaleUniform || gGizmoState.activePart == GizmoPart::ScaleUniform);

                drawList->AddLine(originScreen, xEnd, xColor, 2.5f);
                drawList->AddRectFilled(
                    ImVec2(xEnd.x - handleBox, xEnd.y - handleBox),
                    ImVec2(xEnd.x + handleBox, xEnd.y + handleBox), xColor);

                drawList->AddLine(originScreen, yEnd, yColor, 2.5f);
                drawList->AddRectFilled(
                    ImVec2(yEnd.x - handleBox, yEnd.y - handleBox),
                    ImVec2(yEnd.x + handleBox, yEnd.y + handleBox), yColor);

                drawList->AddRectFilled(
                    ImVec2(uniformEnd.x - handleBox * 1.1f, uniformEnd.y - handleBox * 1.1f),
                    ImVec2(uniformEnd.x + handleBox * 1.1f, uniformEnd.y + handleBox * 1.1f), uColor);
            }
            else if (gCurrentTransformMode == EditorTransformMode::Rotate)
            {
                const ImU32 ringColor = ColorWithActive(rotationColor, hoverRotate || gGizmoState.activePart == GizmoPart::Rotate);
                drawList->AddCircle(originScreen, ringPixels, ringColor, 64, 2.5f);
                if (ringWorld > 0.0f)
                {
                    const ImVec2 pointer = WorldToScreen(position + axisX * ringWorld, view, projection, viewportRect);
                    drawList->AddLine(originScreen, pointer, ringColor, 2.0f);
                }
            }

            drawList->AddText(
                ImVec2(viewportRect.x + 8.0f, viewportRect.y + 8.0f),
                ImGui::GetColorU32(ImGuiCol_Text),
                TransformModeLabel(gCurrentTransformMode));
        }

    } // namespace editor
} // namespace Framework

#endif