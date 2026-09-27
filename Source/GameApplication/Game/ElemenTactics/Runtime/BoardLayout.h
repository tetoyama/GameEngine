#pragma once

#include "../Core/ElemenTacticsCore.h"

#include <array>
#include <optional>

namespace ElemenTactics {

struct ScreenPoint {
	float x = 0.0f;
	float y = 0.0f;
};

struct ScreenRect {
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;

	bool Contains(ScreenPoint point) const noexcept{
		return point.x >= x && point.x <= x + width &&
			point.y >= y && point.y <= y + height;
	}
};

class BoardLayout final {
public:
	static BoardLayout Create(float viewportWidth, float viewportHeight);

	ScreenPoint CellCenter(int cellId) const noexcept;
	std::optional<int> HitTestCell(ScreenPoint point) const noexcept;
	bool ContainsPoint(int cellId, ScreenPoint point) const noexcept;

	float HexRadius() const noexcept{ return m_hexRadius; }
	ScreenRect BoardBounds() const noexcept{ return m_boardBounds; }
	ScreenRect LeftHudBounds() const noexcept{ return m_leftHudBounds; }
	ScreenRect RightHudBounds() const noexcept{ return m_rightHudBounds; }
	ScreenRect FooterBounds() const noexcept{ return m_footerBounds; }

private:
	float m_hexRadius = 48.0f;
	std::array<ScreenPoint, BoardCellCount> m_centers{};
	ScreenRect m_boardBounds{};
	ScreenRect m_leftHudBounds{};
	ScreenRect m_rightHudBounds{};
	ScreenRect m_footerBounds{};
};

} // namespace ElemenTactics
