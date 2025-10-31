#include "DecisionTree.h"
DecisionTree::DecisionTree(std::unique_ptr<DecisionNode> startNode):rootNode(std::move(startNode)){}
void DecisionTree::run(float dt){if (rootNode){rootNode->evaluate(dt);}}

