#pragma once
#include <memory>
#include <functional>
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

