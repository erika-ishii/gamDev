#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
namespace Framework
{
    //A data container by itself (Does not do anything)
    class PlayerComponent : public GameComponent 
    {
        public:
            void initialize() override { std::cout << "This object has a PlayerComponent!\n"; }
            void SendMessage(Message& m) override { (void)m; }
            void Serialize(ISerializer& s) override { (void)s; }
            std::unique_ptr<GameComponent> Clone() const override 
            {return std::make_unique<PlayerComponent>();}
    };
}