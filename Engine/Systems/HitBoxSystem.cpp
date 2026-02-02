/*********************************************************************************************
 \file      HitBoxSystem.cpp
 \par       SofaSpuds
 \author    Ho Jun (h.jun@digipen.edu) - Primary Author, 100%
 \brief     Spawns and updates short-lived hit boxes for attack interactions.
 \details   This lightweight system manages transient attack volumes (HitBoxComponent):
            - Creation: SpawnHitBox() attaches owner/context and a lifetime timer.
            - Lifetime: Each active hit box counts down; removed when it expires or hits.
            - Collision: On each Update(), checks hit box vs. world hurt boxes (other objs'
              HitBoxComponent flagged active) via AABB overlap.
            - Integration: Driven by LogicSystem (e.g., mouse click creates a hit box in
              the player's facing direction).

            Notes:
            * The same HitBoxComponent struct is reused for both "hit" and "hurt" roles:
              - Newly spawned (this system) is used as the "hit" volume.
              - Other objects expose their "hurt" volume when HitBoxComponent::active == true.
            * Collision uses AABB vs AABB through Collision::CheckCollisionRectToRect.
            * This module stores hit boxes internally (not added to factory); they are
              ephemeral gameplay helpers rather than persistent game objects.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "HitBoxSystem.h"
#include "Composition/Component.h"
#include "LogicSystem.h"
#include "Component/HitBoxComponent.h"
#include "Component/SpriteAnimationComponent.h"
#include "Systems/VfxHelpers.h"
#include "Factory/Factory.h"

#include <iostream>
#include <cctype>
#include <string_view>
#include <cmath>
#include <glm/vec2.hpp>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace Framework
{
    namespace
    {
        /*****************************************************************************************
         \brief  Find the index of a named animation on a SpriteAnimationComponent (case-insensitive).

         \param anim
                Pointer to the SpriteAnimationComponent.
         \param desired
                Name of the animation we want to find.

         \return
                Index of the animation if found, otherwise -1.
        *****************************************************************************************/
        int FindAnimationIndex(SpriteAnimationComponent* anim, std::string_view desired)
        {
            if (!anim)
                return -1;

            auto equalsIgnoreCase = [](std::string_view a, std::string_view b)
                {
                    if (a.size() != b.size())
                        return false;

                    for (std::size_t i = 0; i < a.size(); ++i)
                    {
                        if (std::tolower(static_cast<unsigned char>(a[i])) !=
                            std::tolower(static_cast<unsigned char>(b[i])))
                            return false;
                    }
                    return true;
                };

            for (std::size_t i = 0; i < anim->animations.size(); ++i)
            {
                if (equalsIgnoreCase(anim->animations[i].name, desired))
                    return static_cast<int>(i);
            }

            return -1;
        }

        /*****************************************************************************************
         \brief  Helper to safely switch an animation by name if it exists on the given object.

         \param goc
                Game object that owns the SpriteAnimationComponent.
         \param name
                Animation name we want to set as active.

         \details
                Does nothing if the animation component or requested animation is missing.
        *****************************************************************************************/
        void PlayAnimationIfAvailable(GOC* goc, std::string_view name)
        {
            if (!goc)
                return;

            auto* anim =
                goc->GetComponentType<SpriteAnimationComponent>(ComponentTypeId::CT_SpriteAnimationComponent);
            if (!anim)
                return;

            const int idx = FindAnimationIndex(anim, name);
            if (idx >= 0 && idx != anim->ActiveAnimationIndex())
            {
                anim->SetActiveAnimation(idx);
            }
        }
    } // anonymous namespace

    /*****************************************************************************************
      \brief Construct the system with a reference to the driving LogicSystem.
      \param logicRef Owner that provides access to level objects for collision checks.
    *****************************************************************************************/
    HitBoxSystem::HitBoxSystem(LogicSystem& logicRef)
        : logic(logicRef)
    {
    }

    /// Ensures containers are cleared on destruction.
    HitBoxSystem::~HitBoxSystem()
    {
        Shutdown();
    }

    /*****************************************************************************************
      \brief Prepare internal state for a fresh session.
      \note  Clears any dangling hit boxes that might remain from a previous run.
    *****************************************************************************************/
    void HitBoxSystem::Initialize()
    {
        activeHitBoxes.clear();
    }

    /*****************************************************************************************
      \brief Release runtime state held by the system.
             (No heap ownership beyond unique_ptrs inside activeHitBoxes.)
    *****************************************************************************************/
    void HitBoxSystem::Shutdown()
    {
        activeHitBoxes.clear();
    }

    /*****************************************************************************************
      \brief Spawn a transient hit box owned by \p attacker at (\p targetX, \p targetY).
      \param attacker  The game object creating the hit (not collided against itself).
      \param targetX   World X center of the hit box.
      \param targetY   World Y center of the hit box.
      \param width     Width of the AABB.
      \param height    Height of the AABB.
      \param damage    Damage payload to apply (carried in the component; application external).
      \param duration  Lifetime in seconds before the hit box auto-expires.
      \param team      Initial team value (may be overridden based on attacker).
      \details
        - Allocates a HitBoxComponent and marks it active for collision participation.
        - Stores an internal ActiveHitBox record with countdown timer.
        - The hit is checked against other objects' active hurt boxes during Update().
    *****************************************************************************************/
    void HitBoxSystem::SpawnHitBox(GameObjectComposition* attacker,
        float targetX, float targetY,
        float width, float height,
        float damage,
        float duration,
        HitBoxComponent::Team team, float soundDelay)
    {
        if (!attacker)
            return;

        auto newhitbox = std::make_unique<HitBoxComponent>();
        newhitbox->spawnX = targetX;
        newhitbox->spawnY = targetY;
        newhitbox->width = width;
        newhitbox->height = height;
        newhitbox->damage = damage;
        newhitbox->duration = duration;
        newhitbox->owner = attacker;
        newhitbox->team = team;
        newhitbox->soundDelay = soundDelay;

        // Decide team based on attacker, so we avoid friendly fire.
        if (attacker->GetComponentType<PlayerComponent>(ComponentTypeId::CT_PlayerComponent))
            newhitbox->team = HitBoxComponent::Team::Player;
        else if (attacker->GetComponentType<EnemyComponent>(ComponentTypeId::CT_EnemyComponent))
            newhitbox->team = HitBoxComponent::Team::Enemy;
        else
            newhitbox->team = HitBoxComponent::Team::Neutral;

        newhitbox->ActivateHurtBox(); // reuse flag: treat as active volume for collisions

        std::string teamStr;
        switch (newhitbox->team)
        {
        case HitBoxComponent::Team::Player:  teamStr = "Player";  break;
        case HitBoxComponent::Team::Enemy:   teamStr = "Enemy";   break;
        case HitBoxComponent::Team::Thrown:  teamStr = "Thrown";  break;
        case HitBoxComponent::Team::Neutral: teamStr = "Neutral"; break;
        }

        std::cout << "HitBox spawned at (" << targetX << ", " << targetY
            << ") with team: " << teamStr << "\n";

        ActiveHitBox active;
        active.hitbox = std::move(newhitbox);
        active.ownerId = attacker->GetId();
        active.timer = duration;

        activeHitBoxes.push_back(std::move(active));
    }

    /*****************************************************************************************
      \brief Spawn a projectile hit box that moves over time in the given direction.
      \param attacker  The game object creating the projectile.
      \param targetX   Initial world X center of the projectile.
      \param targetY   Initial world Y center of the projectile.
      \param dirX      Direction X (will be normalized).
      \param dirY      Direction Y (will be normalized).
      \param speed     Projectile speed.
      \param width     Width of the AABB.
      \param height    Height of the AABB.
      \param damage    Damage payload to apply.
      \param duration  Lifetime in seconds.
      \param team      Initial team (may be overridden based on attacker).
    *****************************************************************************************/
    void HitBoxSystem::SpawnProjectile(GameObjectComposition* attacker,
        float targetX, float targetY,
        float dirX, float dirY,
        float speed,
        float width, float height,
        float damage,
        float duration,
        HitBoxComponent::Team team)
    {
        if (!attacker)
            return;

        // Normalized direction
        float len = std::sqrt(dirX * dirX + dirY * dirY);
        if (len < 0.0001f)
            return;

        dirX /= len;
        dirY /= len;

        auto newhitbox = std::make_unique<HitBoxComponent>();
        newhitbox->spawnX = targetX;
        newhitbox->spawnY = targetY;
        newhitbox->width = width;
        newhitbox->height = height;
        newhitbox->damage = damage;
        newhitbox->duration = duration;
        newhitbox->owner = attacker;
        newhitbox->team = team;

        // Set the team / make sure friendly fire does not happen.
        if (attacker->GetComponentType<PlayerComponent>(ComponentTypeId::CT_PlayerComponent))
            newhitbox->team = HitBoxComponent::Team::Thrown;
        else if (attacker->GetComponentType<EnemyComponent>(ComponentTypeId::CT_EnemyComponent))
            newhitbox->team = HitBoxComponent::Team::Enemy;

        newhitbox->ActivateHurtBox();

        std::string teamStr;
        switch (newhitbox->team)
        {
        case HitBoxComponent::Team::Player:  teamStr = "Player";  break;
        case HitBoxComponent::Team::Enemy:   teamStr = "Enemy";   break;
        case HitBoxComponent::Team::Thrown:  teamStr = "Thrown";  break;
        case HitBoxComponent::Team::Neutral: teamStr = "Neutral"; break;
        }

        std::cout << "HitBox spawned at (" << targetX << ", " << targetY
            << ") with team: " << teamStr << "\n";

        ActiveHitBox projectile;
        projectile.hitbox = std::move(newhitbox);
        projectile.ownerId = attacker->GetId();
        projectile.timer = duration;
        projectile.hitGraceTimer = 1.0f;
        projectile.velX = dirX * speed;
        projectile.velY = dirY * speed;
        projectile.isProjectile = true;

        activeHitBoxes.push_back(std::move(projectile));
    }

    /*****************************************************************************************
      \brief Advance all active hit boxes: tick timers, check collisions, and cull as needed.
      \param dt Delta time (seconds).
      \details
        - For each active hit box:
          * Decrement remaining time.
          * Build its AABB (center + extents).
          * Iterate over level objects from LogicSystem; skip the owner.
          * If an object is a valid target (based on team/type), check overlap.
          * If an enemy is hit, apply damage, knockback, and trigger "knockback" animation.
        - If no hit occurs, remove the hit box once its timer reaches zero.
    *****************************************************************************************/
    void HitBoxSystem::Update(float dt)
    {
        if (!FACTORY)
            return;

        auto& layers = FACTORY->Layers();

        for (auto it = activeHitBoxes.begin(); it != activeHitBoxes.end();)
        {
            it->timer -= dt;
            auto* HB = it->hitbox.get();
            auto* attacker = FACTORY->GetObjectWithId(it->ownerId);

            if (!attacker || !HB || !HB->active)
            {
                it = activeHitBoxes.erase(it);
                continue;
            }
            if (!layers.IsLayerEnabled(attacker->GetLayerName()))
            {
                it = activeHitBoxes.erase(it);
                continue;
            }
            // Projectile movement
            if (it->isProjectile || HB->team == HitBoxComponent::Team::Thrown)
            {
                it->hitbox->spawnX += it->velX * dt;
                it->hitbox->spawnY += it->velY * dt;
            }

            AABB hitboxAABB(HB->spawnX, HB->spawnY, HB->width, HB->height);

            bool hitAnything = false;
            bool hitEnemy = false;
            bool ineffectiveHit = false;

            if (it->isProjectile && it->hitGraceTimer > 0.0f)
                it->hitGraceTimer = std::max(0.0f, it->hitGraceTimer - dt);

            for (auto* obj : logic.LevelObjects())
            {
                if (!obj || obj == attacker) continue;
                if (!layers.IsLayerEnabled(obj->GetLayerName())) continue;
                auto* tr = obj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
                auto* rb = obj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
                if (!(tr && rb)) continue;

                if (it->isProjectile && it->hitGraceTimer > 0.0f)
                {
                    const bool isPlayer = obj->GetComponentType<PlayerComponent>(
                        ComponentTypeId::CT_PlayerComponent) != nullptr;
                    const bool isEnemy = obj->GetComponentType<EnemyComponent>(
                        ComponentTypeId::CT_EnemyComponent) != nullptr;
                    if (!isPlayer && !isEnemy)
                        continue;
                }

                AABB targetAABB(tr->x, tr->y, rb->width, rb->height);
                if (!Collision::CheckCollisionRectToRect(hitboxAABB, targetAABB)) continue;

                bool validTargetHit = false;

                // Player hit logic
                if (auto* playerHealth = obj->GetComponentType<PlayerHealthComponent>(ComponentTypeId::CT_PlayerHealthComponent))
                {
                    if (!playerHealth->isInvulnerable)
                    {
                        playerHealth->TakeDamage(static_cast<int>(HB->damage));
                        validTargetHit = true;

                        if (auto* audio = obj->GetComponentType<AudioComponent>(ComponentTypeId::CT_AudioComponent))
                        {
                            if (!playerHealth->isDead)
                                audio->TriggerSound("PlayerHit");
                            else if (!playerHealth->deathSoundPlayed)
                            {
                                audio->TriggerSound("PlayerDead");
                                playerHealth->deathSoundPlayed = true;
                            }
                        }
                    }
                }
                // Enemy hit logic
                else if (auto* enemyHealth = obj->GetComponentType<EnemyHealthComponent>(ComponentTypeId::CT_EnemyHealthComponent))
                {
                    bool canHit = false;

                    if (auto* typeComp = obj->GetComponentType<EnemyTypeComponent>(ComponentTypeId::CT_EnemyTypeComponent))
                    {
                        if (typeComp->Etype == EnemyTypeComponent::EnemyType::physical && HB->team == HitBoxComponent::Team::Player)
                            canHit = true;
                        if (typeComp->Etype == EnemyTypeComponent::EnemyType::ranged && HB->team == HitBoxComponent::Team::Thrown)
                            canHit = true;
                    }
                    else
                    {
                        canHit = true;
                    }

                    if (enemyHealth->enemyHealth <= 0) canHit = false;

                    if (canHit)
                    {
                        enemyHealth->TakeDamage(static_cast<int>(HB->damage));
                        validTargetHit = true;
                        hitEnemy = true;
                        SpawnHitImpactVFX(glm::vec2(tr->x, tr->y));
                        if (auto* audio = obj->GetComponentType<AudioComponent>(ComponentTypeId::CT_AudioComponent))
                        {audio->TriggerSound("EnemyHit");}
                    }
                    else if (enemyHealth->enemyHealth > 0)
                    {
                        ineffectiveHit = true;
                    }
                }
                else
                {
                    validTargetHit = true; // Non-damaging hit (neutral objects)
                }

                // Apply knockback if valid hit
                if (validTargetHit)
                {
                    bool isPlayer = obj->GetComponentType<PlayerComponent>(ComponentTypeId::CT_PlayerComponent) != nullptr;
                    bool isEnemy = obj->GetComponentType<EnemyComponent>(ComponentTypeId::CT_EnemyComponent) != nullptr;
                    if ((isPlayer || isEnemy))
                    {
                        auto* attackerTr = attacker->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
                        if (attackerTr)
                        {
                            float dx = tr->x - attackerTr->x;
                            float dy = tr->y - attackerTr->y;
                            float len = std::sqrt(dx * dx + dy * dy);
                            if (len > 0.001f) { dx /= len; dy /= len; }

                            const float knockStrength = 1.5f;
                            rb->knockVelX = dx * knockStrength;
                            rb->knockVelY = dy * knockStrength * 0.4f;
                            rb->knockbackTime = 0.25f;
                        }

                        PlayAnimationIfAvailable(obj, "knockback");
                    }
                }

                if (validTargetHit) hitAnything = true;
                break; // Only first collision per hitbox
            }

            // Play air swing or ineffective sound if no enemy hit
            if (!HB->soundTriggered && HB->team == HitBoxComponent::Team::Player)
            {
                HB->soundDelay -= dt;
                if (HB->soundDelay <= 0.0f)
                {
                    if (auto* audio = attacker->GetComponentType<AudioComponent>(ComponentTypeId::CT_AudioComponent))
                    {
                        if (hitEnemy)
                            audio->TriggerSound("Slash");
                        if (ineffectiveHit)
                            audio->TriggerSound("Ineffective"); // Blocked or no effect
                        if (!hitAnything)
                            audio->TriggerSound("Punch");       // Missed swing
                    }
                }
                HB->soundTriggered = true;
            }

            // Remove hitbox if hit or expired
            if (hitAnything || it->timer <= 0.0f)
                it = activeHitBoxes.erase(it);
            else
                ++it;
        }
    }


} // namespace Framework
