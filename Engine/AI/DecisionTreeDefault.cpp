#include "DecisionTreeDefault.h"
namespace Framework
{
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

    std::unique_ptr<DecisionTree> CreateDefaultEnemyTree(GOC* enemy)
    {
        if (!enemy) return nullptr;
        const GOCId enemyID = enemy->GetId();
 
        //Patrol Leaf
        auto patrolLeaf = std::make_unique<DecisionNode>
        (nullptr, nullptr,nullptr,[enemyID](float dt)
        { 
            static float dir = 1.0f;
            GOC* enemy = FACTORY->GetObjectWithId(enemyID);
            if (!enemy) return;
            auto* rb = enemy->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
            auto* tr = enemy->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
            if (rb && tr)
            {
                const float patrolSpeed = 0.5f;
                const float patrolRange = 1.0f;

                rb->velX = patrolSpeed * dir;
                rb->velY = 0.0f;

                tr->x += rb->velX * dt;   
                tr->y += rb->velY * dt;

                if (tr->x < -patrolRange) dir = 1.0f;
                if (tr->x > patrolRange) dir = -1.0f;
            }
        }
        );
       
        //Attack Leaf
        auto AttackLeaf = std::make_unique<DecisionNode>
        (nullptr, nullptr,nullptr,[enemyID](float)
        { 
            GOC* enemy = FACTORY->GetObjectWithId(enemyID);
            if (!enemy) return;
            auto* attack = enemy->GetComponentType<EnemyAttackComponent>(ComponentTypeId::CT_EnemyAttackComponent);
            if (attack )
            {
                std::cout << "Enemy attacks with " << attack->damage << " damage!\n";
            }
        }
        );

        //Root node - CAPTURE enemyID
        auto root = std::make_unique<DecisionNode>(
            [enemyID](float) {
                GOC* enemy = FACTORY->GetObjectWithId(enemyID);
                if (!enemy) return false;
                return IsPlayerNear(enemy, 0.5f);
            },
            std::move(AttackLeaf),
            std::move(patrolLeaf),
            [](float) {}   // empty lambda for leaf action
        );

        return std::make_unique<DecisionTree>(std::move(root));

    }

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