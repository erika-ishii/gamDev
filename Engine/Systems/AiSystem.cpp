#include "AiSystem.h"
#include <iostream>

Framework::AiSystem::AiSystem(gfx::Window& window) : window(&window) {}

void Framework::AiSystem::Initialize()
{
    std::cout << "[AiSystem] Initialized.\n";
}

void Framework::AiSystem::Update(float dt)
{
    auto& objects = FACTORY->Objects();

    for (auto& [id, gocPtr] : objects)
    {
        if (!gocPtr) continue;
        GOC* goc = gocPtr.get();

        // Run AI tree safely with dt
        UpdateDefaultEnemyTree(goc, dt);
    }

}


void Framework::AiSystem::draw()
{
}

void Framework::AiSystem::Shutdown()
{
    std::cout << "[AiSystem] Shutdown.\n";
}
