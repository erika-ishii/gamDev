#include "HealthSystem.h"
#include "Factory/Factory.h"
#include <algorithm>
#include <iostream>

namespace Framework 
{
 HealthSystem::HealthSystem(gfx::Window& win) : window(&win) {}
 void HealthSystem::Initialize()
 {
    gameObjects.clear();
    for (auto& [id, goc] : FACTORY->Objects())
    {
        if (!goc) continue;
        auto* enemyHealth = goc->GetComponentType<EnemyHealthComponent>(ComponentTypeId::CT_EnemyHealthComponent);
        auto* playerHealth = goc->GetComponentType<PlayerHealthComponent>(ComponentTypeId::CT_PlayerHealthComponent);
        if (enemyHealth || playerHealth) gameObjects.push_back(goc.get());
    }
 }
 void HealthSystem::Update(float dt)
 { 
    (void)dt;
    gameObjects.erase(
        std::remove_if(gameObjects.begin(), gameObjects.end(),
            [](GOC* goc) -> bool
            {
                if (!goc) return true; // remove null pointers just in case

                // Check EnemyHealthComponent
                if (auto* enemyHealth = goc->GetComponentType<EnemyHealthComponent>(ComponentTypeId::CT_EnemyHealthComponent))
                {
                    if (enemyHealth->enemyHealth <= 0)
                    {
                        FACTORY->Destroy(goc);
                        std::cout << "[HealthSystem] Enemy " << goc->GetId() << " destroyed.\n";
                        return true; // remove from gameObjects
                    }
                }

                // Check PlayerHealthComponent
                if (auto* playerHealth = goc->GetComponentType<PlayerHealthComponent>(ComponentTypeId::CT_PlayerHealthComponent))
                {
                    if (playerHealth->playerHealth <= 0)
                    {
                        FACTORY->Destroy(goc);
                        std::cout << "[HealthSystem] Player " << goc->GetId() << " destroyed.\n";
                        return true; // remove from gameObjects
                    }
                }

                return false; // keep this object in the vector
            }),
        gameObjects.end());
 }

 void HealthSystem::draw(){}

 void HealthSystem::Shutdown(){gameObjects.clear();}
}