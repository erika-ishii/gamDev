#include "HitBoxSystem.h"
#include "Composition/Component.h"
#include "LogicSystem.h"
#include <iostream>
#include "Component/HitBoxComponent.h"

namespace Framework
{
	HitBoxSystem::HitBoxSystem(LogicSystem& logicRef) : logic(logicRef)
	{

	}

	HitBoxSystem::~HitBoxSystem()
	{
		Shutdown();
	}

	void HitBoxSystem::Initialize()
	{
		activeHitBoxes.clear();
	}

	void HitBoxSystem::Shutdown()
	{
		activeHitBoxes.clear();
	}

	void HitBoxSystem::SpawnHitBox(GameObjectComposition* attacker,
		float targetX, float targetY,
		float width, float height,
		float damage,
		float duration)
	{
		if (!attacker)
			return;

		auto newhitbox = std::make_unique<HitBoxComponent>();
		newhitbox->spawnX = targetX;
		newhitbox->spawnY = targetY;
		newhitbox->width = width;
		newhitbox->height = height;
		newhitbox->damage = damage;
		newhitbox->duration = duration;
		newhitbox->owner = attacker;
		newhitbox->ActivateHurtBox();

		ActiveHitBox active;
		active.hitbox = std::move(newhitbox);
		active.owner = attacker;
		active.timer = duration;

		activeHitBoxes.push_back(std::move(active));
	}

	void HitBoxSystem::Update(float dt)
	{
		for (auto it = activeHitBoxes.begin(); it != activeHitBoxes.end();)
		{
			bool hit = false;
			it->timer -= dt;

			AABB hitboxAABB(
				it->hitbox->spawnX,
				it->hitbox->spawnY,
				it->hitbox->width,
				it->hitbox->height
			);
			for (auto* obj : logic.LevelObjects())
			{
				if (!obj || obj == it->owner)
					continue;

				auto* hitbox = obj->GetComponentType<HitBoxComponent>(ComponentTypeId::CT_HitBoxComponent);

				if (hitbox && hitbox->active)
				{
					AABB hitboxAABB(it->hitbox->spawnX, it->hitbox->spawnY,
									it->hitbox->width, it->hitbox->height);

					AABB hurtboxAABB(hitbox->spawnX, hitbox->spawnY,
									 hitbox->width, hitbox->height);

					if (Collision::CheckCollisionRectToRect(hitboxAABB, hurtboxAABB))
					{
						std::cout << "Hit detected! ("
							<< hitboxAABB.min.getX() << ", " << hitboxAABB.min.getY() << ") vs ("
							<< hurtboxAABB.min.getX() << ", " << hurtboxAABB.min.getY() << ")\n";
						hit = true;
						break;
					}
				}
			}

			if (hit || it->timer <= 0.f)
			{
				it = activeHitBoxes.erase(it);
			}
			else // Remove hitbox if duration expires
			{
				it++;
			}
		}
	}
}