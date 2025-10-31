#pragma once
#include "Common/System.h"
#include "Factory/Factory.h"
#include "Component/EnemyComponent.h"
#include "Component/EnemyDecisionTreeComponent.h"
#include "../../Engine/Graphics/Window.hpp"
namespace Framework {
	class AiSystem :public Framework::ISystem {
	public:
		explicit AiSystem(gfx::Window& window);
		void Initialize() override;
		void Update(float dt) override;
		void draw() override;
		void Shutdown() override;
		std::string GetName() override{ return "AiSystem"; }
	private:
		gfx::Window* window;
	};

}