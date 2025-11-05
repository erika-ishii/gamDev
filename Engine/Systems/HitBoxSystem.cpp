/*********************************************************************************************
 \file      HitBoxSystem.cpp
 \par       SofaSpuds
 \author    
 \brief     Spawns and updates short-lived hit boxes for attack interactions.
 \details   This lightweight system manages transient attack volumes (HitBoxComponent):
			- Creation: SpawnHitBox() attaches owner/context and a lifetime timer.
			- Lifetime: Each active hit box counts down; removed when it expires or hits.
			- Collision: On each Update(), checks hit box vs. world hurt boxes (other objs'
			  HitBoxComponent flagged active) via AABB overlap.
			- Integration: Driven by LogicSystem (e.g., mouse click creates a hit box in
			  the player�s facing direction).

			Notes:
			* The same HitBoxComponent struct is reused for both "hit" and "hurt" roles:
			  - Newly spawned (this system) is used as the "hit" volume.
			  - Other objects expose their "hurt" volume when HitBoxComponent::active == true.
			* Collision uses AABB vs AABB through Collision::CheckCollisionRectToRect.
			* This module stores hit boxes internally (not added to factory); they are
			  ephemeral gameplay helpers rather than persistent game objects.
 \copyright
			All content �2025 DigiPen Institute of Technology Singapore.
			All rights reserved.
*********************************************************************************************/

#include "HitBoxSystem.h"
#include "Composition/Component.h"
#include "LogicSystem.h"
#include <iostream>
#include "Component/HitBoxComponent.h"

namespace Framework
{
	/*****************************************************************************************
	  \brief Construct the system with a reference to the driving LogicSystem.
	  \param logicRef Owner that provides access to level objects for collision checks.
	*****************************************************************************************/
	HitBoxSystem::HitBoxSystem(LogicSystem& logicRef) : logic(logicRef)
	{
	}

	/// Ensures containers are cleared on destruction.
	HitBoxSystem::~HitBoxSystem()
	{
		Shutdown();
	}

	/*****************************************************************************************
	  \brief Prepare internal state for a fresh session.
	  \note  Clears any dangling hit boxes that might remain from a previous run.
	*****************************************************************************************/
	void HitBoxSystem::Initialize()
	{
		activeHitBoxes.clear();
	}

	/*****************************************************************************************
	  \brief Release runtime state held by the system.
			 (No heap ownership beyond unique_ptrs inside activeHitBoxes.)
	*****************************************************************************************/
	void HitBoxSystem::Shutdown()
	{
		activeHitBoxes.clear();
	}

	/*****************************************************************************************
	  \brief Spawn a transient hit box owned by \p attacker at (\p targetX, \p targetY).
	  \param attacker  The game object creating the hit (not collided against itself).
	  \param targetX   World X center of the hit box.
	  \param targetY   World Y center of the hit box.
	  \param width     Width of the AABB.
	  \param height    Height of the AABB.
	  \param damage    Damage payload to apply (carried in the component; application external).
	  \param duration  Lifetime in seconds before the hit box auto-expires.
	  \details
		- Allocates a HitBoxComponent and marks it active for collision participation.
		- Stores an internal ActiveHitBox record with countdown timer.
		- The hit is checked against other objects' active hurt boxes during Update().
	*****************************************************************************************/
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
		newhitbox->ActivateHurtBox(); // reuse flag: treat as active volume for collisions

		ActiveHitBox active;
		active.hitbox = std::move(newhitbox);
		active.owner = attacker;
		active.timer = duration;

		activeHitBoxes.push_back(std::move(active));
	}

	/*****************************************************************************************
	  \brief Advance all active hit boxes: tick timers, check collisions, and cull as needed.
	  \param dt Delta time (seconds).
	  \details
		- For each active hit box:
		  * Decrement remaining time.
		  * Build its AABB (center + extents).
		  * Iterate over level objects from LogicSystem; skip the owner.
		  * If an object has an active HitBoxComponent (used as "hurt" box), check overlap.
		  * On first hit, print a debug message and remove the hit box.
		- If no hit occurs, remove the hit box once its timer reaches zero.
	*****************************************************************************************/
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

			// Scan all level objects to find active hurt boxes to test against.
			for (auto* obj : logic.LevelObjects())
			{
				if (!obj || obj == it->owner)
					continue;

				auto* hitbox = obj->GetComponentType<HitBoxComponent>(ComponentTypeId::CT_HitBoxComponent);

				if (hitbox && hitbox->active)
				{
		/*			AABB hitboxAABB(it->hitbox->spawnX, it->hitbox->spawnY,
									it->hitbox->width, it->hitbox->height);*/

					AABB hurtboxAABB(hitbox->spawnX, hitbox->spawnY,
						hitbox->width, hitbox->height);

					if (Collision::CheckCollisionRectToRect(hitboxAABB, hurtboxAABB))
					{
						std::cout << "Hit detected! ("
							<< hitboxAABB.min.getX() << ", " << hitboxAABB.min.getY() << ") vs ("
							<< hurtboxAABB.min.getX() << ", " << hurtboxAABB.min.getY() << ")\n";
						hit = true;
						break; // stop after first contact
					}
				}
			}

			// Remove immediately if hit or expired; otherwise keep ticking.
			if (hit || it->timer <= 0.f)
			{
				it = activeHitBoxes.erase(it);
			}
			else
			{
				++it;
			}
		}
	}
} // namespace Framework
