/*********************************************************************************************
 \file      UniformGrid.cpp
 \par       SofaSpuds
 \author    Ho Jun (h.jun@digipen.edu) - Primary Author, 100%
 \brief		Implements a uniform spatial grid used for broad-phase collision queries.
 \details	Defines the runtime behavior of the UniformGrid helper, including cell
			insertion, spatial queries, and grid clearing between simulation steps.
			Objects are mapped to all grid cells overlapped by their AABBs, allowing
			efficient retrieval of nearby candidates for narrow-phase collision tests.

			The grid operates in world space using a fixed cell size and is intended
			to be rebuilt each physics update. All queries return object identifiers
			only; ownership and lifetime of objects remain the responsibility of the
			calling system (typically PhysicSystem).
 \copyright
			All content ©2025 DigiPen Institute of Technology Singapore.
			All rights reserved.
*********************************************************************************************/

#include "Systems/UniformGrid.h"
#include <cmath>

namespace
{
	inline int WorldToCell(float value, float cellSize)
	{
		return static_cast<int>(std::floor(value / cellSize));
	}
}

void UniformGrid::Clear()
{
	m_cells.clear();
}

void UniformGrid::Insert(GOCId id, const Framework::AABB& box)
{
	int minX = WorldToCell(box.min.getX(), m_cellSize);
	int minY = WorldToCell(box.min.getY(), m_cellSize);
	int maxX = WorldToCell(box.max.getX(), m_cellSize);
	int maxY = WorldToCell(box.max.getY(), m_cellSize);

	for (int x = minX; x <= maxX; x++)
	{
		for (int y = minY; y <= maxY; y++)
		{
			CellCoord coord{ x,y };
			m_cells[coord].push_back(id);
		}
	}
}

void UniformGrid::Query(const Framework::AABB& box, std::vector<GOCId>& out) const
{
	out.clear();

	int minX = WorldToCell(box.min.getX(), m_cellSize);
	int minY = WorldToCell(box.min.getY(), m_cellSize);
	int maxX = WorldToCell(box.max.getX(), m_cellSize);
	int maxY = WorldToCell(box.max.getY(), m_cellSize);

	for (int x = minX; x <= maxX; x++)
	{
		for (int y = minY; y <= maxY; y++)
		{
			CellCoord coord{ x,y };
			auto it = m_cells.find(coord);
			if (it == m_cells.end())
				continue;

			for (GOCId id : it->second)
			{
				out.push_back(id);
			}
		}
	}
}