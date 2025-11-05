#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include "Common/ComponentTypeID.h"
#include "Common/System.h"
#include "Factory/Factory.h"
namespace Framework
{
	class HitBoxComponent : public GameComponent
	{
	public:
		float width = 5.0f;
		float height = 5.0f;
		float duration = 0.1f; // seconds (how long the hurt box lasts)
		bool active = false;
		float spawnX = 0.0f;
		float spawnY = 0.0f;
		GOC* owner = nullptr;
		float damage = 1.0f;
		// How long (in seconds) we should keep rendering the hurtbox for debug after a hit
		float debugDrawTimer = 0.0f;


		void initialize() override { active = false; }
		void SendMessage(Message& m) override { (void)m; }

		void Serialize(ISerializer& s) override
		{
			if (s.HasKey("width")) StreamRead(s, "width", width);
			if (s.HasKey("height")) StreamRead(s, "height", height);
			if (s.HasKey("duration")) StreamRead(s, "duration", duration);
		}

		std::unique_ptr<GameComponent> Clone() const override
		{
			auto copy = std::make_unique<HitBoxComponent>();
			copy->width = width;
			copy->height = height;
			copy->duration = duration;
			copy->active = active;
			copy->debugDrawTimer = debugDrawTimer;
			return copy;
		}

		void ActivateHurtBox() { active = true; }
		void DeactivateHurtBox() { active = false; }
	};
}