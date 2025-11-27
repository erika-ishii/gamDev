/*********************************************************************************************
 \file      DecisionTreeDefault.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Declaration of default decision tree logic for enemy AI behavior. This module
            defines helper functions to create and update a standard enemy decision tree
            using basic conditions such as player proximity and health state.

 \details
            The default decision tree provides a simple AI structure for enemies:
            - Detects player proximity for initiating attacks.
            - Evaluates health to determine when to flee.
            - Falls back to patrol behavior when no immediate action is required.

            These utilities can be reused across multiple enemy types that require
            basic reactive decision-making.

 \copyright
            All content 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "Composition/Composition.h"
#include "Factory/Factory.h"
#include "Common/ComponentTypeID.h"
#include "DecisionTree.h"
#include "DecisionNode.h"
#include "Component/EnemyDecisionTreeComponent.h"
#include "Component/EnemyHealthComponent.h"
#include "Component/EnemyAttackComponent.h"
#include "Component/AudioComponent.h"
#include "Component/TransformComponent.h"
#include "Component/PlayerComponent.h"
#include "Physics/Dynamics/RigidBodyComponent.h"
#include "Physics/System/Physics.h"
#include "Systems/LogicSystem.h"
#include <cstdio>
#include <random>
namespace Framework 
{
 class LogicSystem;
 bool IsPlayerNear(GOC* enemy, float radius = 0.1f); 
 std::unique_ptr<DecisionTree> CreateDefaultEnemyTree(GOC* enemy, LogicSystem* logic);
 void UpdateDefaultEnemyTree(GOC* enemy, float dt, LogicSystem* logic);
}