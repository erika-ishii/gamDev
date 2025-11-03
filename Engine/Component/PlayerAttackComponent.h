#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include <iostream>
namespace Framework
{
    
    class PlayerAttackComponent : public GameComponent 
    {
        public:
            int damage{50};
            float attack_speed{1.0f};
            PlayerAttackComponent() = default;
            PlayerAttackComponent(int dmg, float spd) : damage(dmg), attack_speed(spd) {}
            void initialize() override { std::cout << "This object has a PlayerAttackComponent!\n"; }
            void SendMessage(Message& m) override { (void)m; }
            void Serialize(ISerializer& s) override 
            {
                if (s.HasKey("damage")) StreamRead(s,"damage", damage);
                if (s.HasKey("attack_speed")) StreamRead(s,"attack_speed", attack_speed);
            }
            std::unique_ptr<GameComponent> Clone() const override 
            {
               auto copy = std::make_unique<PlayerAttackComponent>();
               copy->damage= damage;
               copy->attack_speed= attack_speed;
               return copy;
            }
    };
}