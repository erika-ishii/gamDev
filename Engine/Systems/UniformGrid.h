/*********************************************************************************************
 \file      UniformGrid.h
 \par       SofaSpuds
 \author    Ho Jun (h.jun@digipen.edu) - Primary Author, 100%
 \brief		Provides a uniform spatial partitioning structure for broad-phase collision
			detection in 2D space.
 \details	Divides the world into fixed-size square cells and maps game object IDs to
			the cells overlapped by their axis-aligned bounding boxes (AABBs). Used as
			a broad-phase accelerator to reduce the number of narrow-phase collision
			checks by querying only nearby objects instead of the full scene.

			The grid is typically rebuilt each physics update (Clear → Insert) and
			queried using an AABB that represents either an object's current bounds
			or its swept volume for continuous collision detection. Designed to be
			owned and managed by the PhysicSystem as an internal helper, without
			direct knowledge of game objects or components.
 \copyright
			All content ©2025 DigiPen Institute of Technology Singapore.
			All rights reserved.
*********************************************************************************************/

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