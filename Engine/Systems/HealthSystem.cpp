#include "HealthSystem.h"
#include "Factory/Factory.h"
#include <algorithm>
#include <iostream>

namespace Framework
{
    HealthSystem::HealthSystem(gfx::Window& win)
        : window(&win)
    {
    }

    void HealthSystem::Initialize()
    {
        // Track by ID instead of raw pointers to avoid dangling references.
        gameObjectIds.clear();

        for (auto& [id, goc] : FACTORY->Objects())
        {
            if (!goc)
                continue;

            auto* enemyHealth =
                goc->GetComponentType<EnemyHealthComponent>(
                    ComponentTypeId::CT_EnemyHealthComponent);

            auto* playerHealth =
                goc->GetComponentType<PlayerHealthComponent>(
                    ComponentTypeId::CT_PlayerHealthComponent);

            if (enemyHealth || playerHealth)
                gameObjectIds.push_back(id);
        }
    }

    void HealthSystem::Update(float dt)
    {
        (void)dt;

        gameObjectIds.erase(
            std::remove_if(gameObjectIds.begin(), gameObjectIds.end(),
                [](GOCId id) -> bool
                {
                    // Re-resolve each frame; if gone, drop it safely.
                    GOC* goc = FACTORY->GetObjectWithId(id);
                    if (!goc)
                        return true;

                    // Check EnemyHealthComponent
                    if (auto* enemyHealth =
                        goc->GetComponentType<EnemyHealthComponent>(
                            ComponentTypeId::CT_EnemyHealthComponent))
                    {
                        if (enemyHealth->enemyHealth <= 0)
                        {
                            FACTORY->Destroy(goc);
                            std::cout << "[HealthSystem] Enemy "
                                << goc->GetId()
                                << " destroyed.\n";
                            return true; // remove from tracked ids
                        }
                    }

                    // Check PlayerHealthComponent
                    if (auto* playerHealth =
                        goc->GetComponentType<PlayerHealthComponent>(
                            ComponentTypeId::CT_PlayerHealthComponent))
                    {
                        if (playerHealth->playerHealth <= 0)
                        {
                            FACTORY->Destroy(goc);
                            std::cout << "[HealthSystem] Player "
                                << goc->GetId()
                                << " destroyed.\n";
                            return true; // remove from tracked ids
                        }
                    }

                    return false; // keep this id
                }),
            gameObjectIds.end());
    }

    void HealthSystem::draw()
    {
        // Currently no UI/visuals for health.
    }

    void HealthSystem::Shutdown()
    {
        gameObjectIds.clear();
    }

} // namespace Framework
