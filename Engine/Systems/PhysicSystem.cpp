#include "PhysicSystem.h"
#include <iostream>

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

			// Horizontal Collision
			for (auto& [otherId, otherObj] : objects)
			{
				if (!otherObj || otherObj == obj)
					continue;

				auto* rbO = otherObj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
				auto* trO = otherObj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);

				if (!rbO || !trO)
					continue;

				// Only check walls
				if (otherObj->GetObjectName() != "rect")
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

		
    }

    void PhysicSystem::Shutdown() {
        
    }
}

