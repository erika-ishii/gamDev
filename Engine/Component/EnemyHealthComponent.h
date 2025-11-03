#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
namespace Framework
{
    //A data container by itself (Does not do anything)
    class EnemyHealthComponent : public GameComponent 
    {
        public:
            int enemyHealth{100};
            int enemyMaxhealth{ 100 };
            void initialize() override {}
            void SendMessage(Message& m) override { (void)m; }
            void Serialize(ISerializer& s) override 
            {
                if (s.HasKey("enemyHealth")) StreamRead(s, "enemyHealth", enemyHealth);
                if (s.HasKey("enemyMaxhealth")) StreamRead(s, "enemyMaxhealth", enemyMaxhealth);
            }
            std::unique_ptr<GameComponent> Clone() const override 
            {
             auto copy = std::make_unique<EnemyHealthComponent>();
             copy->enemyHealth = enemyHealth;
             copy->enemyMaxhealth = enemyMaxhealth;
             return copy;
            }
    };
}