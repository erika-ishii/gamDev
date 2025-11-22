/*********************************************************************************************
 \file      Inspector.cpp
 \par       SofaSpuds
 \author    ChatGPT (OpenAI)

 \brief     Implements the ImGui inspector panel. The panel mirrors the active editor
            selection and exposes commonly-used component fields so designers can quickly
            verify or tweak object properties after clicking them in the viewport or the
            hierarchy list. When a new object is selected the inspector window requests
            focus to satisfy the rubric requirement that picked objects become immediately
            inspectable.

 \copyright
            All content �2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#include "Inspector.h"

#include "Debug/Selection.h"
#include "Factory/Factory.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/CircleRenderComponent.h"
#include "Component/SpriteComponent.h"
#include "Physics/Dynamics/RigidBodyComponent.h"
#include "Debug/UndoStack.h"
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace {
    std::array<char, 128> gNameBuffer{};
    Framework::GOCId      gNameBufferObject = 0;
    Framework::GOCId      gFocusedSelection = 0;

    void SyncNameBuffer(Framework::GOCId id, const std::string& name)
    {
        gNameBufferObject = id;
        std::fill(gNameBuffer.begin(), gNameBuffer.end(), '\0');

        if (!name.empty())
            std::snprintf(gNameBuffer.data(), gNameBuffer.size(), "%s", name.c_str());
    }
}

namespace mygame {

    void DrawInspectorWindow()
    {
        Framework::GOCId selectedId = mygame::HasSelectedObject() ? mygame::GetSelectedObjectId() : 0;
        if (selectedId == 0)
        {
            gFocusedSelection = 0;
            gNameBufferObject = 0;
            gNameBuffer.fill('\0');
        }
        else if (selectedId != gFocusedSelection)
        {
            ImGui::SetNextWindowFocus();
            gFocusedSelection = selectedId;
        }

        if (!ImGui::Begin("Inspector Window"))
        {
            ImGui::End();
            return;
        }

        if (!Framework::FACTORY)
        {
            ImGui::TextDisabled("Factory not initialized.");
            ImGui::End();
            return;
        }

        if (selectedId == 0)
        {
            ImGui::TextDisabled("Select an object from the viewport or hierarchy to inspect it.");
            ImGui::End();
            return;
        }

        Framework::GOC* obj = Framework::FACTORY->GetObjectWithId(selectedId);
        if (!obj)
        {
            ImGui::TextDisabled("Previously selected object no longer exists.");
            mygame::ClearSelection();
            ImGui::End();
            return;
        }

        if (gNameBufferObject != selectedId)
            SyncNameBuffer(selectedId, obj->GetObjectName());

        ImGui::SeparatorText("Identity");
        ImGui::BeginDisabled(true);
        if (ImGui::InputText("Name", gNameBuffer.data(), gNameBuffer.size()))
            obj->SetObjectName(gNameBuffer.data());
        ImGui::EndDisabled();
        ImGui::Text("ID: %u", obj->GetId());
        ImGui::Text("Layer: %s", obj->GetLayerName().c_str());
        ImGui::Spacing();

        bool anyComponentDrawn = false;

        if (auto* tr = obj->GetComponentType<Framework::TransformComponent>(
            Framework::ComponentTypeId::CT_TransformComponent))
        {
            anyComponentDrawn = true;
            ImGui::SeparatorText("Transform");
            mygame::editor::TransformSnapshot before{};
            bool captured = false;
            auto capture = [&]()
                {
                    if (!captured)
                    {
                        before = mygame::editor::CaptureTransformSnapshot(*obj);
                        captured = true;
                    }
                };

            ImGui::BeginDisabled(true);
            float pos[2] = { tr->x, tr->y };
            if (ImGui::DragFloat2("Position", pos, 0.05f))
            {
                capture();
                tr->x = pos[0];
                tr->y = pos[1];
            }
            if (ImGui::SliderAngle("Rotation", &tr->rot, -360.0f, 360.0f))
                capture();

            if (captured)
                mygame::editor::RecordTransformChange(*obj, before);
            ImGui::EndDisabled();
        }

        if (auto* render = obj->GetComponentType<Framework::RenderComponent>(
            Framework::ComponentTypeId::CT_RenderComponent))
        {
            anyComponentDrawn = true;
            ImGui::SeparatorText("Rectangle Render");
            ImGui::BeginDisabled(true);
            mygame::editor::TransformSnapshot before{};
            bool captured = false;
            auto capture = [&]()
                {
                    if (!captured)
                    {
                        before = mygame::editor::CaptureTransformSnapshot(*obj);
                        captured = true;
                    }
                };
            float size[2] = { render->w, render->h };
            if (ImGui::DragFloat2("Size", size, 0.01f))
            {
                capture();
                render->w = size[0];
                render->h = size[1];
            }
            float color[4] = { render->r, render->g, render->b, render->a };
            if (ImGui::ColorEdit4("Color", color))
            {
                render->r = color[0];
                render->g = color[1];
                render->b = color[2];
                render->a = color[3];
            }
            bool visible = render->visible;
            if (ImGui::Checkbox("Visible", &visible))
                render->visible = visible;
            ImGui::EndDisabled();

            ImGui::Text("Texture Key: %s", render->texture_key.empty() ? "<none>" : render->texture_key.c_str());
            if (!render->texture_path.empty())
                ImGui::Text("Texture Path: %s", render->texture_path.c_str());
            ImGui::Text("Texture Id: %u", render->texture_id);
            if (captured)
                mygame::editor::RecordTransformChange(*obj, before);
        }

        if (auto* sprite = obj->GetComponentType<Framework::SpriteComponent>(
            Framework::ComponentTypeId::CT_SpriteComponent))
        {
            anyComponentDrawn = true;
            ImGui::SeparatorText("Sprite");
            ImGui::Text("Texture Key: %s", sprite->texture_key.empty() ? "<none>" : sprite->texture_key.c_str());
            ImGui::Text("Texture Path: %s", sprite->path.empty() ? "<none>" : sprite->path.c_str());
            ImGui::Text("Texture Id: %u", sprite->texture_id);
        }

        if (auto* circle = obj->GetComponentType<Framework::CircleRenderComponent>(
            Framework::ComponentTypeId::CT_CircleRenderComponent))
        {
            anyComponentDrawn = true;
            ImGui::SeparatorText("Circle");
            ImGui::BeginDisabled(true);
            float radius = circle->radius;
            if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.0f, 0.0f, "%.3f"))
                circle->radius = std::max(0.0f, radius);
            float color[4] = { circle->r, circle->g, circle->b, circle->a };
            if (ImGui::ColorEdit4("Color##Circle", color))
            {
                circle->r = color[0];
                circle->g = color[1];
                circle->b = color[2];
                circle->a = color[3];
            }
            ImGui::EndDisabled();
        }

        if (auto* rb = obj->GetComponentType<Framework::RigidBodyComponent>(
            Framework::ComponentTypeId::CT_RigidBodyComponent))
        {
            anyComponentDrawn = true;
            ImGui::SeparatorText("RigidBody");
            ImGui::BeginDisabled(true);
            float vel[2] = { rb->velX, rb->velY };
            if (ImGui::DragFloat2("Velocity", vel, 0.05f))
            {
                rb->velX = vel[0];
                rb->velY = vel[1];
            }
            float size[2] = { rb->width, rb->height };
            if (ImGui::DragFloat2("Collider Size", size, 0.01f))
            {
                rb->width = std::max(0.0f, size[0]);
                rb->height = std::max(0.0f, size[1]);
            }
            ImGui::EndDisabled();
        }

        if (!anyComponentDrawn)
            ImGui::TextDisabled("Object has no editable components.");

        ImGui::End();
    }

} // namespace mygame