#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include "Common/ComponentTypeID.h"

namespace Framework
{
	class HurtBoxComponent : public GameComponent
	{
	public:
		float width = 5.0f;
		float height = 5.0f;
		float duration = 0.1f; // seconds (how long the hurt box lasts)
		bool active = false;
		float spawnX = 0.0f;
		float spawnY = 0.0f;

		void initialize() override { active = false; }
		void SendMessage(Message& m) override { (void)m; }

		void Serialize(ISerializer& s) override
		{
			if (s.HasKey("hurtwidth")) StreamRead(s, "hurtwidth", width);
			if (s.HasKey("hurtheight")) StreamRead(s, "hurtheight", height);
			if (s.HasKey("hurtduration")) StreamRead(s, "hurtduration", duration);
		}

		std::unique_ptr<GameComponent> Clone() const override
		{
			auto copy = std::make_unique<HurtBoxComponent>();
			copy->width = width;
			copy->height = height;
			copy->duration = duration;
			copy->active = active;
			return copy;
		}

		void ActivateHurtBox() { active = true; }
		void DeactivateHurtBox() { active = false; }
	};
}