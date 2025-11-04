#pragma once

#include "Factory/Factory.h"
#include "Composition/PrefabManager.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/CircleRenderComponent.h"
#include "Component/SpriteComponent.h"

#include "Component/PlayerComponent.h"
#include "Component/PlayerAttackComponent.h"
#include "Component/PlayerHealthComponent.h"

#include "Component/EnemyComponent.h"
#include "Component/EnemyAttackComponent.h"
#include "Component/EnemyDecisionTreeComponent.h"
#include "Component/EnemyHealthComponent.h"
#include "Component/EnemyTypeComponent.h"

#include "Physics/Dynamics/RigidBodyComponent.h"
#include <Serialization/JsonSerialization.h>
#include "InputSystem.h"


#include "Config/WindowConfig.h"
#include "Debug/CrashLogger.hpp"

#include "Graphics/Window.hpp"
#include "Physics/Collision/Collision.h"
#include "Component/HurtBoxComponent.h"

#include "../../Sandbox/MyGame/MathUtils.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <memory>
#include <vector>
namespace gfx { class Window; }

class CrashLogger;
namespace Framework {

	class InputsSyetm;
	class GameObjectFactory;
	

	class LogicSystem :public Framework::ISystem {
	public:
		struct AnimationInfo {
			int frame{ 0 };
			int columns{ 1 };
			int rows{ 1 };
			bool running{ false };
		};

		struct CollisionInfo {
			bool playerValid{ false };
			bool targetValid{ false };
			AABB player{ 0.f, 0.f, 0.f, 0.f };
			AABB target{ 0.f, 0.f, 0.f, 0.f };
		};

		LogicSystem(gfx::Window& window, InputSystem& input);
		~LogicSystem();
		void Initialize() override;

		void Update(float dt) override;

		void Shutdown() override;

		void ReloadLevel();

		GameObjectFactory* Factory() const { return factory.get(); }
		const std::vector<GOC*>& LevelObjects() const { return levelObjects; }

		const AnimationInfo& Animation() const { return animInfo; }
		const CollisionInfo& Collision() const { return collisionInfo; }

		float PlayerBaseWidth() const { return rectBaseW; }
		float PlayerBaseHeight() const { return rectBaseH; }
		float PlayerScale() const { return rectScale; }
		bool GetPlayerWorldPosition(float& outX, float& outY) const;
		int ScreenWidth() const { return screenW; }
		int ScreenHeight() const { return screenH; }


		std::string GetName() override{ return "LogicSystem"; }

	private:
		enum class AnimState { Idle, Run };

		struct AnimConfig {
			int cols;
			int rows;
			int frames;
			float fps;
		};

		const AnimConfig& CurrentConfig() const;

		bool IsAlive(GOC* obj) const;
		void CachePlayerSize();
		void RefreshLevelReferences();
		void UpdateAnimation(float dt, bool wantRun);

		gfx::Window* window;
		InputSystem& input;

		std::unique_ptr<GameObjectFactory> factory;
		std::vector<GOC*> levelObjects;

		GOC* player{ nullptr };
		GOC* collisionTarget{ nullptr };

		float rectScale{ 1.f };
		float rectBaseW{ 0.5f };
		float rectBaseH{ 0.5f };

		AnimState animState{ AnimState::Idle };
		AnimConfig idleConfig{ 5, 1, 5, 6.f };
		AnimConfig runConfig{ 8, 1, 8, 10.f };
		int frame{ 0 };
		float frameClock{ 0.f };
		AnimationInfo animInfo{};

		CollisionInfo collisionInfo{};

		int screenW{ 800 };
		int screenH{ 600 };

		bool captured{ false };
		std::unique_ptr<CrashLogger> crashLogger;
	};
}