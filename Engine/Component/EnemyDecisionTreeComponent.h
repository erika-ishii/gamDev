#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include "AI/DecisionTreeDefault.h"
#include "Composition/Composition.h"
#include "AI/DecisionTree.h"
#include<iostream>
#define NOMINMAX
#include <Windows.h>
#ifdef SendMessage
#undef SendMessage
#endif
namespace Framework {

	std::unique_ptr<DecisionTree> CreateDefaultEnemyTree(GOC* enemy);
	class EnemyDecisionTreeComponent : public GameComponent
	{
	public:
		std::unique_ptr<DecisionTree> tree;
        float dir = 1.0f;
        float pauseTimer = 0.0f;
		float chaseSpeed = 0.0f;
		float chaseTimer = 0.0f;
		float maxChaseDuration = 3.0f;
		bool hasSeenPlayer = false;
		void initialize() override
		{
			GOC* ownerGOC = GetOwner();
			if (ownerGOC) {
				tree = Framework::CreateDefaultEnemyTree(ownerGOC);
				std::cout << "[EnemyDecisionTreeComponent] Tree initialized.\n";
			}
		}
		void SendMessage(Message& m) override { (void)m; }
		void Serialize(ISerializer& s) override { (void)s; }
		std::unique_ptr<GameComponent> Clone() const override
		{
			auto copy = std::make_unique<EnemyDecisionTreeComponent>();
			return copy;
		}
	};
}
