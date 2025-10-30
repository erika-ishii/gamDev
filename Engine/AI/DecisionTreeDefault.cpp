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
       
        //Patrol Leaf
        auto patrolLeaf = std::make_unique<DecisionNode>
        (nullptr, nullptr,nullptr,[enemy]()
        { 
            static float dir = 1.0f;
            auto* tr = enemy->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
            if (tr)
            {
                // simple bounds for patrol range
                const float patrolSpeed = 0.5f;
                const float patrolRange = 10.0f;
                tr->x += patrolSpeed * dir;
                // bounds check
                if (tr->x < -patrolRange) dir = 1.0f;
                if (tr->x > patrolRange) dir = -1.0f;
            }
        }
        );
       
        //Attack Leaf
        auto AttackLeaf = std::make_unique<DecisionNode>
        (nullptr, nullptr,nullptr,[enemy]()
        { 
            auto* attack = enemy->GetComponentType<EnemyAttackComponent>(ComponentTypeId::CT_EnemyAttackComponent);
            if (attack )
            {
                std::cout << "Enemy attacks with " << attack->damage << " damage!\n";
            }
        }
        );

        //--Root node: Is Player near?--
        auto root = std::make_unique<DecisionNode>
        ([enemy](){ return IsPlayerNear(enemy, 0.5f);},
        std::move(AttackLeaf),
        std::move(patrolLeaf),
        nullptr
        );

        return std::make_unique<DecisionTree>(std::move(root));

    }
}