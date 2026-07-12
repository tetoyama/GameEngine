#include "BoardLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ElemenTactics {

namespace {

constexpr float Sqrt3 = 1.7320508075688772f;

} // namespace

BoardLayout BoardLayout::Create(float viewportWidth, float viewportHeight){
	BoardLayout layout;
	viewportWidth = std::max(640.0f, viewportWidth);
	viewportHeight = std::max(360.0f, viewportHeight);

	const float margin = std::clamp(viewportWidth * 0.01875f, 12.0f, 28.0f);
	const float hudWidth = std::clamp(viewportWidth * 0.19f, 150.0f, 260.0f);
	const float footerHeight = std::clamp(viewportHeight * 0.16f, 72.0f, 116.0f);
	const float boardLeft = margin + hudWidth + margin;
	const float boardRight = viewportWidth - margin - hudWidth - margin;
	const float boardTop = margin;
	const float boardBottom = viewportHeight - footerHeight - margin;
	const float availableWidth = std::max(240.0f, boardRight - boardLeft);
	const float availableHeight = std::max(240.0f, boardBottom - boardTop);

	layout.m_hexRadius = std::clamp(
		std::min(availableWidth / (5.0f * Sqrt3), availableHeight / 8.0f),
		26.0f,
		74.0f);
	const ScreenPoint origin{
		(boardLeft + boardRight) * 0.5f,
		(boardTop + boardBottom) * 0.5f
	};

	float minimumX = std::numeric_limits<float>::max();
	float minimumY = std::numeric_limits<float>::max();
	float maximumX = std::numeric_limits<float>::lowest();
	float maximumY = std::numeric_limits<float>::lowest();
	for(const BoardCell& cell : BoardCells){
		const float q = static_cast<float>(cell.q);
		const float r = static_cast<float>(cell.r);
		const ScreenPoint center{
			origin.x + layout.m_hexRadius * Sqrt3 * (q + r * 0.5f),
			origin.y + layout.m_hexRadius * 1.5f * r
		};
		layout.m_centers[static_cast<std::size_t>(cell.id)] = center;
		minimumX = std::min(minimumX, center.x - layout.m_hexRadius * Sqrt3 * 0.5f);
		maximumX = std::max(maximumX, center.x + layout.m_hexRadius * Sqrt3 * 0.5f);
		minimumY = std::min(minimumY, center.y - layout.m_hexRadius);
		maximumY = std::max(maximumY, center.y + layout.m_hexRadius);
	}
	layout.m_boardBounds = ScreenRect{
		minimumX,
		minimumY,
		maximumX - minimumX,
		maximumY - minimumY
	};
	layout.m_leftHudBounds = ScreenRect{
		margin,
		margin,
		hudWidth,
		viewportHeight - footerHeight - margin * 2.0f
	};
	layout.m_rightHudBounds = ScreenRect{
		viewportWidth - margin - hudWidth,
		margin,
		hudWidth,
		viewportHeight - footerHeight - margin * 2.0f
	};
	layout.m_footerBounds = ScreenRect{
		margin,
		viewportHeight - footerHeight,
		viewportWidth - margin * 2.0f,
		footerHeight - margin
	};
	return layout;
}

ScreenPoint BoardLayout::CellCenter(int cellId) const noexcept{
	if(!IsBoardCell(cellId)) return {};
	return m_centers[static_cast<std::size_t>(cellId)];
}

std::optional<int> BoardLayout::HitTestCell(ScreenPoint point) const noexcept{
	std::optional<int> best;
	float bestDistanceSquared = std::numeric_limits<float>::max();
	for(const BoardCell& cell : BoardCells){
		if(!ContainsPoint(cell.id, point)) continue;
		const ScreenPoint center = CellCenter(cell.id);
		const float deltaX = point.x - center.x;
		const float deltaY = point.y - center.y;
		const float distanceSquared = deltaX * deltaX + deltaY * deltaY;
		if(distanceSquared < bestDistanceSquared){
			bestDistanceSquared = distanceSquared;
			best = cell.id;
		}
	}
	return best;
}

bool BoardLayout::ContainsPoint(int cellId, ScreenPoint point) const noexcept{
	if(!IsBoardCell(cellId)) return false;
	const ScreenPoint center = CellCenter(cellId);
	const float deltaX = std::abs(point.x - center.x);
	const float deltaY = std::abs(point.y - center.y);
	const float halfWidth = m_hexRadius * Sqrt3 * 0.5f;
	if(deltaX > halfWidth || deltaY > m_hexRadius) return false;
	return deltaX + Sqrt3 * deltaY <= Sqrt3 * m_hexRadius;
}

} // namespace ElemenTactics
