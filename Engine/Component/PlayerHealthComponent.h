#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include <iostream>
namespace Framework
{
    //A data container by itself (Does not do anything)
    class PlayerHealthComponent : public GameComponent 
    {
        public:
            int playerHealth{100};
            int playerMaxhealth{ 100 };
            void initialize() override { std::cout << "This object has a PlayerHealthComponent!\n"; }
            void SendMessage(Message& m) override { (void)m; }
            void Serialize(ISerializer& s) override 
            {
                if (s.HasKey("playerHealth")) StreamRead(s, "playerHealth", playerHealth);
                if (s.HasKey("playerMaxhealth")) StreamRead(s, "playerMaxhealth", playerMaxhealth);
            }
            std::unique_ptr<GameComponent> Clone() const override 
            {
             auto copy = std::make_unique<PlayerHealthComponent>();
             copy->playerHealth = playerHealth;
             copy->playerMaxhealth = playerMaxhealth;
             return copy;
            }
    };
}