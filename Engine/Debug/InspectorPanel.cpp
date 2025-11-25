/*********************************************************************************************
 \file      InspectorPanel.cpp
 \par       SofaSpuds
 \author     elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%
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
#include "Component/HitBoxComponent.h"
#include "Component/PlayerAttackComponent.h"
#include "Component/EnemyAttackComponent.h"
#include "Component/PlayerHealthComponent.h"
#include "Component/EnemyHealthComponent.h"
#include "Component/EnemyTypeComponent.h"
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

    void DrawAudioSection(AudioComponent& audio)
    {
        if (!ImGui::CollapsingHeader("Audio Component", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        // 1. Global Volume Control
        ImGui::DragFloat("Master Volume", &audio.volume, 0.01f, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::TextDisabled("Sound Actions");

        // 2. Gather all available sound resource IDs for the Dropdown
        std::vector<std::string> availableSounds;
        availableSounds.push_back(""); // Allow empty/none
        for (auto& [id, res] : Resource_Manager::resources_map) {
            if (res.type == Resource_Manager::Sound) {
                availableSounds.push_back(id);
            }
        }

        // 3. Iterate over existing actions (footsteps, Slash1, etc.)
        // Note: We use a copy of keys to avoid iterator invalidation if you modify the map structure,
        // though here we are only modifying values.
        for (auto& [action, info] : audio.sounds)
        {
            if (ImGui::TreeNode(action.c_str()))
            {
                // --- Sound ID Dropdown ---
                int currentIdx = 0;
                for (size_t i = 0; i < availableSounds.size(); ++i) {
                    if (availableSounds[i] == info.id) {
                        currentIdx = static_cast<int>(i);
                        break;
                    }
                }

                std::string comboLabel = "Sound Resource##" + action;
                if (ImGui::Combo(comboLabel.c_str(), &currentIdx,
                    [](void* data, int idx, const char** out_text) {
                        auto* vec = static_cast<std::vector<std::string>*>(data);
                        *out_text = (*vec)[idx].c_str();
                        return true;
                    },
                    &availableSounds, static_cast<int>(availableSounds.size())))
                {
                    info.id = availableSounds[currentIdx];
                }

                // --- Loop Checkbox ---
                std::string loopLabel = "Loop##" + action;
                ImGui::Checkbox(loopLabel.c_str(), &info.loop);

                ImGui::TreePop();
            }
        }
    }
    void DrawHitBoxFields(HitBoxComponent& hb, const char* labelPrefix = "")
    {
        // Optional prefix so we can reuse this for nested hitboxes if needed.
        auto MakeLabel = [labelPrefix](const char* base) -> const char*
            {
                static char buf[64];
                if (labelPrefix && *labelPrefix) {
                    std::snprintf(buf, sizeof(buf), "%s%s", labelPrefix, base);
                    return (const char*)buf;   // force const
                }
                return base;
            };
        float size[2] = { hb.width, hb.height };
        if (ImGui::DragFloat2(MakeLabel("Size##HitBox"), size, 0.01f, 0.0f, 1000.0f, "%.3f"))
        {
            hb.width = size[0];
            hb.height = size[1];
        }

        float offset[2] = { hb.spawnX, hb.spawnY };
        if (ImGui::DragFloat2(MakeLabel("Spawn Offset"), offset, 0.01f, -1000.0f, 1000.0f, "%.3f"))
        {
            hb.spawnX = offset[0];
            hb.spawnY = offset[1];
        }

        ImGui::DragFloat(MakeLabel("Duration"), &hb.duration, 0.01f, 0.0f, 100.0f, "%.2f");
        ImGui::DragFloat(MakeLabel("Damage##HitBox"), &hb.damage, 0.1f, 0.0f, 1000.0f, "%.1f");

        const char* teamLabels[] = { "Player", "Enemy", "Neutral", "Thrown" };
        int teamIndex = static_cast<int>(hb.team);
        if (ImGui::Combo(MakeLabel("Team"), &teamIndex, teamLabels, IM_ARRAYSIZE(teamLabels)))
        {
            hb.team = static_cast<HitBoxComponent::Team>(teamIndex);
        }

        ImGui::Checkbox(MakeLabel("Active"), &hb.active);
    }

    void DrawHitBoxSection(HitBoxComponent& hb)
    {
        if (!ImGui::CollapsingHeader("HitBox", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        DrawHitBoxFields(hb);
    }

    void DrawPlayerAttackSection(PlayerAttackComponent& atk)
    {
        if (!ImGui::CollapsingHeader("PlayerAttack", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        ImGui::DragInt("Damage##PlayerAttack", &atk.damage, 1, 0, 999);
        ImGui::DragFloat("Attack Speed", &atk.attack_speed, 0.01f, 0.0f, 10.0f, "%.2f");

        if (atk.hitbox)
        {
            if (ImGui::TreeNode("HitBox Defaults"))
            {
                DrawHitBoxFields(*atk.hitbox, "HB ");
                ImGui::TreePop();
            }
        }
    }

    void DrawEnemyAttackSection(EnemyAttackComponent& atk)
    {
        if (!ImGui::CollapsingHeader("EnemyAttack", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        ImGui::DragInt("Damage##EnemyAttack", &atk.damage, 1, 0, 999);
        ImGui::DragFloat("Attack Speed", &atk.attack_speed, 0.01f, 0.0f, 10.0f, "%.2f");

        if (atk.hitbox)
        {
            if (ImGui::TreeNode("HitBox Defaults"))
            {
                DrawHitBoxFields(*atk.hitbox, "HB ");
                ImGui::TreePop();
            }
        }
    }

    void DrawPlayerHealthSection(PlayerHealthComponent& hp)
    {
        if (!ImGui::CollapsingHeader("PlayerHealth", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        ImGui::DragInt("Current HP", &hp.playerHealth, 1, 0, hp.playerMaxhealth);
        ImGui::DragInt("Max HP", &hp.playerMaxhealth, 1, 1, 999);
    }

    void DrawEnemyHealthSection(EnemyHealthComponent& hp)
    {
        if (!ImGui::CollapsingHeader("EnemyHealth", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        ImGui::DragInt("Current HP", &hp.enemyHealth, 1, 0, hp.enemyMaxhealth);
        ImGui::DragInt("Max HP", &hp.enemyMaxhealth, 1, 1, 999);
    }

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
    void DrawEnemyTypeSection(EnemyTypeComponent& type)
    {
        if (!ImGui::CollapsingHeader("EnemyType", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        const char* typeItems[] = { "physical", "ranged" };
        int current = (type.Etype == EnemyTypeComponent::EnemyType::physical) ? 0 : 1;
        if (ImGui::Combo("Type", &current, typeItems, IM_ARRAYSIZE(typeItems)))
        {
            type.Etype = (current == 0)
                ? EnemyTypeComponent::EnemyType::physical
                : EnemyTypeComponent::EnemyType::ranged;
        }
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
        if (auto* hit = object->GetComponentAs<HitBoxComponent>(ComponentTypeId::CT_HitBoxComponent))
            DrawHitBoxSection(*hit);

        if (auto* pAtk = object->GetComponentAs<PlayerAttackComponent>(ComponentTypeId::CT_PlayerAttackComponent))
            DrawPlayerAttackSection(*pAtk);

        if (auto* eAtk = object->GetComponentAs<EnemyAttackComponent>(ComponentTypeId::CT_EnemyAttackComponent))
            DrawEnemyAttackSection(*eAtk);

        if (auto* pHp = object->GetComponentAs<PlayerHealthComponent>(ComponentTypeId::CT_PlayerHealthComponent))
            DrawPlayerHealthSection(*pHp);

        if (auto* eHp = object->GetComponentAs<EnemyHealthComponent>(ComponentTypeId::CT_EnemyHealthComponent))
            DrawEnemyHealthSection(*eHp);
        if (auto* eType = object->GetComponentAs<EnemyTypeComponent>(ComponentTypeId::CT_EnemyTypeComponent))
            DrawEnemyTypeSection(*eType);
        if (auto* audio = object->GetComponentAs<AudioComponent>(ComponentTypeId::CT_AudioComponent))
            DrawAudioSection(*audio);

        ImGui::End();
    }
}