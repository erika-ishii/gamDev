#pragma once
#include <memory>
#include <functional>
class DecisionNode
{
    public:
     std::function<bool()> mainqns;
     std::unique_ptr<DecisionNode> ifTrue;
     std::unique_ptr<DecisionNode> ifFalse;
     std::function<void()> action;
     
     DecisionNode(std::function<bool()> qns,
     std::unique_ptr<DecisionNode> trueNode = nullptr,
     std::unique_ptr<DecisionNode> falseNode = nullptr,
     std::function<void()> leafAction = nullptr);

     // Delete copy constructor and copy assignment
     DecisionNode(const DecisionNode&) = delete;
     DecisionNode& operator=(const DecisionNode&) = delete;

     // Explicitly default move constructor and move assignment
     DecisionNode(DecisionNode&&) = default;
     DecisionNode& operator=(DecisionNode&&) = default;

     void evaluate();

};

