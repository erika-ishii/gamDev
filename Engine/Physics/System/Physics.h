/*********************************************************************************************
 \file      Physics.h
 \par       SofaSpuds
 \author    Ho Jun (h.jun@digipen.edu) - Primary Author, 100%

 \brief     Implementation of physics in the game. 

 \copyright
			All content © 2025 DigiPen Institute of Technology Singapore.
			All rights reserved.
*********************************************************************************************/
#pragma once

#include <vector>
#include "Math/Vector_2D.h"
#include "Physics/Collision/Collision.h"
#include "Composition/Component.h"
#include "Common/System.h"
#include "Factory/Factory.h"
#include "Component/TransformComponent.h"
#include "Physics/Dynamics/RigidBodyComponent.h"



namespace Framework
{
	/*****************************************************************************************
	\brief Loops through all game objects managed by FACTORY and updates their position if needed
	*****************************************************************************************/
	class PhysicsSystem : public ISystem
	{
	public:
		void Update(float dt) override
		{
			dt;
			for (auto& [id, objPtr] : FACTORY->Objects())
			{
				auto* obj = objPtr.get();
				if (!obj)
					continue;
				// This hold velocity and size
				auto* rb = obj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
				// This holds position of the object
				auto* tr = obj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);

				if (!rb || !tr)
					continue;
			}
		}
		// This is just to return the name so the engine knows what system this is
		std::string GetName() override { return "PhysicsSystem"; }
	};
}