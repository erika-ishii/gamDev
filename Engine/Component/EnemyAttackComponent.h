#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include "Component/HitBoxComponent.h"
#include "Component/TransformComponent.h"
#include <memory>
namespace Framework
{
    
    class EnemyAttackComponent : public GameComponent 
    {
        public:
            int damage{10};
            float attack_speed{1.0f};
            float attack_timer{ 0.0f };
            std::unique_ptr<HitBoxComponent> hitbox;
            EnemyAttackComponent() { hitbox = std::make_unique<HitBoxComponent>(); }
            EnemyAttackComponent(int dmg, float spd) : damage(dmg), attack_speed(spd) { hitbox = std::make_unique<HitBoxComponent>(); }
            void initialize() override { hitbox->initialize(); }
            void SendMessage(Message& m) override { (void)m; }
            void Serialize(ISerializer& s) override 
            {
                if (s.HasKey("damage")) StreamRead(s,"damage", damage);
                if (s.HasKey("attack_speed")) StreamRead(s,"attack_speed", attack_speed);
                if (s.HasKey("hitwidth")) StreamRead(s, "hitwidth", hitbox->width);
                if (s.HasKey("hitheight")) StreamRead(s, "hitheight", hitbox->height);
                if (s.HasKey("hitduration")) StreamRead(s, "hitduration", hitbox->duration);
            }
            std::unique_ptr<GameComponent> Clone() const override 
            {
               auto copy = std::make_unique<EnemyAttackComponent>();
               copy->damage= damage;
               copy->attack_speed= attack_speed;
               copy->hitbox = std::make_unique<HitBoxComponent>(*hitbox);
               return copy;
            }
             void Update(float dt, TransformComponent* tr)
            {
                attack_timer += dt;
                if (attack_timer >= attack_speed)
                {
                    attack_timer = 0.0f;
                    hitbox->active = true;
                    hitbox->spawnX = tr->x;
                    hitbox->spawnY = tr->y;
                    hitbox->duration = 0.15f;
                    std::cout << "Enemy attacked! HitBox active at (" << hitbox->spawnX << ", " << hitbox->spawnY << ")\n";
                }
                if (hitbox->active)
                {
                    hitbox->duration -= dt;
                    if (hitbox->duration <= 0.0f) 
                    {
                        hitbox->DeactivateHurtBox();
                        hitbox->duration = 0.1f;
                    }
                }

            }
    };
}