/*********************************************************************************************
 \file      HitBoxSystem.h
 \par       SofaSpuds
 \author	Ho Jun (h.jun@digipen.edu) - Primary Author, 100%
 \brief     Spawns and manages temporary attack hitboxes for damage interactions.
 \details   Maintains an active list of one-shot hitboxes created by enemy or player attacks.
			Each hitbox persists for a short duration, moves relative to its owner, and
			checks for overlap against hurtboxes on other objects. Once the hitbox expires
			or applies damage, it is removed from the active list. Driven by LogicSystem and
			typically updated once per frame (Initialize → Update(dt) → Shutdown).
 \copyright
			All content ©2025 DigiPen Institute of Technology Singapore.
			All rights reserved.
*********************************************************************************************/
#pragma once
#include "Composition/Component.h"
#include "LogicSystem.h"
#include "Component/HitBoxComponent.h"

namespace Framework
{
	/*************************************************************************************
	  \class  HitBoxSystem
	  \brief  Manages active attack hitboxes and applies damage when collisions occur.
	  \note   Hitboxes are short-lived and removed automatically when their timers expire.
	*************************************************************************************/
	class GameObjectComposition;
	class HurtBoxComponent;
	class LogicSystem;

	class HitBoxSystem
	{
	public:

		/*************************************************************************
		  \struct ActiveHitBox
		  \brief  Represents a currently active attack hitbox instance.
		  \details Holds ownership of a HitBoxComponent, reference to the owner
				   object, and a timer controlling hitbox lifetime.
		*************************************************************************/
		struct ActiveHitBox
		{
			std::unique_ptr<HitBoxComponent> hitbox;
			GameObjectComposition* owner{};
			float timer{};

			// For projectiles
			float velX = 0.f;
			float velY = 0.f;
			bool isProjectile = false;
		};
		/*************************************************************************
		 \brief  Construct the hitbox system with access to logic/scene queries.
		*************************************************************************/
		HitBoxSystem(LogicSystem& logic);

		/*************************************************************************
		  \brief  Release any stored hitboxes and cleanup.
		*************************************************************************/
		~HitBoxSystem();

		/*************************************************************************
		  \brief  Initialize hitbox state/resources (no-op by default).
		*************************************************************************/
		void Initialize();

		/*************************************************************************
		  \brief  Update active hitboxes: countdown timers and apply collisions.
		  \param  dt  Delta time in seconds.
		*************************************************************************/
		void Update(float dt);

		/*************************************************************************
		  \brief  Shutdown the system and clear hitbox memory.
		*************************************************************************/
		void Shutdown();

		/*************************************************************************
		  \brief  Spawn a new attack hitbox at a given target position.
		  \param  attacker  Object creating the hitbox.
		  \param  targetX   X position of the hitbox origin.
		  \param  targetY   Y position of the hitbox origin.
		  \param  width     Width of the hitbox (default 0.2f).
		  \param  height    Height of the hitbox (default 0.2f).
		  \param  damage    Damage this hitbox applies on collision (default 1.0f).
		  \param  duration  Lifetime of hitbox before auto-removal (default 0.1f).
		*************************************************************************/
		void SpawnHitBox(GameObjectComposition* attacker,
			float targetX, float targetY,
			float width = 0.2f, float height = 0.2f,
			float damage = 1.0f,
			float duration = 0.1f);

		void SpawnProjectile(GameObjectComposition* attacker,
			float targetX, float targetY,
			float dirX, float dirY,
			float speed,
			float width = 0.2f, float height = 0.2f,
			float damage = 1.0f,
			float duration = 0.1f);

		/*************************************************************************
		  \brief  Access the current active hitboxes (read-only).
		*************************************************************************/
		const std::vector<ActiveHitBox>& GetActiveHitBoxes() const { return activeHitBoxes; }

	private:
		LogicSystem& logic;							//!< Access to objects and scene queries.
		std::vector<ActiveHitBox> activeHitBoxes;	//!< List of currently active hitboxes.
	};



}
