#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
namespace Framework
{
    //A data container by itself (Does not do anything)
    class EnemyComponent : public GameComponent 
    {
        public:
            void initialize() override {}
            void SendMessage(Message& m) override { (void)m; }
            void Serialize(ISerializer& s) override { (void)s; }
            std::unique_ptr<GameComponent> Clone() const override 
            {return std::make_unique<EnemyComponent>();}
    };
}