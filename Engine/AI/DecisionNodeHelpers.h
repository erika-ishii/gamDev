#pragma once
#include <string>
#include "DecisionNodeHelpers.h"
#include "Composition/Composition.h"
#include "Factory/Factory.h"
#include "Common/ComponentTypeID.h"
#include "Component/SpriteAnimationComponent.h"
#include "Component/TransformComponent.h"
#include "Component/EnemyDecisionTreeComponent.h"
#include "Component/EnemyAttackComponent.h"
#include "Component/AudioComponent.h"
#include "Component/PlayerComponent.h"
#include "Physics/Dynamics/RigidBodyComponent.h"
#include "Systems/LogicSystem.h"
#include <cctype>
#include <cmath>
#include <string_view>

namespace Framework
{

	void PlayAnimationIfAvailable(GOC* goc, std::string_view name, bool forceRestart = false);
	float GetAnimationDuration(GOC* goc, const std::string& name);
	inline bool IsPlayerNear(GOC* enemy, float radius);
	void Patrol(void* enemy, float dt);
	void Idle(void* enemy, float dt);
	//static void MeleeAttack(GOC* enemy, float dt, LogicSystem* logic);
	//static void RangedAttack(GOC* enemy, float dt, LogicSystem* logic);

}