#pragma once
#include "../AI/DecisionNode.h"
class DecisionTree
{
    public:
    DecisionTree(std::unique_ptr<DecisionNode> startNode);
    void run();
    private:
    std::unique_ptr<DecisionNode> rootNode; 
    enum ID{Patrol, Attack, Flee };
};