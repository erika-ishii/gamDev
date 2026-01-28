/*********************************************************************************************
 \file      RigidBodyComponent.h
 \par       SofaSpuds
 \author    Ho Jun (h.jun@digipen.edu) - Primary Author, 100%

 \brief     Implementation of the Rigid Body component. Provides AABB style collision checks for
			Rectangle to Circle and Rectangle to Rectangle, along with having structs
			for both Rectangle and Circle

 \copyright
			All content © 2025 DigiPen Institute of Technology Singapore.
			All rights reserved.
*********************************************************************************************/

#pragma once
#include "Composition/Component.h"
#include "Memory/ComponentPool.h"
#include "Common/ComponentTypeID.h"
#include "Serialization/Serialization.h"

namespace Framework
{

	class RigidBodyComponent : public GameComponent
	{
	public:
		float velX = 1.0f;
		float velY = 1.0f;
		float knockVelX = 0.0f;
		float knockVelY = 0.0f;
		float width = 1.0f;
		float height = 1.0f;
		float knockbackTime = 0.0f;
		float dampening = 0.7f;
		float lungeTime = 0.0f;

		void initialize() override {}
		void SendMessage(Message& m) override { (void)m; }

		/*****************************************************************************************
		\brief Serializes the values with a .json file
		\param s The .json file the information is on
		*****************************************************************************************/
		void Serialize(ISerializer& s) override
		{
			if (s.HasKey("velocity_x")) StreamRead(s, "velocity_x", velX);
			if (s.HasKey("velocity_y")) StreamRead(s, "velocity_y", velY);
			if (s.HasKey("width")) StreamRead(s, "width", width);
			if (s.HasKey("height")) StreamRead(s, "height", height);
		}

		/*****************************************************************************************
		\brief Copies, or clones, the value from the .json file to a copy that will be returned
		\return A copy of with all the values from the .json file
		*****************************************************************************************/
		ComponentHandle Clone() const override 
		{
			// Create new CircleRenderComponent on heap
			// Wrap inside unique_ptr so it is automatically clean up if something goes wrong
			auto copy = ComponentPool<RigidBodyComponent>::CreateTyped();
			//copy the values 
			copy->velX = velX;
			copy->velY = velY;
			copy->width = width;
			copy->height = height;
			//Transfer ownership to whoever call clone()
			return copy;

		}
	};
}