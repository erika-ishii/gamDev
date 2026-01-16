/*********************************************************************************************
 \file      GateController.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%
 \brief     Implements the GateController, which manages gate unlocking logic, enemy-clearing
            checks, player–gate interactions, and safe reference tracking across level loads.
 \details   Responsibilities:
            - Tracks the player and gate GameObjectComposition pointers safely.
            - Scans level objects to find and cache gate objects with GateTargetComponent.
            - Unlocks the gate once all enemies with EnemyHealthComponent are defeated.
            - Performs collision testing between player and gate using AABB + physics data.
            - Determines when a level transition should occur (player touches unlocked gate).
            - Validates object liveness through the factory to avoid dangling pointers.
            - Automatically resets state when levels reload.
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "GateController.h"

#include "Factory/Factory.h"
#include "Component/EnemyComponent.h"
#include "Component/EnemyHealthComponent.h"
#include "Component/GateTargetComponent.h"
#include "Component/TransformComponent.h"
#include "Physics/Collision/Collision.h"
#include "Physics/Dynamics/RigidBodyComponent.h"

#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace Framework
{
    /*****************************************************************************************
      \brief Stores the factory pointer used to query existing GOCs and validate object states.
      \param f Pointer to the active GameObjectFactory.
    *****************************************************************************************/
    void GateController::SetFactory(GameObjectFactory* f)
    {
        factory = f;
    }

    /*****************************************************************************************
      \brief Assigns the current player object to the controller.
      \param p Pointer to the player GOC.
      \note  May be nullptr during level load/reset. Safe to call anytime.
    *****************************************************************************************/
    void GateController::SetPlayer(GameObjectComposition* p)
    {
        player = p;
    }

    /*****************************************************************************************
      \brief Updates the cached gate list by checking the active level objects.

      - Refreshes the gate list based on GateTargetComponent presence.
      - If no gate is found, the unlocked state is reset.

      \param levelObjects Vector of active objects in the current level.
    *****************************************************************************************/
    void GateController::RefreshGateReference(const std::vector<GameObjectComposition*>& levelObjects)
    {
        gates = FindGatesInLevel(levelObjects);

        if (gates.empty())
        {
            gateUnlocked = false;
        }
    }

    /*****************************************************************************************
      \brief Resets all cached state (player, gates, unlocked flag).

      Called during level reload to ensure the next level begins with clean state and the
      controller does not reference stale objects.
    *****************************************************************************************/
    void GateController::Reset()
    {
        player = nullptr;
        gates.clear();
        gateUnlocked = false;
    }

    /*****************************************************************************************
      \brief Updates whether the gate should become unlocked.

      Conditions:
       - Gate must exist.
       - Gate must not already be unlocked.
       - There must be no remaining enemies in the level.

      When unlocked, the gate’s RenderComponent tint is modified to visually indicate that
      the player may proceed.

      \note  Safe to call every frame; unlock only triggers once.
    *****************************************************************************************/
    void GateController::UpdateGateUnlockState()
    {
        if (gates.empty() || gateUnlocked)
            return;

        if (HasRemainingEnemies())
            return;

        gateUnlocked = true;

     
    }

    /*****************************************************************************************
      \brief Determines whether level transition should occur due to player–gate collision.

      \param pendingLevelTransition True if a transition is already underway.
      \param outLevelPath Outputs the gate's target level path.
      \return True if a gate is unlocked, the player is alive, and a collision occurs.

      \note Prevents multiple transitions if the flag is already set.
    *****************************************************************************************/
    bool GateController::ShouldTransitionOnPlayerContact(bool pendingLevelTransition, std::string& outLevelPath) const
    {
        if (!gateUnlocked || gates.empty() || !IsAlive(player) || pendingLevelTransition)
            return false;

        for (auto* gateObject : gates)
        {
            if (!IsAlive(gateObject))
                continue;
            if (!PlayerIntersectsGate(gateObject))
                continue;

            auto* target = gateObject->GetComponentType<GateTargetComponent>(
                ComponentTypeId::CT_GateTargetComponent);
            if (!target || target->levelPath.empty())
                continue;

            outLevelPath = target->levelPath;
            return true;
        }

        return false;
    }

    /*****************************************************************************************
      \brief Determines whether any enemies with remaining health still exist in the level.

      \return True if at least one enemy GOC is alive and has enemyHealth > 0.

      \note Robust: relies on component presence, not object naming.
    *****************************************************************************************/
    bool GateController::HasRemainingEnemies() const
    {
        if (!factory)
            return false;

        for (auto const& [id, ptr] : factory->Objects())
        {
            (void)id;
            auto* obj = ptr.get();
            if (!obj)
                continue;

            auto* enemy = obj->GetComponentType<EnemyComponent>(ComponentTypeId::CT_EnemyComponent);
            if (!enemy)
                continue;

            auto* health = obj->GetComponentType<EnemyHealthComponent>(ComponentTypeId::CT_EnemyHealthComponent);
            if (!health || health->enemyHealth > 0)
                return true;
        }

        return false;
    }

    /*****************************************************************************************
      \brief Validates whether a GOC pointer still refers to an existing, live object.

      \param obj Pointer to test.
      \return True if the object is still owned by the factory.

      \note Protects against stale pointers after deletion or level reload.
    *****************************************************************************************/
    bool GateController::IsAlive(GameObjectComposition* obj) const
    {
        if (!obj || !factory)
            return false;

        for (auto& [id, ptr] : factory->Objects())
        {
            (void)id;
            if (ptr.get() == obj)
                return true;
        }

        return false;
    }

    /*****************************************************************************************
      \brief Locates gate objects in the provided level object list.

     Gate identification is performed via GateTargetComponent presence.

      \param levelObjects Vector of active objects in the level.
      \return Vector of gate objects found in the level.
    *****************************************************************************************/
    std::vector<GameObjectComposition*> GateController::FindGatesInLevel(
        const std::vector<GameObjectComposition*>& levelObjects) const
    {
        std::vector<GameObjectComposition*> found;

        for (auto* obj : levelObjects)
        {
            if (!obj)
                continue;

            auto* target = obj->GetComponentType<GateTargetComponent>(
                ComponentTypeId::CT_GateTargetComponent);
            if (target)
                found.push_back(obj);
        }

        return found;
    }

    /*****************************************************************************************
      \brief Tests for collision between the player and the gate using bounding boxes.

      \return True if both objects have valid TransformComponent and RigidBodyComponent,
              and their AABBs overlap.

      \note Relies on Collision::CheckCollisionRectToRect for final evaluation.
    *****************************************************************************************/
    bool GateController::PlayerIntersectsGate(GameObjectComposition* gateObject) const
    {
        if (!gateObject)
            return false;

        auto* gateTr = gateObject->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
        auto* gateRb = gateObject->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
        auto* tr = player->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
        auto* rb = player->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);

        if (!(gateTr && gateRb && tr && rb))
            return false;

        AABB gateBox(gateTr->x, gateTr->y, gateRb->width, gateRb->height);
        AABB playerBox(tr->x, tr->y, rb->width, rb->height);

        return Collision::CheckCollisionRectToRect(playerBox, gateBox);
    }
}