#pragma once
#include "Common/System.h"
#include "LogicSystem.h"
#include "Physics/Collision/Collision.h"
namespace Framework {
	class LogicSystem;

	class PhysicSystem :public Framework::ISystem {
	public:
		explicit PhysicSystem(LogicSystem& logic);

		void Initialize() override;

		void Update(float dt) override;

		void Shutdown() override;

		std::string GetName() override{ return "PhysicSystem"; }

	private:
		LogicSystem& logic;

	};
}
