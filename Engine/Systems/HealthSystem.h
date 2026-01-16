/*********************************************************************************************
 \file      HealthSystem.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Secondary Author, 40%
            yimo.kong (yimo.kong@digipen.edu) - Secondary Author, 40%
            elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - co Author, 20%
 \brief     Implements the HealthSystem responsible for managing player and enemy health,
            handling death timers, triggering death animations, and destroying objects at
            the correct time.
 \details   Responsibilities:
            - Tracks all GameObjectComposition instances that contain health components.
            - Handles enemy death: triggers death animation (if available), waits for both
              animation completion and a minimum timer before destruction.
            - Handles player death: plays death animation, enforces invulnerability timers,
              and destroys the player only after animation + timer finish.
            - Uses stable IDs instead of raw pointers to avoid dangling references.
            - Provides draw() support for player HUD through PlayerHUDComponent.
            - Fully integrates with SpriteAnimationComponent for frame-based animation logic.
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
//Player Components
#include "Component/PlayerComponent.h"
#include "Component/PlayerAttackComponent.h"
#include "Component/PlayerHealthComponent.h"
#include "Component/EnemyComponent.h"
#include "Component/EnemyAttackComponent.h"
#include "Component/EnemyDecisionTreeComponent.h"
#include "Component/EnemyHealthComponent.h"
#include "Component/EnemyTypeComponent.h"
#include "Graphics/PlayerHUD.h"
#include "Physics/Dynamics/RigidBodyComponent.h"

// Enemy Components
#include "../Component/RenderComponent.h"
#include "../Component/SpriteComponent.h"
#include "../Component/TransformComponent.h"
#include "../Component/EnemyComponent.h"
#include "../Component/EnemyAttackComponent.h"
#include "../Component/EnemyDecisionTreeComponent.h"
#include "../Component/EnemyHealthComponent.h"
#include "../Component/EnemyTypeComponent.h"
#include "../Component/SpriteAnimationComponent.h"

#include "Component/AudioComponent.h"
#include <unordered_map>


namespace Framework {


    class HealthSystem : public Framework::ISystem {
    public:
        // Bind to the active window for resolution/DPI�aware logic.
        explicit HealthSystem(gfx::Window& window);

        // Allocate resources, discover/register enemy entities, and prime state.
        void Initialize() override;

        // Step enemy AI/state for the current frame.
        void Update(float dt) override;

      
        void draw() override;

        // Release resources and clear internal caches.
        void Shutdown() override;

        // System name for diagnostics and registries.
        std::string GetName() override { return "HealthSystem"; }
        void RefreshTrackedObjects();

        // Expose player death state so the game loop can react (e.g., show defeat screen).
        bool HasPlayerDied() const { return playerDied; }

        // Clear latched death state when restarting / reloading a level.
        void ClearPlayerDeathFlag() { playerDied = false; }


    private:
        gfx::Window* window;          // Non-owning window handle used by the system.
        std::vector<GOCId> gameObjectIds;
        std::unordered_map<GOCId, float> deathTimers;
        float lastDt = 0.0f;

        bool playerDied = false;      // Latched when the player hits 0 health.
    };

} // namespace Framework
