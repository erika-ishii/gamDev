#include "PhysicSystem.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <string>

namespace Framework {
    PhysicSystem::PhysicSystem(LogicSystem& logic)
        : logic(logic) {
    }
    void PhysicSystem::Initialize() {


    }
    void PhysicSystem::Update(float dt)
    {
		/*
        //for (GOC)
        const auto& info = logic.Collision();
        if (info.playerValid && info.targetValid)
        {
            if (Collision::CheckCollisionRectToRect(info.player, info.target))
            {
                std::cout << "Collision detected!" << std::endl;
            }
        }
        */
		auto& objects = FACTORY->Objects();
		//Physics System aabb
		for (auto& [id, obj] : objects)
		{
			if (!obj)
				continue;

			auto* rb = obj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
			auto* tr = obj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);

			if (!rb || !tr)
				continue;

			// Update pos
			float newX = tr->x + rb->velX * dt;
			float newY = tr->y + rb->velY * dt;

			AABB playerBoxX(newX, tr->y, rb->width, rb->height);
			AABB playerBoxY(tr->x, newY, rb->width, rb->height);

			const std::string& objectLayer = obj->GetLayerName();
			// Horizontal Collision
			for (auto& [otherId, otherObj] : objects)
			{
				if (!otherObj || otherObj == obj)
					continue;
				// check layer
				if (otherObj->GetLayerName() != objectLayer)
					continue;

				auto* rbO = otherObj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
				auto* trO = otherObj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);

				if (!rbO || !trO)
					continue;

				// Only check walls (case-insensitive comparison)
				std::string otherName = otherObj->GetObjectName();
				std::transform(otherName.begin(), otherName.end(), otherName.begin(),
					[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
				if (otherName != "rect")
					continue;

				AABB wallBox(trO->x, trO->y, rbO->width, rbO->height);

				if (Collision::CheckCollisionRectToRect(playerBoxX, wallBox))
				{
					newX = tr->x;
				}
				if (Collision::CheckCollisionRectToRect(playerBoxY, wallBox))
				{
					newY = tr->y;
				}
			}

			tr->x = newX;
			tr->y = newY;
		}
        //HitBox Collision
		for (auto& [id, obj] : objects)
		{
            if (!obj)
                continue;

            auto* enemyAttack = obj->GetComponentType<EnemyAttackComponent>(ComponentTypeId::CT_EnemyAttackComponent);
            if (!enemyAttack || !enemyAttack->hitbox || !enemyAttack->hitbox->active)
                continue;

            // Build AABB for the hitbox
            AABB enemyHitBox(
                enemyAttack->hitbox->spawnX,
                enemyAttack->hitbox->spawnY,
                enemyAttack->hitbox->width,
                enemyAttack->hitbox->height
            );

            // Compare with all players
            for (auto& [pid, playerObj] : FACTORY->Objects())
            {
                if (!playerObj)
                    continue;

                auto* playerComp = playerObj->GetComponent(ComponentTypeId::CT_PlayerComponent);
                if (!playerComp)
                    continue;

                auto* trP = playerObj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
                auto* rbP = playerObj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
                if (!trP || !rbP)
                    continue;

                AABB playerBox(trP->x, trP->y, rbP->width, rbP->height);

                if (Collision::CheckCollisionRectToRect(enemyHitBox, playerBox))
                {
                    std::cout << "Player hit! Took " << enemyAttack->damage
                        << " damage from enemy at ("
                        << enemyAttack->hitbox->spawnX << ", "
                        << enemyAttack->hitbox->spawnY << ")\n";

                    // Reduce health if component exists
                    if (auto* health = playerObj->GetComponentType<PlayerHealthComponent>(
                        ComponentTypeId::CT_PlayerHealthComponent))
                    {
                        health->playerHealth -= enemyAttack->damage;
                        if (health->playerHealth < 0) health->playerHealth = 0;
                        std::cout << "Player Health: " << health->playerHealth << "\n";
                    }

                    // Disable hitbox so it doesn’t deal damage every frame
                    enemyAttack->hitbox->DeactivateHurtBox();
                }
            }
		}
		
    }

    void PhysicSystem::Shutdown() {
        
    }
}

