/*********************************************************************************************
 \file      EnemyDecisionTreeComponent.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Declares the EnemyDecisionTreeComponent class, which attaches an AI decision
            tree to an enemy game object. This component governs high-level enemy
            behavior such as idle, chase, or patrol states using decision-tree logic.

 \details
            EnemyDecisionTreeComponent integrates the AI/DecisionTree system into the
            engine's ECS architecture. When initialized, it automatically constructs a
            default decision tree for the owning GameObjectComposition via
            CreateDefaultEnemyTree(). The component stores additional runtime data such
            as movement direction, chase timers, and flags indicating whether the player
            has been seen.

            Responsibilities:
            - Owns and updates an AI DecisionTree instance.
            - Tracks state data like chase direction, pause timers, and player detection.
            - Provides a framework for extensible enemy AI logic.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include "AI/DecisionTreeDefault.h"
#include "Composition/Composition.h"
#include "AI/DecisionTree.h"
#include "Systems/LogicSystem.h"
#include <iostream>

#define NOMINMAX
#if defined(APIENTRY)
#  undef APIENTRY
#endif
#include <Windows.h>
#ifdef SendMessage
#  undef SendMessage
#endif

namespace Framework {
    enum class Facing { LEFT, RIGHT };
    /*****************************************************************************************
      \brief Creates a default decision tree for an enemy.
      \param enemy  Pointer to the enemy�s GameObjectComposition.
      \return A unique_ptr to a new DecisionTree instance configured for default AI behavior.
    *****************************************************************************************/
    std::unique_ptr<DecisionTree> CreateDefaultEnemyTree(GOC* enemy, LogicSystem* logic);

    /*****************************************************************************************
      \class EnemyDecisionTreeComponent
      \brief Component responsible for managing the AI decision tree of an enemy.

      This component attaches a DecisionTree to an enemy entity, initializing it with
      default behaviors defined in AI/DecisionTreeDefault.h. It tracks state variables
      such as movement direction, chase duration, and player detection flags.
    *****************************************************************************************/
    class EnemyDecisionTreeComponent : public GameComponent
    {
    public:
        std::unique_ptr<DecisionTree> tree;  ///< The decision tree controlling enemy behavior.
        float dir = 1.0f;                    ///< Movement direction (1.0 for right, -1.0 for left).
        float pauseTimer = 0.0f;             ///< Timer used for brief pauses between AI actions.
        float chaseSpeed = 0.0f;             ///< Current speed while chasing the player.
        float chaseTimer = 0.0f;             ///< Accumulated time spent in chase mode.
        float maxChaseDuration = 3.0f;       ///< Maximum allowed chase time before reset.
        bool hasSeenPlayer = false;          ///< Tracks whether the enemy has detected the player.
        Facing facing = Facing::RIGHT;

        EnemyDecisionTreeComponent() = default;

        /*************************************************************************************
          \brief Initializes the decision tree for this enemy component.
          \details
              - Retrieves the owning GameObjectComposition.
              - Constructs a default DecisionTree using CreateDefaultEnemyTree().
              - Logs a debug message upon successful initialization.
        *************************************************************************************/
        void initialize() override
        {std::cout << "[EnemyDecisionTreeComponent] Tree initialized.\n";   }

        /*************************************************************************************
          \brief Handles incoming messages sent to this component.
          \param m  Reference to the incoming message.
          \note  Currently unused but kept for future AI message handling.
        *************************************************************************************/
        void SendMessage(Message& m) override { (void)m; }

        /*************************************************************************************
          \brief Serializes the component data.
          \param s  Reference to the serializer.
          \note  Currently a placeholder; no serializable data yet.
        *************************************************************************************/
        void Serialize(ISerializer& s) override { (void)s; }

        /*************************************************************************************
          \brief Creates a deep copy of this component.
          \return A unique_ptr holding a cloned EnemyDecisionTreeComponent.
        *************************************************************************************/
        std::unique_ptr<GameComponent> Clone() const override
        {
            auto copy = std::make_unique<EnemyDecisionTreeComponent>();
            copy->dir = dir;
            copy->pauseTimer = pauseTimer;
            copy->chaseSpeed = chaseSpeed;
            copy->chaseTimer = chaseTimer;
            copy->maxChaseDuration = maxChaseDuration;
            copy->hasSeenPlayer = hasSeenPlayer;

            return copy;
        }
    };
}
