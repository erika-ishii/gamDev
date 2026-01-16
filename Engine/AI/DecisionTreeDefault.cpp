/*********************************************************************************************
 \file      DecisionTreeDefault.cpp
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 80%
            yimo kong (yimo.kong@digipen.edu)      - Author, 20%

 \brief     Implementation of default decision tree behavior for enemy AI. Defines helper
            functions that manage proximity checks, tree creation, and periodic updates
            for simple patrol and attack behaviors.

 \details
            This module provides a reusable default decision tree structure used by
            enemy entities. The decision tree includes:
            - A proximity check to detect the player.
            - An attack branch triggered upon player detection.
            - A patrol branch used when the player is not nearby.

            The functions in this file handle tree construction, condition evaluation,
            and execution of context-specific actions such as movement or attack logic.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "DecisionTreeDefault.h"
#include "Component/SpriteAnimationComponent.h"
#include "Component/EnemyTypeComponent.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <cmath>
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
                 Does nothing if the component or animation is missing.
         *****************************************************************************************/
        void PlayAnimationIfAvailable(GOC* goc, std::string_view name, bool forceRestart = false)
        {
            (void)forceRestart;
            if (!goc)
                return;

            auto* anim = goc->GetComponentType<SpriteAnimationComponent>(ComponentTypeId::CT_SpriteAnimationComponent);
            if (!anim)
                return;

            const int idx = FindAnimationIndex(anim, name);
            if (idx >= 0 && idx != anim->ActiveAnimationIndex())
            {
                anim->SetActiveAnimation(idx);
            }
        }

        float GetAnimationDuration(GOC* enemy, const std::string& name)
        {
            auto* anim = enemy->GetComponentType<SpriteAnimationComponent>(
                ComponentTypeId::CT_SpriteAnimationComponent);

            if (!anim) return 0.2f;

            for (auto& a : anim->animations)
            {
                if (a.name == name)
                {
                    return a.config.totalFrames / a.config.fps;
                }
            }
            return 0.2f;
        }
    }

    /*****************************************************************************************
    \brief
    Checks whether the player is within a specified distance of the given enemy.

    \param enemy
    Pointer to the enemy game object being evaluated.
    \param radius
    Distance threshold for proximity detection (default is 0.1f).

    \return
    True if the player is within the given radius, otherwise false.
    *****************************************************************************************/
    bool IsPlayerNear(GOC* enemy, float radius)
    {
        if (!enemy)
            return false;

        GOC* player = nullptr;
        auto& objects = FACTORY->Objects();

        // Find the first object that has a PlayerComponent
        for (auto& kv : objects)
        {
            GOC* goc = kv.second.get();
            if (!goc)
                continue;

            auto* base = goc->GetComponent(ComponentTypeId::CT_PlayerComponent);
            if (base)
            {
                player = goc;
                break;
            }
        }

        if (!player)
            return false;

        // Access transforms of enemy and player
        auto* enemyTra = enemy->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
        auto* playerTra = player->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
        if (!enemyTra || !playerTra)
            return false;

        float dx = enemyTra->x - playerTra->x;
        float dy = enemyTra->y - playerTra->y;
        return (dx * dx + dy * dy) <= radius * radius;
    }

    /*****************************************************************************************
    \brief
    Creates a default decision tree for basic enemy AI behavior.

    \param enemy
    Pointer to the enemy game object that will own the decision tree.
    \param logic
    Pointer to the LogicSystem, used to spawn hitboxes for attacks.

    \return
    A unique pointer to a newly created DecisionTree containing patrol and attack logic.
    *****************************************************************************************/
    std::unique_ptr<DecisionTree> CreateDefaultEnemyTree(GOC* enemy, LogicSystem* logic)
    {
        if (!enemy)
            return nullptr;

        const GOCId enemyID = enemy->GetId();
        // ---------------------------------------------------------------------
        // Patrol leaf: simple left-right patrol with pause when turning around.
        // ---------------------------------------------------------------------
        auto patrolLeaf = std::make_unique<DecisionNode>(
            nullptr,
            nullptr,
            nullptr,
            [enemyID](float dt)
            {
                GOC* enemy = FACTORY->GetObjectWithId(enemyID);
                if (!enemy)
                    return;

                auto* rb = enemy->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
                auto* tr = enemy->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
                auto* ai = enemy->GetComponentType<EnemyDecisionTreeComponent>(ComponentTypeId::CT_EnemyDecisionTreeComponent);

                if (rb && tr && ai)
                {
                    const float patrolSpeed = 0.2f;
                    const float patrolRange = 0.5f;
                    const float pauseDuration = 2.0f;

                    // If currently pausing, count down and stop movement
                    if (ai->pauseTimer > 0.0f)
                    {
                        ai->pauseTimer -= dt;
                        rb->velX = 0.0f;
                        return;
                    }

                    // Move horizontally according to patrol direction
                    rb->velX = patrolSpeed * ai->dir;
                    rb->velY = 0.0f;

                    float newX = tr->x + rb->velX * dt;
                    float newY = tr->y;

                    // Predict the future AABB and test collisions with "rect" walls
                    AABB futureBox(newX, newY, rb->width, rb->height);
                    bool collisionDetected = false;

                    auto& objects = FACTORY->Objects();
                    for (auto& [otherId, otherObj] : objects)
                    {
                        auto* rbO = otherObj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
                        auto* trO = otherObj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
                        if (!rbO || !trO)
                            continue;

                        std::string otherName = otherObj->GetObjectName();
                        std::transform(
                            otherName.begin(),
                            otherName.end(),
                            otherName.begin(),
                            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
                        );

                        if (otherName != "rect")
                            continue;

                        AABB wallBox(trO->x, trO->y, rbO->width, rbO->height);
                        if (Collision::CheckCollisionRectToRect(futureBox, wallBox))
                        {
                            collisionDetected = true;
                            break;
                        }
                    }

                    // If we hit a wall, flip direction and pause briefly
                    if (collisionDetected)
                    {
                        ai->dir *= -1.0f;
                        ai->pauseTimer = pauseDuration;
                    }
                    else
                    {
                        // Apply the movement
                        tr->x = newX;
                        tr->y = newY;
                    }

                    // Clamp patrol range and flip direction at the edges
                    if (tr->x < -patrolRange)
                    {
                        tr->x = -patrolRange;
                        ai->dir = 1.0f;
                        ai->pauseTimer = pauseDuration;
                    }
                    if (tr->x > patrolRange)
                    {
                        tr->x = patrolRange;
                        ai->dir = -1.0f;
                        ai->pauseTimer = pauseDuration;
                    }
                    // Optional: ensure a patrol/idle animation when not attacking
                    PlayAnimationIfAvailable(enemy, "idle");
                }
            }
        );

        // ---------------------------------------------------------------------
        // Attack leaf: chase the player and spawn a hitbox when in range.
        // Also drives enemy attack/idle animations.
        // Extended to support ranged enemies (projectiles) based on EnemyTypeComponent.
        // ---------------------------------------------------------------------
        auto AttackLeaf = std::make_unique<DecisionNode>(
            nullptr,
            nullptr,
            nullptr,
            [enemyID, logic](float dt)
            {
                GOC* enemy = FACTORY->GetObjectWithId(enemyID);
                if (!enemy)
                    return;

                auto* attack = enemy->GetComponentType<EnemyAttackComponent>(ComponentTypeId::CT_EnemyAttackComponent);
                auto* rb = enemy->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
                auto* tr = enemy->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
                auto* ai = enemy->GetComponentType<EnemyDecisionTreeComponent>(ComponentTypeId::CT_EnemyDecisionTreeComponent);
                auto* typeComp = enemy->GetComponentType<EnemyTypeComponent>(ComponentTypeId::CT_EnemyTypeComponent);
                auto* audio = enemy->GetComponentType<AudioComponent>(ComponentTypeId::CT_AudioComponent);

                if (!attack || !rb || !tr || !ai)
                    return;

                GOC* player = nullptr;
                ai->chaseSpeed = 0.05f;

                auto& objects = FACTORY->Objects();
                for (auto& kv : objects)
                {
                    GOC* goc = kv.second.get();
                    if (!goc)
                        continue;

                    auto* base = goc->GetComponent(ComponentTypeId::CT_PlayerComponent);
                    if (base)
                    {
                        player = goc;
                        break;
                    }
                }

                if (!player)
                    return;

                auto* trplayer = player->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
                if (!trplayer)
                    return;

                // Direction and distance to player
                float dx = trplayer->x - tr->x;
                float dy = trplayer->y - tr->y;
                float distance = std::sqrt(dx * dx + dy * dy);

                const float speed = 1.0f;
                const float accel = 2.0f;

                // Determine behavior based on Type (melee vs ranged)
                bool isRanged = (typeComp && typeComp->Etype == EnemyTypeComponent::EnemyType::ranged);

                // Keep ranged enemies a bit closer so they don't aggro from too far away
                float stopDistance = isRanged ? 1.0f : 0.1f;

                // Smoothly move towards the player
                if (distance > stopDistance)
                {
                    float norm = (distance > 0.001f) ? distance : 1.0f;
                    float targetVX = (dx / norm) * speed;
                    float targetVY = (dy / norm) * speed;

                    // Smooth approach using simple linear interpolation
                    rb->velX += (targetVX - rb->velX) * std::min(accel * dt, 1.0f);
                    rb->velY += (targetVY - rb->velY) * std::min(accel * dt, 1.0f);
                }
                else
                {
                    // Slow down when very close to the player
                    rb->velX *= 0.5f;
                    rb->velY *= 0.5f;
                }

                // Update attack timer and spawn hitbox if ready
                attack->attack_timer += dt;

                // Determine facing direction based on player position
                ai->facing = (dx < 0.0f) ? Facing::LEFT : Facing::RIGHT;

                if (attack->attack_timer >= attack->attack_speed)
                {
                    // Check range before attacking. Ranged enemies should only fire when much closer.
                    bool canAttack = isRanged ? (distance < 3.5f) : (distance < 0.8f);

                    if (canAttack)
                    {
                        // Note: For melee, we check !attack->hitbox->active. For ranged, we just fire.
                        bool meleeReady = !isRanged && !attack->hitbox->active;
                        bool rangedReady = isRanged; // Ranged fires based on timer solely

                        if (meleeReady || rangedReady)
                        {
                            attack->attack_timer = 0.0f;

                            if (isRanged)
                            {
                                // --- Ranged Attack: Spawn Projectile ---
                                float norm = (distance > 0.001f) ? distance : 1.0f;
                                float dirX = dx / norm;
                                float dirY = dy / norm;

                                // Spawn offset
                                float spawnX = tr->x;
                                float spawnY = tr->y;

                                logic->hitBoxSystem->SpawnProjectile(
                                    enemy,
                                    spawnX, spawnY,
                                    dirX, dirY,
                                    0.2f,       // Projectile speed 
                                    0.3f, 0.15f, // Size
                                    static_cast<float>(attack->damage),
                                    3.0f,        // Duration
                                    HitBoxComponent::Team::Enemy
                                );
                                if (audio)
                                {
                                    audio->TriggerSound("EnemyAttack");
                                }
                                PlayAnimationIfAvailable(enemy, "rangeattack", true);
                                attack->attack_timer = -3.0f;
                            }
                            else
                            {
                                // --- Melee Attack: Spawn Hitbox ---
                                attack->hitbox->active = true;
                                float direction = (ai->facing == Facing::LEFT) ? -1.0f : 1.0f;

                                float hbWidth = rb->width * 1.2f;
                                float hbHeight = rb->height * 0.8f;

                                // Spawn X just outside enemy's hitbox
                                float spawnX = tr->x + (direction * hbWidth * 0.25f);
                                float spawnY = tr->y; // centered vertically

                                attack->hitbox->duration = GetAnimationDuration(enemy, "slashattack");

                                logic->hitBoxSystem->SpawnHitBox(
                                    enemy,
                                    spawnX,
                                    spawnY,
                                    hbWidth,
                                    hbHeight,
                                    static_cast<float>(attack->damage),
                                    attack->hitbox->duration,
                                    HitBoxComponent::Team::Enemy
                                );
                                if (audio)
                                {
                                    audio->TriggerSound("EnemyAttack");
                                }

                                // Play attack animation when slashing
                                PlayAnimationIfAvailable(enemy, "slashattack", true);
                            }
                        }
                    }
                }

                // Update hitbox lifetime and return to idle animation when not attacking
                // (Only relevant for melee hitboxes attached to the enemy)
                if (!isRanged && attack->hitbox->active)
                {
                    attack->hitboxElapsed += dt;
                    if (attack->hitboxElapsed >= attack->hitbox->duration)
                    {
                        attack->hitbox->active = false;
                        attack->hitboxElapsed = 0.0f;
                        PlayAnimationIfAvailable(enemy, "idle");
                    }
                }
                else if (isRanged && attack->attack_timer > 0.5f)
                {
                    // Simple fallback for ranged to go back to idle after shooting
                    PlayAnimationIfAvailable(enemy, "idle");
                }

                // Update chase duration state
                if (distance > (isRanged ? 4.0f : 0.5f))
                {
                    ai->chaseTimer += dt;
                    if (ai->chaseTimer >= ai->maxChaseDuration)
                    {
                        ai->hasSeenPlayer = false;
                        ai->chaseTimer = 0.0f;
                    }
                }
                else
                {
                    ai->chaseTimer = 0.0f;   // reset while player is near
                    ai->hasSeenPlayer = true;
                }
            }
        );

        // ---------------------------------------------------------------------
        // Root decision: if the player is near (or has been seen recently),
        // go to the attack branch, otherwise patrol.
        // ---------------------------------------------------------------------
        auto root = std::make_unique<DecisionNode>(
            [enemyID](float)
            {
                GOC* enemy = FACTORY->GetObjectWithId(enemyID);
                if (!enemy)
                    return false;

                auto* ai = enemy->GetComponentType<EnemyDecisionTreeComponent>(ComponentTypeId::CT_EnemyDecisionTreeComponent);
                if (!ai)
                    return false;

                // Refresh "seen player" state based on proximity
                // Reduced detection radius so enemies don't aggro from across the arena
                if (IsPlayerNear(enemy, 0.2f))
                {
                    ai->hasSeenPlayer = true;
                    ai->chaseTimer = 0.0f;
                }

                return ai->hasSeenPlayer;
            },
            std::move(AttackLeaf),
            std::move(patrolLeaf),
            [](float) { /* Optional: root on-run hook (unused) */ }
        );

        return std::make_unique<DecisionTree>(std::move(root));
    }

    /*****************************************************************************************
    \brief
    Updates and executes the default decision tree for a given enemy.

    \param enemy
    Pointer to the enemy game object whose decision tree should be evaluated.
    \param dt
    Floating-point delta time or contextual value for logic evaluation.
    \param logic
    Pointer to LogicSystem, used during attack leaf execution.

    \details
    Initializes the decision tree if it does not yet exist, then runs it to determine
    and perform the appropriate behavior.
    *****************************************************************************************/
    void UpdateDefaultEnemyTree(GOC* enemy, float dt, LogicSystem* logic)
    {
        if (!enemy)
            return;

        auto* enemyDecisionTree =
            enemy->GetComponentType<EnemyDecisionTreeComponent>(ComponentTypeId::CT_EnemyDecisionTreeComponent);
        if (!enemyDecisionTree)
            return;

        // If the enemy is dead, avoid running the decision tree so death animations are not overridden.
        if (auto* health = enemy->GetComponentType<EnemyHealthComponent>(ComponentTypeId::CT_EnemyHealthComponent))
        {
            if (health->enemyHealth <= 0)
            {
                if (auto* rb = enemy->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent))
                {
                    rb->velX = 0.0f;
                    rb->velY = 0.0f;
                }
                return;
            }
        }

        // Lazy initialization of the decision tree
        if (!enemyDecisionTree->tree)
        {
            enemyDecisionTree->tree = CreateDefaultEnemyTree(enemy, logic);
        }

        // Run the tree with delta time
        if (enemyDecisionTree->tree)
        {
            enemyDecisionTree->tree->run(dt);
        }
    }
}
