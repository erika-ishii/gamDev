#pragma once
#include <unordered_map>
#include <vector>
#include "Physics/Collision/Collision.h" 

using GOCId = unsigned int;

struct CellCoord
{
	int x;
	int y;

	bool operator==(const CellCoord& other) const
	{
		return x == other.x && other.y;
	}
};

struct CellCoordHash
{
	size_t operator()(const CellCoord& c) const
	{
		return std::hash<int>()(c.x) ^ (std::hash<int>()(c.y) << 1);
	}
};

class UniformGrid
{
public:
	void Clear();
	void Insert(GOCId id, const Framework::AABB& box); 
	void Query(const Framework::AABB& box, std::vector<GOCId>& out) const;

private:
	float m_cellSize = 64.0f;
	std::unordered_map<CellCoord, std::vector<GOCId>, CellCoordHash> m_cells;
};