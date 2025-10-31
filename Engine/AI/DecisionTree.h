#pragma once
#include "../AI/DecisionNode.h"
class DecisionTree
{
    public:
    DecisionTree(std::unique_ptr<DecisionNode> startNode);
  
    // Delete copy constructor and copy assignment
    DecisionTree(const DecisionTree&) = delete;
    DecisionTree& operator=(const DecisionTree&) = delete;

    // Default move constructor and move assignment
    DecisionTree(DecisionTree&&) = default;
    DecisionTree& operator=(DecisionTree&&) = default;

    // Destructor
    ~DecisionTree() = default;
    void run(float dt);
    private:
    std::unique_ptr<DecisionNode> rootNode; 
    enum ID{Patrol, Attack, Flee };
};