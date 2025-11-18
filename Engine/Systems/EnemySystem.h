/*********************************************************************************************
 \file      EnemySystem.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu)
 \brief     Interface and state for enemy lifecycle control (init, update, draw, shutdown).
 \details   Holds a window reference for size-aware logic and a working list of enemy objects.
            Actual behaviors are implemented in the corresponding .cpp using registered
            enemy components (render, sprite, transform, AI, health, type, attack).
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once
#include "Common/System.h"
#include "Factory/Factory.h"
#include "../Systems/AiSystem.h"
#include "../../Engine/Graphics/Window.hpp"
#include "Serialization/Serialization.h"

// Enemy Components
#include "../Component/RenderComponent.h"
#include "../Component/SpriteComponent.h"
#include "../Component/TransformComponent.h"
#include "../Component/EnemyComponent.h"
#include "../Component/EnemyAttackComponent.h"
#include "../Component/EnemyDecisionTreeComponent.h"
#include "../Component/EnemyHealthComponent.h"
#include "../Component/EnemyTypeComponent.h"

namespace Framework {

    /**
     * \class EnemySystem
     * \brief System stub for managing enemy objects and per-frame enemy logic.
     *
     * The system exposes the standard ISystem hooks. It maintains a pointer to the active
     * window and a cache of enemy object pointers discovered/managed in the implementation.
     */
    class EnemySystem : public Framework::ISystem {
    public:
        // Bind to the active window for resolution/DPI�aware logic.
        explicit EnemySystem(gfx::Window& window);

        // Allocate resources, discover/register enemy entities, and prime state.
        void Initialize() override;

        // Step enemy AI/state for the current frame.
        void Update(float dt) override;

      
        void draw() override;

        // Release resources and clear internal caches.
        void Shutdown() override;

        // System name for diagnostics and registries.
        std::string GetName() override { return "EnemySystem"; }

    private:
        gfx::Window* window;          // Non-owning window handle used by the system.
        std::vector<GOC*> enemies;    // Working set of enemy objects (populated in .cpp).
    };

} // namespace Framework
