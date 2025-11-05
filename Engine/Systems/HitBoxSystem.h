#pragma once
#include "Composition/Component.h"
#include "LogicSystem.h"
#include "Component/HitBoxComponent.h"

namespace Framework
{

	class GameObjectComposition;
	class HurtBoxComponent;
	class LogicSystem;

	class HitBoxSystem
	{
	public:
		struct ActiveHitBox
		{
			std::unique_ptr<HitBoxComponent> hitbox;
			GameObjectComposition* owner;
			float timer;
		};
		HitBoxSystem(LogicSystem& logic);
		~HitBoxSystem();

		void Initialize();
		void Update(float dt);
		void Shutdown();

		void SpawnHitBox(GameObjectComposition* attacker,
			float targetX, float targetY,
			float width = 0.2f, float height = 0.2f,
			float damage = 1.0f,
			float duration = 0.1f);

		const std::vector<ActiveHitBox>& GetActiveHitBoxes() const { return activeHitBoxes; }

	private:
		LogicSystem& logic;
		std::vector<ActiveHitBox> activeHitBoxes; 
	};



}
