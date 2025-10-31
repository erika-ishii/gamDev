#include "DecisionNode.h"

//Constructor
DecisionNode::DecisionNode
(
    std::function<bool(float)> condition,
    std::unique_ptr<DecisionNode> trueNode,
    std::unique_ptr<DecisionNode> falseNode,
    std::function<void(float)> leafAction
)
    : mainqns(std::move(condition)),
    ifTrue(std::move(trueNode)),
    ifFalse(std::move(falseNode)),
    action(std::move(leafAction))
{}


void DecisionNode::evaluate(float dt)
{
    if (mainqns) {
        if (mainqns(dt)) {
            if (ifTrue) ifTrue->evaluate(dt);
            else if (action) action(dt);
        }
        else {
            if (ifFalse) ifFalse->evaluate(dt);
            else if (action) action(dt);
        }
    }
    else if (action) {
        action(dt);
    }
}