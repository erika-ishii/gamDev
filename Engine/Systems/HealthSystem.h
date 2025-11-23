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

    private:
        gfx::Window* window;          // Non-owning window handle used by the system.
        std::vector<GOCId> gameObjectIds;
        std::unordered_map<GOCId, float> deathTimers;
    };

} // namespace Framework
