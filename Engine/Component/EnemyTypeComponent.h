#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include <string>
namespace Framework
{
    
    class EnemyTypeComponent : public GameComponent 
    {
        public:
            enum class EnemyType{phyiscal, ranged};
            EnemyType Etype{EnemyType::phyiscal};
            EnemyTypeComponent() = default;
            EnemyTypeComponent(EnemyType t) : Etype(t){}
            void initialize() override {}
            void SendMessage(Message& m) override { (void)m; }
            void Serialize(ISerializer& s) override 
            {
                if (s.HasKey("type"))
                {
                    std::string typeStr;
                    StreamRead(s,"type",typeStr);
                    if(typeStr=="ranged" ||typeStr=="Ranged"){Etype = EnemyType::ranged;}
                    else Etype = EnemyType::phyiscal;
                }
            }
            std::unique_ptr<GameComponent> Clone() const override 
            {
             auto copy = std::make_unique<EnemyTypeComponent>();
             copy->Etype = Etype;
             return copy;
            }
    };
}