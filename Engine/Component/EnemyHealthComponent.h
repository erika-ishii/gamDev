#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
namespace Framework
{
    //A data container by itself (Does not do anything)
    class EnemyHealthComponent : public GameComponent 
    {
        public:
            int health{100};
            int maxhealth{ 100 };
            void initialize() override {}
            void SendMessage(Message& m) override { (void)m; }
            void Serialize(ISerializer& s) override 
            {
                if (s.HasKey("health")) StreamRead(s, "health", health);
                if (s.HasKey("maxhealth")) StreamRead(s, "maxhealth", maxhealth);
            }
            std::unique_ptr<GameComponent> Clone() const override 
            {
             auto copy = std::make_unique<EnemyHealthComponent>();
             copy->health = health;
             return copy;
            }
    };
}