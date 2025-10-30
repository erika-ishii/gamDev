/*
//Physics.cpp
#include "Physics.h"

namespace Framework
{
	// Physics object
	PhysicsObject::PhysicsObject(float x, float y, float width, float height) 
		: posX(x), posY(y), width(width), height(height)
	{
		// Empty by design
	}

	void PhysicsObject::SetVelocity(float vx, float vy)
	{
		velX = vx;
		velY = vy;
	}

	// So when players press a directional key, it will set the vel of it to a number
	void PhysicsObject::Update(float dt)
	{
		// Using Vector2D
		Vector2D<float> pos(posX, posY);
		Vector2D<float> vel(velX, velY);

		// Update position with velocity
		pos = Vector2D<float>(pos.getX() + vel.getX() * dt, pos.getY() + vel.getY() * dt);

		// Pushback to floats
		posX = pos.getX();
		posY = pos.getY();
	}

	AABB PhysicsObject::GetAABB() const
	{
		return AABB(posX, posY, width, height);
	}

	// Physics System
	void PhysicsSystem::addObject(PhysicsObject* obj) 
	{
		objects.push_back(obj);
	}

	void PhysicsSystem::Update(float dt)
	{
		// Move objects
		for (auto* obj : objects)
			obj->Update(dt);

		// Checks for collision
		for (size_t i = 0; i < objects.size(); i++)
		{
			for (size_t j = 0; j < objects.size(); j++)
			{
				if (Collision::CheckCollision(objects[i]->GetAABB(), objects[j]->GetAABB()))
				{
					// For now, leave it empty, will resolve physics later
				}
			}
		}
	}
}
*/