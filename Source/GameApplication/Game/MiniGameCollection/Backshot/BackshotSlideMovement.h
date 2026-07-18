#pragma once

#include "Game/MiniGameCollection/Core/MiniGameMath.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace MiniGameCollection::Backshot {

struct SlideCell {
    int x = 0;
    int y = 0;

    friend constexpr bool operator==(SlideCell lhs, SlideCell rhs) noexcept {
        return lhs.x == rhs.x && lhs.y == rhs.y;
    }
};

enum class SlideDirection : unsigned char {
    Up,
    Down,
    Left,
    Right
};

struct SlideMove {
    SlideCell start{};
    SlideCell stop{};
    SlideDirection direction = SlideDirection::Up;
    int distanceCells = 0;

    bool IsValid() const noexcept {
        return distanceCells > 0 && start != stop;
    }
};

class BackshotSlideBoard final {
public:
    BackshotSlideBoard(
        int width,
        int height,
        float cellSize,
        std::vector<SlideCell> blockedCells
    )
        : m_width(std::max(1, width)),
          m_height(std::max(1, height)),
          m_cellSize(std::max(0.1f, cellSize)),
          m_blockedCells(std::move(blockedCells)) {
    }

    int Width() const noexcept { return m_width; }
    int Height() const noexcept { return m_height; }
    float CellSize() const noexcept { return m_cellSize; }

    bool IsInside(SlideCell cell) const noexcept {
        return cell.x >= 0 && cell.y >= 0 &&
            cell.x < m_width && cell.y < m_height;
    }

    bool IsBlocked(SlideCell cell) const noexcept {
        return std::find(
            m_blockedCells.begin(),
            m_blockedCells.end(),
            cell
        ) != m_blockedCells.end();
    }

    bool IsReserved(
        SlideCell cell,
        const std::vector<SlideCell>& reservedCells
    ) const noexcept {
        return std::find(
            reservedCells.begin(),
            reservedCells.end(),
            cell
        ) != reservedCells.end();
    }

    bool IsFree(
        SlideCell cell,
        const std::vector<SlideCell>& reservedCells
    ) const noexcept {
        return IsInside(cell) &&
            !IsBlocked(cell) &&
            !IsReserved(cell, reservedCells);
    }

    SlideMove ComputeMove(
        SlideCell start,
        SlideDirection direction,
        const std::vector<SlideCell>& reservedCells
    ) const noexcept {
        SlideMove result{
            .start = start,
            .stop = start,
            .direction = direction,
            .distanceCells = 0
        };
        if (!IsInside(start) || IsBlocked(start)) {
            return result;
        }

        const SlideCell step = Step(direction);
        SlideCell cursor = start;
        while (true) {
            const SlideCell next{cursor.x + step.x, cursor.y + step.y};
            if (!IsFree(next, reservedCells)) {
                break;
            }
            cursor = next;
            ++result.distanceCells;
        }
        result.stop = cursor;
        return result;
    }

    std::vector<SlideCell> TraceMove(const SlideMove& move) const {
        std::vector<SlideCell> cells;
        if (!move.IsValid()) {
            if (IsInside(move.start)) {
                cells.push_back(move.start);
            }
            return cells;
        }

        const SlideCell step = Step(move.direction);
        SlideCell cursor = move.start;
        cells.push_back(cursor);
        for (int index = 0; index < move.distanceCells; ++index) {
            cursor = {cursor.x + step.x, cursor.y + step.y};
            cells.push_back(cursor);
        }
        return cells;
    }

    Vec2 CellToWorld(SlideCell cell) const noexcept {
        return {
            (static_cast<float>(cell.x) -
                static_cast<float>(m_width - 1) * 0.5f) * m_cellSize,
            (static_cast<float>(cell.y) -
                static_cast<float>(m_height - 1) * 0.5f) * m_cellSize
        };
    }

    static constexpr SlideCell Step(SlideDirection direction) noexcept {
        switch (direction) {
        case SlideDirection::Up: return {0, 1};
        case SlideDirection::Down: return {0, -1};
        case SlideDirection::Left: return {-1, 0};
        case SlideDirection::Right: return {1, 0};
        }
        return {};
    }

    static constexpr Vec2 Forward(SlideDirection direction) noexcept {
        const SlideCell step = Step(direction);
        return {
            static_cast<float>(step.x),
            static_cast<float>(step.y)
        };
    }

private:
    int m_width = 1;
    int m_height = 1;
    float m_cellSize = 1.0f;
    std::vector<SlideCell> m_blockedCells;
};

} // namespace MiniGameCollection::Backshot
