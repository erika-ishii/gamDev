#include "AiSystem.h"
#include <iostream>

Framework::AiSystem::AiSystem(gfx::Window& window) : window(&window) {}

void Framework::AiSystem::Initialize()
{
    std::cout << "[AiSystem] Initialized.\n";
}

void Framework::AiSystem::Update(float dt)
{
    (void)dt;
    auto& objects = FACTORY->Objects();
    for (auto& kv : objects)
    {
        GOC* goc = kv.second.get();
        if (!goc) continue;

        auto* base = goc->GetComponent(ComponentTypeId::CT_EnemyDecisionTreeComponent);
        if (!base) continue; // skip, don’t return

        auto* ai = static_cast<EnemyDecisionTreeComponent*>(base);
        if (ai && ai->tree)
        {
            ai->tree->run();
        }
    }
}


void Framework::AiSystem::draw()
{
}

void Framework::AiSystem::Shutdown()
{
    std::cout << "[AiSystem] Shutdown.\n";
}
