/*********************************************************************************************
 \file      DecisionTree.cpp
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Implementation of the DecisionTree class, which manages and executes a hierarchy
            of DecisionNode objects representing branching decision logic.

 \details
            The DecisionTree class acts as the entry point for evaluating a decision tree.
            It contains a single root node and provides the run() function, which begins
            recursive evaluation through all connected nodes to determine and perform
            appropriate actions based on defined conditions.

 \copyright
            All content 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#include "DecisionTree.h"
/*********************************************************************************************
 \brief
    Constructs a DecisionTree with the specified root DecisionNode.

 \param startNode
    Unique pointer to the root DecisionNode that serves as the starting point for evaluation.
*********************************************************************************************/
DecisionTree::DecisionTree(std::unique_ptr<DecisionNode> startNode):rootNode(std::move(startNode)){}
/*********************************************************************************************
 \brief
    Executes the decision tree starting from the root node.

 \param dt
    Floating-point input parameter, typically representing delta time or a contextual value
    used by the DecisionNode evaluation logic.

 \details
    If the root node exists, the evaluation process begins by calling its evaluate() method.
*********************************************************************************************/
void DecisionTree::run(float dt){if (rootNode){rootNode->evaluate(dt);}}

