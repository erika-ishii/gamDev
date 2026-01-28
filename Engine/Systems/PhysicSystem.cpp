/*********************************************************************************************
 \file      PhysicSystem.cpp
 \par       SofaSpuds
 \author    Ho Jun (h.jun@digipen.edu) - Primary Author, 100%
 \brief     Lightweight 2D physics step: AABB moves/collisions + enemy hitbox damage.
 \details   Updates Transform by RigidBody velocity (dt) with axis-separated AABB tests
            against same-layer “rect?walls, then checks active EnemyAttack hitboxes
            against player AABBs to apply damage (via PlayerHealthComponent) and
            deactivate the hitbox after a successful hit. Includes simple layer filtering
            and case-insensitive wall name checks; printing to stdout for quick debugging.
 \copyright
            All content ?025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "PhysicSystem.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <string>
#include <cmath>

#include "Component/ZoomTriggerComponent.h"
#include "RenderSystem.h"
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
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

        // Build the uniform grid
        m_grid.Clear();

        // --- Kinematic step with AABB collisions against walls on the same layer ----------
        auto& layers = FACTORY->Layers();
        for (auto& [id, obj] : objects)
        {
            if (!obj)
                continue;

            const LayerKey objectLayer = layers.LayerKeyFor(obj->GetId());
            if (!layers.IsLayerEnabled(objectLayer))
                continue;

            // Determine if THIS object is the Player (by name)
            std::string objName = obj->GetObjectName();
            std::transform(objName.begin(), objName.end(), objName.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            const bool isPlayer = (objName == "player");

            auto* rb = obj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
            auto* tr = obj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
            if (!rb || !tr)
                continue;

            AABB box(tr->x, tr->y, rb->width, rb->height);
            m_grid.Insert(id, box);
            // ------------------------------
            // START OF KNOCKBACK APPLICATION 
            // ------------------------------
            float totalVelX = rb->velX;
            float totalVelY = rb->velY;
            if (rb->knockbackTime > 0.0f)
            {
                totalVelX += rb->knockVelX;
                totalVelY += rb->knockVelY;
            }
            // ------------------------------
            // END OF KNOCKBACK APPLICATION
            // ------------------------------
            
            // Integrate proposed new position
            float newX = tr->x + totalVelX * dt;
            float newY = tr->y + totalVelY * dt;

            // Sweep volumes prevent tunnelling when velocity * dt exceeds wall thickness.
                        // Center is midpoint of start/end; width/height span covers full travel distance.
            AABB playerBoxX((tr->x + newX) * 0.5f, tr->y,
                std::fabs(newX - tr->x) + rb->width, rb->height);
            AABB playerBoxY(tr->x, (tr->y + newY) * 0.5f,
                rb->width, std::fabs(newY - tr->y) + rb->height);



            // Sweep all objects on the same layer, checking only “rect?walls
            
            // Replaced with a model.
            // Explanation: The old code USED TO check every other object in the world,
            // now, the logic will only check the objects that are near.
            std::vector<GOCId> candidates;

            // Query grid using the object's current bounds
            AABB selfBox(tr->x, tr->y, rb->width, rb->height);
            m_grid.Query(selfBox, candidates);

            for (GOCId otherId : candidates) 
            { 
                auto it = objects.find(otherId);
                if (it == objects.end())
                    continue;

                auto& otherObj = it->second;

                if (!otherObj || otherObj == obj)
                    continue;
                const LayerKey otherLayer = layers.LayerKeyFor(otherObj->GetId());
                if (!layers.IsLayerEnabled(otherLayer))
                    continue;
                if (!(otherLayer == objectLayer))
                    continue;

                auto* rbO = otherObj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
                auto* trO = otherObj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
                if (!rbO || !trO)
                    continue;

        // -------------------------------------------------
        // 1) Zoom trigger logic (does NOT block movement)
        // -------------------------------------------------
                if (isPlayer) // only player should trigger zoom
                {
                    if (auto* zoom = otherObj->GetComponentType<ZoomTriggerComponent>(
                        ComponentTypeId::CT_ZoomTriggerComponent))
                    {
                        // AABB for player at new position
                        AABB playerBoxTrigger(newX, newY, rb->width, rb->height);
                        // AABB for the zoomObject (use its rigid body area)
                        AABB triggerBox(trO->x, trO->y, rbO->width, rbO->height);

                        if (Collision::CheckCollisionRectToRect(playerBoxTrigger, triggerBox))
                        {
                            if (!zoom->triggered)
                            {
                                zoom->triggered = true;

                                if (auto* rs = RenderSystem::Get())
                                {
                                    // targetZoom is interpreted as "view height" here.
                                    rs->SetCameraViewHeight(zoom->targetZoom);
                                }

                                if (zoom->oneShot)
                                {
                                    // Optional: remove the trigger so it doesn't fire again.
                                    // FACTORY->Destroy(otherObj);
                                }
                            }
                        }
                    }
                }

                // -------------------------------------------------
                // 2) Wall collision (existing code)
                // -------------------------------------------------

                // Case-insensitive name check for walls
                std::string otherName = otherObj->GetObjectName();
                std::transform(otherName.begin(), otherName.end(), otherName.begin(),
                    [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                if (otherName != "rect" && otherName != "invisiblehitbox")
                    continue;

                AABB wallBox(trO->x, trO->y, rbO->width, rbO->height);
                // Resolve X then Y independently
                if (Collision::CheckCollisionRectToRect(playerBoxX, wallBox))
                {
                    newX = tr->x;
                    rb->velX = 0.0f;
                    rb->knockVelX = 0.0f;   // ← cancel knockback on X
                }

                if (Collision::CheckCollisionRectToRect(playerBoxY, wallBox))
                {
                    newY = tr->y;
                    rb->velY = 0.0f;
                    rb->knockVelY = 0.0f;   // ← cancel knockback on Y
                }
                

            }
            // Commit final position
            tr->x = newX;
            tr->y = newY;

            // ------------------------------
            // KNOCKBACK DECAY
            // ------------------------------
            if (rb->knockbackTime > 0.0f)
            {
                rb->knockbackTime -= dt;

                // Optional damping for nicer feel
                rb->knockVelX *= 0.95f;
                rb->knockVelY *= 0.95f;

                if (rb->knockbackTime <= 0.0f)
                {
                    rb->knockVelX = 0.0f;
                    rb->knockVelY = 0.0f;
                }
            }
        }
    }

    /*************************************************************************************
      \brief  Release physics resources (currently no-op).
    *************************************************************************************/
    void PhysicSystem::Shutdown() {
        // Intentionally empty; add resource teardown when needed.
    }

} // namespace Framework
