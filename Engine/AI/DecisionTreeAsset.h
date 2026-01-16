#pragma once
#include <string>
#include <vector>
enum class DTNodeType
{ Condition,Action};

enum class DTConditionType
{None, PlayerNear,AlwaysTrue};

enum class DTActionType
{None, Patrol, Melee, Ranged, Idle};

struct DecisionTreeNodeData
{
    int NodeId = -1;
    DTConditionType conditiontype = DTConditionType::None;
    float ConditionValue = 0.0f;
    DTActionType action = DTActionType::None;
    int trueChild = -1;
    int falseChild = -1;
};

struct DecisionTreeAsset
{
    std::string treeId;
    int rootNodeId = -1;
    std::vector<DecisionTreeNodeData>nodes;
};
