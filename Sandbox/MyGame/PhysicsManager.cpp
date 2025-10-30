
#include "PhysicsManager.h"
#include <algorithm>

namespace Framework
{
	void PhysicsManager::Update(float dt)
	{
		auto& objects = FACTORY->Objects();

		for (auto& [id, objPtr] : objects)
		{
			auto* obj = objPtr.get();
			if (!obj) continue;


			auto* rb = obj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
			auto* tr = obj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);

			if (!rb || !tr)
				continue;

			// Update pos
			tr->x += rb->velX * dt;
			tr->y += rb->velY * dt;
		}

		// SImple collision checks for now
		for (auto itA = objects.begin(); itA != objects.end(); itA++)
		{
			auto* objA = itA->second.get();
			if (!objA)
				continue;

			auto* rbA = objA->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
			auto* trA = objA->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);

			if (!rbA || !trA)
				continue;

			AABB boxA(trA->x, trA->y, rbA->width, rbA->height);

			for (auto itB = std::next(itA); itB != objects.end(); itB++)
			{
				auto* objB = itB->second.get();
				if (!objB)
					continue;

				auto* rbB = objB->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
				auto* trB = objB->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);

				if (!rbB || !trB)
					continue;

				AABB boxB(trB->x, trB->y, rbB->width, rbB->height);

				// Check for AABB to AABB collision
				if (Collision::CheckCollisionRectToRect(boxA, boxB))
				{
					// Add collision response later, for now just say colliding
					std::cout << "Collision detected!!!!!";
				}
			}
		}
	}

}
