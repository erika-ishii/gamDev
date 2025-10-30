#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
namespace Framework
{
    
    class EnemyAttackComponent : public GameComponent 
    {
        public:
            int damage{10};
            float attack_speed{1.0f};
            EnemyAttackComponent() = default;
            EnemyAttackComponent(int dmg, float spd) : damage(dmg), attack_speed(spd) {}
            void initialize() override {}
            void SendMessage(Message& m) override { (void)m; }
            void Serialize(ISerializer& s) override 
            {
                if (s.HasKey("damage")) StreamRead(s,"damage", damage);
                if (s.HasKey("attack_speed")) StreamRead(s,"attack_speed", attack_speed);
            }
            std::unique_ptr<GameComponent> Clone() const override 
            {
               auto copy = std::make_unique<EnemyAttackComponent>();
               copy->damage= damage;
               copy->attack_speed= attack_speed;
               return copy;
            }
    };
}