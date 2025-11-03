/*********************************************************************************************
 \file      DecisionTree.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Declaration of the DecisionTree class, which manages and executes a decision
            tree composed of interconnected DecisionNode objects.

 \details
            The DecisionTree class serves as the controller for a hierarchy of DecisionNode
            instances. It holds the root node and provides an interface to evaluate and
            execute the decision logic through the run() function. This structure allows
            AI entities to make branching decisions such as patrol, attack, or flee
            behaviors dynamically at runtime.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "../AI/DecisionNode.h"
/*********************************************************************************************
 \class DecisionTree
 \brief
    Represents a complete decision tree for AI or behavior logic, composed of DecisionNode objects.

 \details
    The DecisionTree maintains a single root node (`rootNode`) and provides a method (`run`)
    to evaluate the tree. When `run` is called, the tree starts evaluation from the root
    node and traverses through child nodes based on each node’s conditional logic, executing
    actions as defined.

    This structure allows AI behaviors to be defined modularly and executed dynamically
    at runtime. The root node is supplied at construction, and the tree can contain
    any number of DecisionNode objects arranged in a branching hierarchy.

 \note
    - The class supports move semantics but deletes copy semantics to enforce unique
      ownership of the nodes.
    - Use `run(float dt)` each frame or AI update cycle to evaluate the tree.
*********************************************************************************************/
class DecisionTree
{
    public:
    DecisionTree(std::unique_ptr<DecisionNode> startNode);
    void run(float dt);
    private:
    std::unique_ptr<DecisionNode> rootNode; 
   
};