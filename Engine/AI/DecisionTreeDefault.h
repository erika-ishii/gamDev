#pragma once
#include "Composition/Composition.h"
#include "Factory/Factory.h"
#include "Common/ComponentTypeID.h"
#include "DecisionTree.h"
#include "DecisionNode.h"
#include "Component/EnemyDecisionTreeComponent.h"
#include "Component/EnemyHealthComponent.h"
#include "Component/EnemyAttackComponent.h"
#include "Component/TransformComponent.h"
#include "Component/PlayerComponent.h"
//#include "Physics/Dynamics/RigidBodyComponent.h"
//#include "Physics/System/Physics.h"
#include <cstdio>
namespace Framework 
{
 bool IsPlayerNear(GOC* enemy, float radius = 0.1f); 
 std::unique_ptr<DecisionTree> CreateDefaultEnemyTree(GOC* enemy);
}