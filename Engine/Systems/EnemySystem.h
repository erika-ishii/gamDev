#pragma once
#include "Common/System.h"
#include "Factory/Factory.h"
#include "../Systems/AiSystem.h"
#include "../../Engine/Graphics/Window.hpp"
#include "Serialization/Serialization.h"
//Enemy Components
#include "../Component/RenderComponent.h"
#include "../Component/SpriteComponent.h"
#include "../Component/TransformComponent.h"
#include "../Component/EnemyComponent.h"
#include "../Component/EnemyAttackComponent.h"
#include "../Component/EnemyDecisionTreeComponent.h"
#include "../Component/EnemyHealthComponent.h"
#include "../Component/EnemyTypeComponent.h"
namespace Framework {
	class EnemySystem :public Framework::ISystem {
	public:
		explicit EnemySystem(gfx::Window& window);
		void Initialize() override;
		void Update(float dt) override;
		void draw() override;
		void Shutdown() override;
		std::string GetName() override{ return "EnemySystem"; }
	private:
		gfx::Window* window;
        std::vector<GOC*> enemies;
	};

}