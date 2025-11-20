/*********************************************************************************************
 \file      InspectorPanel.cpp
 \par       SofaSpuds
 \author    
 \brief     Implements a basic ImGui inspector window for viewing and editing the currently
            selected GameObject's properties and common component data.
*********************************************************************************************/

#include "InspectorPanel.h"

#include "Composition/Composition.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/CircleRenderComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/HitBoxComponent.h"
#include "Physics/Dynamics/RigidBodyComponent.h"
#include "Selection.h"
#include "Factory/Factory.h"
#include "Debug/UndoStack.h"

#include <imgui.h>
#include <array>
#include <string>
#include <cstdio>

namespace
{
    using namespace Framework;

    void DrawRigidBodySection(Framework::GOC& owner, RigidBodyComponent& rb)
    {
        if (!ImGui::CollapsingHeader("RigidBody", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        // Velocity (x, y)
        float velocity[2] = { rb.velX, rb.velY };
        if (ImGui::DragFloat2("Velocity", velocity, 0.1f, -1000.0f, 1000.0f, "%.2f"))
        {
            rb.velX = velocity[0];
            rb.velY = velocity[1];
        }

        // Collider size (width, height)
        float size[2] = { rb.width, rb.height };
        if (ImGui::DragFloat2("Collider Size", size, 0.01f, 0.0f, 1000.0f, "%.3f"))
        {
            rb.width = size[0];
            rb.height = size[1];
        }

        // (owner is passed in so later you can hook this into UndoStack if you want)
        (void)owner;
    }


    void DrawTransformSection(Framework::GOC& owner, TransformComponent& transform)
    {
        if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        mygame::editor::TransformSnapshot before{};
        bool captured = false;
        auto capture = [&]()
            {
                if (!captured)
                {
                    before = mygame::editor::CaptureTransformSnapshot(owner);
                    captured = true;
                }
            };

        float position[2] = { transform.x, transform.y };
        if (ImGui::DragFloat2("Position", position, 0.1f))
        {
            capture();
            transform.x = position[0];
            transform.y = position[1];
        }

        if (ImGui::DragFloat("Rotation", &transform.rot, 0.5f, -360.0f, 360.0f, "%.2f"))
            capture();

        if (captured)
            mygame::editor::RecordTransformChange(owner, before);
    }

    void DrawRenderSection(Framework::GOC& owner, RenderComponent& render)
    {
        if (!ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen))
            return;
        mygame::editor::TransformSnapshot before{};
        bool captured = false;
        auto capture = [&]()
            {
                if (!captured)
                {
                    before = mygame::editor::CaptureTransformSnapshot(owner);
                    captured = true;
                }
            };
        float size[2] = { render.w, render.h };
        if (ImGui::DragFloat2("Size", size, 1.0f, 0.0f, 10000.0f, "%.1f"))
        {
            capture();
            render.w = size[0];
            render.h = size[1];
        }

        float color[4] = { render.r, render.g, render.b, render.a };
        if (ImGui::ColorEdit4("Tint", color))
        {
            render.r = color[0];
            render.g = color[1];
            render.b = color[2];
            render.a = color[3];
        }

        ImGui::Checkbox("Visible", &render.visible);
        if (captured)
            mygame::editor::RecordTransformChange(owner, before);

    
    }

    void DrawCircleRenderSection(Framework::GOC& owner, CircleRenderComponent& circle)
    {
        if (!ImGui::CollapsingHeader("Circle Render", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        mygame::editor::TransformSnapshot before{};
        bool captured = false;
        auto capture = [&]()
            {
                if (!captured)
                {
                    before = mygame::editor::CaptureTransformSnapshot(owner);
                    captured = true;
                }
            };

        if (ImGui::DragFloat("Radius", &circle.radius, 0.05f, 0.0f, 1000.0f, "%.2f"))
            capture();

        float color[4] = { circle.r, circle.g, circle.b, circle.a };
        if (ImGui::ColorEdit4("Color", color))
        {
            circle.r = color[0];
            circle.g = color[1];
            circle.b = color[2];
            circle.a = color[3];
        }

        if (captured)
            mygame::editor::RecordTransformChange(owner, before);
    }

    void DrawSpriteSection(SpriteComponent& sprite)
    {
        if (!ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        static std::array<char, 256> keyBuffer{};
        static std::array<char, 256> pathBuffer{};

        std::snprintf(keyBuffer.data(), keyBuffer.size(), "%s", sprite.texture_key.c_str());
        std::snprintf(pathBuffer.data(), pathBuffer.size(), "%s", sprite.path.c_str());

        if (ImGui::InputText("Texture Key", keyBuffer.data(), keyBuffer.size()))
            sprite.texture_key = keyBuffer.data();
        if (ImGui::InputText("Texture Path", pathBuffer.data(), pathBuffer.size()))
            sprite.path = pathBuffer.data();
    }


}

namespace mygame
{
    void DrawPropertiesEditor()
    {
        using namespace Framework;

        if (!FACTORY)
            return;
        if (mygame::HasSelectedObject())
        {
            const auto selectedId = mygame::GetSelectedObjectId();
            const auto& objects = FACTORY->Objects();
            if (objects.find(selectedId) == objects.end())
            {
                mygame::ClearSelection();
            }
        }
        GOC* object = nullptr;
        if (mygame::HasSelectedObject())
        {
            object = FACTORY->GetObjectWithId(mygame::GetSelectedObjectId());
        }
        if (!ImGui::Begin("Properties Editor"))
        {
            ImGui::End();
            return;
        }

        if (!object)
        {
            ImGui::TextDisabled("No object selected.");
            ImGui::End();
            return;
        }

        static GOCId lastSelection = 0;
        static std::array<char, 128> nameBuffer{};
        static std::array<char, 64> layerBuffer{};

        if (lastSelection != object->GetId())
        {
            std::snprintf(nameBuffer.data(), nameBuffer.size(), "%s", object->GetObjectName().c_str());
            std::snprintf(layerBuffer.data(), layerBuffer.size(), "%s", object->GetLayerName().c_str());
            lastSelection = object->GetId();
        }

        if (ImGui::InputText("Name", nameBuffer.data(), nameBuffer.size()))
            object->SetObjectName(nameBuffer.data());
        if (ImGui::InputText("Layer", layerBuffer.data(), layerBuffer.size()))
            object->SetLayerName(layerBuffer.data());

        ImGui::TextDisabled("ID: %u", object->GetId());
        ImGui::Separator();

        if (auto* transform = object->GetComponentAs<TransformComponent>(ComponentTypeId::CT_TransformComponent))
            DrawTransformSection(*object, *transform);

        if (auto* render = object->GetComponentAs<RenderComponent>(ComponentTypeId::CT_RenderComponent))
            DrawRenderSection(*object, *render);

        if (auto* circle = object->GetComponentAs<CircleRenderComponent>(ComponentTypeId::CT_CircleRenderComponent))
            DrawCircleRenderSection(*object, *circle);

        if (auto* sprite = object->GetComponentAs<SpriteComponent>(ComponentTypeId::CT_SpriteComponent))
            DrawSpriteSection(*sprite);
        if (auto* rb = object->GetComponentAs<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent))
            DrawRigidBodySection(*object, *rb);


        ImGui::End();
    }
}