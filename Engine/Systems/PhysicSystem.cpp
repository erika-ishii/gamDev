/*********************************************************************************************
 \file      PhysicSystem.cpp
 \par       SofaSpuds
 \author    Ho Jun (h.jun@digipen.edu) - Primary Author, 100%
 \brief     Lightweight 2D physics step: AABB moves/collisions + enemy hitbox damage.
 \details   Updates Transform by RigidBody velocity (dt) with axis-separated AABB tests
            against same-layer “rect” walls, then checks active EnemyAttack hitboxes
            against player AABBs to apply damage (via PlayerHealthComponent) and
            deactivate the hitbox after a successful hit. Includes simple layer filtering
            and case-insensitive wall name checks; printing to stdout for quick debugging.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "PhysicSystem.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <string>

namespace Framework {

    /*************************************************************************************
      \brief  Construct the physics system with access to game logic/factory.
      \param  logic  Reference to the LogicSystem for scene queries.
    *************************************************************************************/
    PhysicSystem::PhysicSystem(LogicSystem& logic)
        : logic(logic) {
    }

    /*************************************************************************************
      \brief  Initialize physics state/resources (currently no-op).
    *************************************************************************************/
    void PhysicSystem::Initialize() {
        // Intentionally empty; kept for symmetry and future extensions.
    }

    /*************************************************************************************
      \brief  Advance physics one step: move bodies and resolve simple AABB collisions;
              then process enemy hitboxes vs players and apply damage.
      \param  dt  Delta time (seconds).
      \note   Movement is axis-separated: X and Y are tested independently for wall hits.
              Walls are identified by object name "rect" (case-insensitive) on the same layer.
              Enemy hitboxes are one-shot: after a hit, the hurtbox is deactivated.
    *************************************************************************************/
    void PhysicSystem::Update(float dt)
    {
        /*
        // Example for future collision service usage:
        // const auto& info = logic.Collision();
        // if (info.playerValid && info.targetValid)
        // {
        //     if (Collision::CheckCollisionRectToRect(info.player, info.target))
        //         std::cout << "Collision detected!\n";
        // }
        */

        auto& objects = FACTORY->Objects();

        // --- Kinematic step with AABB collisions against walls on the same layer ----------
        for (auto& [id, obj] : objects)
        {
            if (!obj)
                continue;

            auto* rb = obj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
            auto* tr = obj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
            if (!rb || !tr)
                continue;

            // Integrate proposed new position
            float newX = tr->x + rb->velX * dt;
            float newY = tr->y + rb->velY * dt;

            // Build trial AABBs for axis-separated collision checks
            AABB playerBoxX(newX, tr->y, rb->width, rb->height);
            AABB playerBoxY(tr->x, newY, rb->width, rb->height);

            const std::string& objectLayer = obj->GetLayerName();

            // Sweep all objects on the same layer, checking only “rect” walls
            for (auto& [otherId, otherObj] : objects)
            {
                if (!otherObj || otherObj == obj)
                    continue;
                if (otherObj->GetLayerName() != objectLayer)
                    continue;

                auto* rbO = otherObj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
                auto* trO = otherObj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
                if (!rbO || !trO)
                    continue;

                // Case-insensitive name check for walls
                std::string otherName = otherObj->GetObjectName();
                std::transform(otherName.begin(), otherName.end(), otherName.begin(),
                    [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                if (otherName != "rect" && otherName != "invisiblehitbox")
                    continue;

                AABB wallBox(trO->x, trO->y, rbO->width, rbO->height);

                // Resolve X then Y independently
                if (Collision::CheckCollisionRectToRect(playerBoxX, wallBox))
                    newX = tr->x;

                if (Collision::CheckCollisionRectToRect(playerBoxY, wallBox))
                    newY = tr->y;
            }

            // Commit final position
            tr->x = newX;
            tr->y = newY;
        }
    }

    /*************************************************************************************
      \brief  Release physics resources (currently no-op).
    *************************************************************************************/
    void PhysicSystem::Shutdown() {
        // Intentionally empty; add resource teardown when needed.
    }

} // namespace Framework
