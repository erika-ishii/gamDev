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