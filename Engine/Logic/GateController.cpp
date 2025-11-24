#include "GateController.h"

#include "Factory/Factory.h"
#include "Component/EnemyComponent.h"
#include "Component/EnemyHealthComponent.h"
#include "Component/RenderComponent.h"
#include "Component/TransformComponent.h"
#include "Physics/Collision/Collision.h"
#include "Physics/Dynamics/RigidBodyComponent.h"
#include <cctype>
#include <string>

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
      \brief Updates the cached gate pointer by checking the active level objects.

      - If the cached pointer is stale (destroyed or invalid), it is cleared.
      - If there is no cached gate, the function attempts to locate one.
      - If no gate is found, the unlocked state is reset.

      \param levelObjects Vector of active objects in the current level.
    *****************************************************************************************/
    void GateController::RefreshGateReference(const std::vector<GameObjectComposition*>& levelObjects)
    {
        if (!IsAlive(gate))
        {
            gate = nullptr;
        }

        if (!gate)
        {
            gate = FindGateInLevel(levelObjects);
        }

        if (!gate)
        {
            gateUnlocked = false;
        }
    }

    /*****************************************************************************************
      \brief Resets all cached state (player, gate, unlocked flag).

      Called during level reload to ensure the next level begins with clean state and the
      controller does not reference stale objects.
    *****************************************************************************************/
    void GateController::Reset()
    {
        player = nullptr;
        gate = nullptr;
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
        if (!gate || gateUnlocked)
            return;

        if (HasRemainingEnemies())
            return;

        gateUnlocked = true;

     
    }

    /*****************************************************************************************
      \brief Determines whether level transition should occur due to player–gate collision.

      \param pendingLevelTransition True if a transition is already underway.
      \return True if the gate is unlocked, the player is alive, and a collision occurs.

      \note Prevents multiple transitions if the flag is already set.
    *****************************************************************************************/
    bool GateController::ShouldTransitionOnPlayerContact(bool pendingLevelTransition) const
    {
        if (!gateUnlocked || !gate || !IsAlive(player) || pendingLevelTransition)
            return false;

        return PlayerIntersectsGate();
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
      \brief Locates the gate object in the provided level object list.

      Gate identification is performed via case-insensitive comparison of object names,
      matching "hawker_gate".

      \param levelObjects Vector of active objects in the level.
      \return Pointer to the gate object if found, otherwise nullptr.
    *****************************************************************************************/
    GameObjectComposition* GateController::FindGateInLevel(const std::vector<GameObjectComposition*>& levelObjects) const
    {
        auto nameEqualsIgnoreCase = [](const std::string& lhs, std::string_view rhs)
            {
                if (lhs.size() != rhs.size())
                    return false;
                for (std::size_t i = 0; i < lhs.size(); ++i)
                {
                    unsigned char c1 = static_cast<unsigned char>(lhs[i]);
                    unsigned char c2 = static_cast<unsigned char>(rhs[i]);
                    if (std::tolower(c1) != std::tolower(c2))
                        return false;
                }
                return true;
            };

        for (auto* obj : levelObjects)
        {
            if (obj && nameEqualsIgnoreCase(obj->GetObjectName(), "hawker_gate"))
                return obj;
        }

        return nullptr;
    }

    /*****************************************************************************************
      \brief Tests for collision between the player and the gate using bounding boxes.

      \return True if both objects have valid TransformComponent and RigidBodyComponent,
              and their AABBs overlap.

      \note Relies on Collision::CheckCollisionRectToRect for final evaluation.
    *****************************************************************************************/
    bool GateController::PlayerIntersectsGate() const
    {
        auto* gateTr = gate->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
        auto* gateRb = gate->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
        auto* tr = player->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
        auto* rb = player->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);

        if (!(gateTr && gateRb && tr && rb))
            return false;

        AABB gateBox(gateTr->x, gateTr->y, gateRb->width, gateRb->height);
        AABB playerBox(tr->x, tr->y, rb->width, rb->height);

        return Collision::CheckCollisionRectToRect(playerBox, gateBox);
    }
}