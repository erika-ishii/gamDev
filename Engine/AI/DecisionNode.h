/*********************************************************************************************
 \file      DecisionNode.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Declaration of the DecisionNode class, which represents a single node in a
            decision tree structure. Each node contains:
            - A conditional function to evaluate a decision based on input data.
            - Two child nodes representing the "true" and "false" branches of the decision.
            - An optional action function to perform when the node is evaluated.

            The DecisionNode class allows branching logic to be built dynamically using
            function objects, enabling flexible AI or behavior tree decision-making.

 \details
            Usage example:
            A DecisionNode can test conditions (e.g., "Is enemy close?") and either execute
            an action (e.g., "Attack") or evaluate further sub-decisions recursively.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include <memory>
#include <functional>
/*********************************************************************************************
 \class DecisionNode
 \brief
    Represents a single node in a decision tree used for AI or behavior logic.

 \details
    Each DecisionNode contains:
      - A conditional function (`mainqns`) that evaluates a boolean decision based on input.
      - Two child nodes (`ifTrue` and `ifFalse`) representing the next branch of logic.
      - An optional action (`action`) to perform when the node is evaluated.

    DecisionNodes can be combined to form complex decision trees where each node
    dynamically chooses the next branch or executes an action. This allows flexible,
    runtime-configurable AI or game logic without hardcoding branching behavior.

 \note
    - The class supports move semantics but deletes copy semantics to ensure unique
      ownership of child nodes.
    - The `evaluate` function should be called with a relevant context (e.g., delta time)
      to execute the node's logic recursively.
*********************************************************************************************/
class DecisionNode
{
    public:
     std::function<bool(float)> mainqns;
     std::unique_ptr<DecisionNode> ifTrue;
     std::unique_ptr<DecisionNode> ifFalse;
     std::function<void(float)> action;
     
     DecisionNode(
         std::function<bool(float)> condition,
         std::unique_ptr<DecisionNode> trueNode,
         std::unique_ptr<DecisionNode> falseNode,
         std::function<void(float)> action
     );

     // Delete copy constructor and copy assignment
     DecisionNode(const DecisionNode&) = delete;
     DecisionNode& operator=(const DecisionNode&) = delete;

     // Explicitly default move constructor and move assignment
     DecisionNode(DecisionNode&&) = default;
     DecisionNode& operator=(DecisionNode&&) = default;

     void evaluate(float dt);

};

