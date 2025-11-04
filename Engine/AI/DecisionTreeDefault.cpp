/*********************************************************************************************
 \file      DecisionTreeDefault.cpp
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

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
            All content � 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#include "DecisionTreeDefault.h"
namespace Framework
{
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
        if (!enemy) return false;
        GOC* player = nullptr;
        auto& objects = FACTORY->Objects();
        for (auto& kv : objects)
        {
            GOC* goc = kv.second.get();
            if (!goc) continue;
            auto* base = goc->GetComponent(ComponentTypeId::CT_PlayerComponent);
            if (base){player=goc; break;}
        }
        if (!player) return false;
        //Direct access via <>
        auto* enemyTra = enemy->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
        auto* playerTra = player->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
        if (!enemyTra || !playerTra) return false;
        float dx = enemyTra->x- playerTra->x;
        float dy = enemyTra->y- playerTra->y;
        return (dx*dx + dy*dy) <= radius*radius;
    }
    /*****************************************************************************************
    \brief
    Creates a default decision tree for basic enemy AI behavior.

    \param enemy
    Pointer to the enemy game object that will own the decision tree.

    \return
    A unique pointer to a newly created DecisionTree containing patrol and attack logic.
    *****************************************************************************************/
    std::unique_ptr<DecisionTree> CreateDefaultEnemyTree(GOC* enemy)
    {
        if (!enemy) return nullptr;
        const GOCId enemyID = enemy->GetId();
 
        //Patrol Leaf
        auto patrolLeaf = std::make_unique<DecisionNode>
        (nullptr, nullptr,nullptr,[enemyID](float dt)
        { 
            
            GOC* enemy = FACTORY->GetObjectWithId(enemyID);
            if (!enemy) return;
            auto* rb = enemy->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
            auto* tr = enemy->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
            auto* ai = enemy->GetComponentType<EnemyDecisionTreeComponent>(ComponentTypeId::CT_EnemyDecisionTreeComponent);
              if (rb && tr && ai)
            {
                const float patrolSpeed = 0.2f;
                const float patrolRange = 0.5f;
                const float pauseDuration = 2.0f;
                
                if (ai->pauseTimer > 0.0f)
                {
                    ai->pauseTimer -= dt;
                    rb->velX = 0.0f;
                    return;
                }
                rb->velX = patrolSpeed * ai->dir;
                rb->velY = 0.0f;
                float newX = tr->x + rb->velX * dt;
                float newY = tr->y;
                AABB futureBox(newX, newY, rb->width, rb->height);
                bool collisionDetected = false;
                auto& objects = FACTORY->Objects();
                for (auto& [otherId, otherObj] : objects)
                {
                    auto* rbO = otherObj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
                    auto* trO = otherObj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
                    if (!rbO || !trO) continue;
                    std::string otherName = otherObj->GetObjectName();
                    std::transform(otherName.begin(), otherName.end(), otherName.begin(),
                        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                    if (otherName != "rect") continue;
                    AABB wallBox(trO->x, trO->y, rbO->width, rbO->height);
                    if (Collision::CheckCollisionRectToRect(futureBox, wallBox)) {
                        collisionDetected = true;
                        break;
                    }
                }

                if (collisionDetected) 
                {
                    ai->dir *= -1.0f;
                    ai->pauseTimer = pauseDuration;
                }
                else 
                {
                    tr->x = newX;
                    tr->y = newY;
                }


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
            }
        }
        );
       
        //Attack Leaf
        auto AttackLeaf = std::make_unique<DecisionNode>
        (nullptr, nullptr,nullptr,[enemyID](float dt)
        { 
            GOC* enemy = FACTORY->GetObjectWithId(enemyID);
            if (!enemy) return;
            auto* attack = enemy->GetComponentType<EnemyAttackComponent>(ComponentTypeId::CT_EnemyAttackComponent);
            auto* rb = enemy->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
            auto* tr = enemy->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
            auto* ai = enemy->GetComponentType<EnemyDecisionTreeComponent>(ComponentTypeId::CT_EnemyDecisionTreeComponent);
            if (!attack || !rb || !tr) return;
            GOC* player = nullptr;
            if (ai->chaseSpeed < 0.0f) 
            { 
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<float> dist(0.3f, 0.5f);
                ai->chaseSpeed = dist(gen);
            }

            auto& objects = FACTORY->Objects();
            for (auto& kv : objects)
            {
                GOC* goc = kv.second.get();
                if (!goc) continue;
                auto* base = goc->GetComponent(ComponentTypeId::CT_PlayerComponent);
                if (base){player=goc; break;}
            }
            if (!player) return;
            
            auto*trplayer= player->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
            float speed = 0.3f;
            float dx = trplayer->x -tr->x;
            float dy = trplayer->y -tr->y;
            float distance = std::sqrt(dx*dx + dy*dy);
            if (distance > 0.1f)
            {
                float vx = 0.0f;
                float vy = 0.0f;
                const float nudge = 0.2f;
                if (std::fabs(dx) > 0.01f)  
                {
                    vx = (dx > 0.0f ? speed : -speed);
                    vy = 0.0f;
                }
                else if (std::fabs(dy) > 0.01f)  
                {
                    vx = 0.0f;
                    vy = (dy > 0.0f ? speed : -speed);
                }
                if (std::fabs(vx) < 0.01f && std::fabs(vy) < 0.01f)
                {
                    vx = (dx > 0.0f ? -nudge : nudge);
                    vy = (dy > 0.0f ? -nudge : nudge);
                }

                rb->velX = vx;
                rb->velY = vy;
                bool collision = false; 
                tr->x += rb->velX * dt;
                tr->y += rb->velY * dt;
            }

            static float attackTimer = 0.0f;
            const float attackInterval = 1.5f;
            attackTimer += dt;
            if (attackTimer >= attackInterval)
            {attackTimer=0.0f; std::cout << "Enemy attacks with " << attack->damage << " damage!\n"; }
            ai->chaseTimer += dt;
            if (ai->chaseTimer >= ai->maxChaseDuration) {
                ai->hasSeenPlayer = false;
                ai->chaseTimer = 0.0f;
            }
        }
        );

        //Root Node
        auto root = std::make_unique<DecisionNode>(
            [enemyID](float) 
            {
                GOC* enemy = FACTORY->GetObjectWithId(enemyID);
                if (!enemy) return false;
                auto* ai = enemy->GetComponentType<EnemyDecisionTreeComponent>(ComponentTypeId::CT_EnemyDecisionTreeComponent);
                if (IsPlayerNear(enemy, 0.2f))
                {
                    ai->hasSeenPlayer = true;
                    ai->chaseTimer = 0.0f;
                }
                return ai->hasSeenPlayer;
            },
            std::move(AttackLeaf),
            std::move(patrolLeaf),
            [](float) 
            {}   
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

    \details
    Initializes the decision tree if it does not yet exist, then runs it to determine
    and perform the appropriate behavior.
    *****************************************************************************************/
    void UpdateDefaultEnemyTree(GOC* enemy, float dt)
    {
        if (!enemy) return;

        auto* enemyDecisionTree = enemy->GetComponentType<EnemyDecisionTreeComponent>(ComponentTypeId::CT_EnemyDecisionTreeComponent);
        if (!enemyDecisionTree) return;

        // Lazy initialization
        if (!enemyDecisionTree->tree)
            enemyDecisionTree->tree = CreateDefaultEnemyTree(enemy);

        // Run the tree with delta time
        if (enemyDecisionTree->tree)
            enemyDecisionTree->tree->run(dt);
    }
}