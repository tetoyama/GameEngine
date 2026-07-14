#pragma once

#include "Game/MiniGameCollection/Backshot/BackshotSlideMovement.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace MiniGameCollection::Backshot {

enum class RouteConnection : std::uint8_t {
    None = 0,
    North = 1u << 0,
    East = 1u << 1,
    South = 1u << 2,
    West = 1u << 3
};

constexpr RouteConnection operator|(
    RouteConnection lhs,
    RouteConnection rhs
) noexcept {
    return static_cast<RouteConnection>(
        static_cast<std::uint8_t>(lhs) |
        static_cast<std::uint8_t>(rhs)
    );
}

constexpr RouteConnection& operator|=(
    RouteConnection& lhs,
    RouteConnection rhs
) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool HasConnection(
    RouteConnection mask,
    RouteConnection connection
) noexcept {
    return (
        static_cast<std::uint8_t>(mask) &
        static_cast<std::uint8_t>(connection)
    ) != 0u;
}

enum class RouteCellType : std::uint8_t {
    Empty,
    StraightHorizontal,
    StraightVertical,
    CornerNE,
    CornerNW,
    CornerSE,
    CornerSW,
    TJunctionN,
    TJunctionE,
    TJunctionS,
    TJunctionW,
    Cross,
    DeadEndN,
    DeadEndE,
    DeadEndS,
    DeadEndW,
    Block
};

struct RouteCellDefinition {
    RouteCellType type = RouteCellType::Block;
    RouteConnection connections = RouteConnection::None;
};

struct BackshotRouteLayout {
    std::string name;
    int width = 1;
    int height = 1;
    float cellSize = 1.0f;
    std::vector<RouteCellDefinition> cells;
    std::array<SlideCell, 4> playerStarts{};
    std::uint32_t deterministicSeed = 0;

    bool IsInside(SlideCell cell) const noexcept {
        return cell.x >= 0 && cell.y >= 0 &&
            cell.x < width && cell.y < height;
    }

    std::size_t Index(SlideCell cell) const noexcept {
        return static_cast<std::size_t>(cell.y * width + cell.x);
    }

    const RouteCellDefinition* TryGet(SlideCell cell) const noexcept {
        if (!IsInside(cell)) {
            return nullptr;
        }
        const std::size_t index = Index(cell);
        return index < cells.size() ? &cells[index] : nullptr;
    }
};

struct RouteSlideMove {
    SlideCell start{};
    SlideCell stop{};
    SlideDirection initialDirection = SlideDirection::Up;
    SlideDirection finalDirection = SlideDirection::Up;
    std::vector<SlideCell> path;

    bool IsValid() const noexcept {
        return path.size() > 1 && start != stop;
    }

    int DistanceCells() const noexcept {
        return path.empty()
            ? 0
            : static_cast<int>(path.size()) - 1;
    }
};

class BackshotRouteLayoutBuilder final {
public:
    BackshotRouteLayoutBuilder(
        std::string name,
        int width,
        int height,
        float cellSize,
        std::uint32_t seed
    ) {
        m_layout.name = std::move(name);
        m_layout.width = (std::max)(1, width);
        m_layout.height = (std::max)(1, height);
        m_layout.cellSize = (std::max)(0.1f, cellSize);
        m_layout.deterministicSeed = seed;
        m_masks.assign(
            static_cast<std::size_t>(m_layout.width * m_layout.height),
            RouteConnection::None
        );
    }

    BackshotRouteLayoutBuilder& ConnectPath(
        const std::vector<SlideCell>& path
    ) {
        for (std::size_t index = 1; index < path.size(); ++index) {
            Connect(path[index - 1], path[index]);
        }
        return *this;
    }

    BackshotRouteLayoutBuilder& SetPlayerStarts(
        std::array<SlideCell, 4> starts
    ) noexcept {
        m_layout.playerStarts = starts;
        return *this;
    }

    BackshotRouteLayout Build() {
        m_layout.cells.resize(m_masks.size());
        for (std::size_t index = 0; index < m_masks.size(); ++index) {
            m_layout.cells[index] = {
                .type = TypeFromMask(m_masks[index]),
                .connections = m_masks[index]
            };
        }
        return std::move(m_layout);
    }

private:
    void Connect(SlideCell lhs, SlideCell rhs) {
        if (!m_layout.IsInside(lhs) || !m_layout.IsInside(rhs)) {
            return;
        }
        const int dx = rhs.x - lhs.x;
        const int dy = rhs.y - lhs.y;
        RouteConnection from = RouteConnection::None;
        RouteConnection to = RouteConnection::None;
        if (dx == 1 && dy == 0) {
            from = RouteConnection::East;
            to = RouteConnection::West;
        } else if (dx == -1 && dy == 0) {
            from = RouteConnection::West;
            to = RouteConnection::East;
        } else if (dx == 0 && dy == 1) {
            from = RouteConnection::North;
            to = RouteConnection::South;
        } else if (dx == 0 && dy == -1) {
            from = RouteConnection::South;
            to = RouteConnection::North;
        } else {
            return;
        }
        m_masks[m_layout.Index(lhs)] |= from;
        m_masks[m_layout.Index(rhs)] |= to;
    }

    static RouteCellType TypeFromMask(RouteConnection mask) noexcept {
        const std::uint8_t value = static_cast<std::uint8_t>(mask);
        switch (value) {
        case 0: return RouteCellType::Block;
        case 1: return RouteCellType::DeadEndN;
        case 2: return RouteCellType::DeadEndE;
        case 4: return RouteCellType::DeadEndS;
        case 8: return RouteCellType::DeadEndW;
        case 1 | 4: return RouteCellType::StraightVertical;
        case 2 | 8: return RouteCellType::StraightHorizontal;
        case 1 | 2: return RouteCellType::CornerNE;
        case 1 | 8: return RouteCellType::CornerNW;
        case 4 | 2: return RouteCellType::CornerSE;
        case 4 | 8: return RouteCellType::CornerSW;
        case 1 | 2 | 8: return RouteCellType::TJunctionN;
        case 1 | 2 | 4: return RouteCellType::TJunctionE;
        case 2 | 4 | 8: return RouteCellType::TJunctionS;
        case 1 | 4 | 8: return RouteCellType::TJunctionW;
        case 1 | 2 | 4 | 8: return RouteCellType::Cross;
        default: return RouteCellType::Empty;
        }
    }

    BackshotRouteLayout m_layout;
    std::vector<RouteConnection> m_masks;
};

class BackshotRouteLayouts final {
public:
    static BackshotRouteLayout LayoutA(float cellSize = 1.55f) {
        BackshotRouteLayoutBuilder builder(
            "CENTRAL CROSS",
            11,
            7,
            cellSize,
            0xA11CE001u
        );
        builder
            .ConnectPath(Horizontal(1, 9, 1))
            .ConnectPath(Horizontal(1, 9, 3))
            .ConnectPath(Horizontal(1, 9, 5))
            .ConnectPath(Vertical(1, 1, 5))
            .ConnectPath(Vertical(5, 1, 5))
            .ConnectPath(Vertical(9, 1, 5))
            .SetPlayerStarts({SlideCell{1, 1}, {9, 1}, {9, 5}, {1, 5}});
        return builder.Build();
    }

    static BackshotRouteLayout LayoutB(float cellSize = 1.55f) {
        BackshotRouteLayoutBuilder builder(
            "CORNER CIRCUIT",
            11,
            7,
            cellSize,
            0xB00B1E02u
        );
        builder
            .ConnectPath(Horizontal(2, 8, 1))
            .ConnectPath(Vertical(8, 1, 5))
            .ConnectPath(HorizontalReverse(8, 2, 5))
            .ConnectPath(VerticalReverse(2, 5, 1))
            .ConnectPath({{2, 3}, {3, 3}, {4, 3}, {4, 4}, {5, 4}, {6, 4}, {6, 3}, {7, 3}, {8, 3}})
            .SetPlayerStarts({SlideCell{2, 1}, {8, 1}, {8, 5}, {2, 5}});
        return builder.Build();
    }

    static BackshotRouteLayout LayoutC(float cellSize = 1.55f) {
        BackshotRouteLayoutBuilder builder(
            "T JUNCTIONS",
            11,
            7,
            cellSize,
            0xC0FFEE03u
        );
        builder
            .ConnectPath(Horizontal(1, 9, 3))
            .ConnectPath(Vertical(1, 1, 5))
            .ConnectPath(Vertical(3, 1, 5))
            .ConnectPath(Vertical(5, 1, 5))
            .ConnectPath(Vertical(7, 1, 5))
            .ConnectPath(Vertical(9, 1, 5))
            .ConnectPath({{1, 1}, {2, 1}, {3, 1}})
            .ConnectPath({{7, 5}, {8, 5}, {9, 5}})
            .SetPlayerStarts({SlideCell{1, 1}, {9, 1}, {9, 5}, {1, 5}});
        return builder.Build();
    }

    static BackshotRouteLayout ForRound(
        std::uint32_t roundIndex,
        float cellSize = 1.55f
    ) {
        switch (roundIndex % 3u) {
        case 0: return LayoutA(cellSize);
        case 1: return LayoutB(cellSize);
        default: return LayoutC(cellSize);
        }
    }

private:
    static std::vector<SlideCell> Horizontal(int minX, int maxX, int y) {
        std::vector<SlideCell> result;
        for (int x = minX; x <= maxX; ++x) {
            result.push_back({x, y});
        }
        return result;
    }

    static std::vector<SlideCell> HorizontalReverse(int maxX, int minX, int y) {
        std::vector<SlideCell> result;
        for (int x = maxX; x >= minX; --x) {
            result.push_back({x, y});
        }
        return result;
    }

    static std::vector<SlideCell> Vertical(int x, int minY, int maxY) {
        std::vector<SlideCell> result;
        for (int y = minY; y <= maxY; ++y) {
            result.push_back({x, y});
        }
        return result;
    }

    static std::vector<SlideCell> VerticalReverse(int x, int maxY, int minY) {
        std::vector<SlideCell> result;
        for (int y = maxY; y >= minY; --y) {
            result.push_back({x, y});
        }
        return result;
    }
};

class BackshotRouteBoard final {
public:
    explicit BackshotRouteBoard(BackshotRouteLayout layout)
        : m_layout(std::move(layout)) {
    }

    const BackshotRouteLayout& Layout() const noexcept { return m_layout; }
    int Width() const noexcept { return m_layout.width; }
    int Height() const noexcept { return m_layout.height; }
    float CellSize() const noexcept { return m_layout.cellSize; }

    bool IsInside(SlideCell cell) const noexcept {
        return m_layout.IsInside(cell);
    }

    bool IsRoute(SlideCell cell) const noexcept {
        const RouteCellDefinition* definition = m_layout.TryGet(cell);
        return definition && definition->connections != RouteConnection::None;
    }

    bool IsTemporarilyBlocked(SlideCell cell) const noexcept {
        return Contains(m_temporaryBlockedCells, cell);
    }

    bool IsBlocked(SlideCell cell) const noexcept {
        return !IsRoute(cell) || IsTemporarilyBlocked(cell);
    }

    void SetTemporaryBlockedCells(std::vector<SlideCell> cells) {
        std::erase_if(cells, [this](SlideCell cell) { return !IsRoute(cell); });
        std::sort(cells.begin(), cells.end(), CellLess);
        cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
        m_temporaryBlockedCells = std::move(cells);
    }

    const std::vector<SlideCell>& GetTemporaryBlockedCells() const noexcept {
        return m_temporaryBlockedCells;
    }

    RouteSlideMove ComputeMove(
        SlideCell start,
        SlideDirection direction,
        const std::vector<SlideCell>& reservedCells
    ) const {
        RouteSlideMove move{
            .start = start,
            .stop = start,
            .initialDirection = direction,
            .finalDirection = direction,
            .path = {start}
        };
        if (IsBlocked(start) || !CanExit(start, direction)) {
            return move;
        }

        SlideCell cursor = start;
        SlideDirection travel = direction;
        while (true) {
            const SlideCell step = BackshotSlideBoard::Step(travel);
            const SlideCell next{cursor.x + step.x, cursor.y + step.y};
            if (IsBlocked(next) || Contains(reservedCells, next) ||
                !CanEnter(next, travel)) {
                break;
            }

            cursor = next;
            move.path.push_back(cursor);
            move.stop = cursor;
            move.finalDirection = travel;

            const RouteCellDefinition* cell = m_layout.TryGet(cursor);
            if (!cell) {
                break;
            }
            const int connectionCount = CountConnections(cell->connections);
            if (connectionCount >= 3) {
                break;
            }

            const RouteConnection reverse = Opposite(ToConnection(travel));
            std::optional<SlideDirection> continuation;
            int continuationCount = 0;
            for (SlideDirection candidate : AllDirections()) {
                const RouteConnection connection = ToConnection(candidate);
                if (connection == reverse ||
                    !HasConnection(cell->connections, connection)) {
                    continue;
                }
                continuation = candidate;
                ++continuationCount;
            }
            if (continuationCount != 1 || !continuation) {
                break;
            }
            travel = *continuation;
        }
        return move;
    }

    Vec2 CellToWorld(SlideCell cell) const noexcept {
        return {
            (static_cast<float>(cell.x) -
                static_cast<float>(m_layout.width - 1) * 0.5f) *
                m_layout.cellSize,
            (static_cast<float>(cell.y) -
                static_cast<float>(m_layout.height - 1) * 0.5f) *
                m_layout.cellSize
        };
    }

    bool WouldRemainNavigable(
        SlideCell candidateBlock,
        const std::vector<SlideCell>& occupiedCells
    ) const {
        if (!IsRoute(candidateBlock) ||
            Contains(occupiedCells, candidateBlock)) {
            return false;
        }

        for (SlideCell occupied : occupiedCells) {
            if (!IsRoute(occupied)) {
                return false;
            }
            if (!HasAnyReachableNeighbor(occupied, candidateBlock)) {
                return false;
            }
        }
        return true;
    }

    std::vector<SlideCell> RouteCells() const {
        std::vector<SlideCell> result;
        for (int y = 0; y < Height(); ++y) {
            for (int x = 0; x < Width(); ++x) {
                const SlideCell cell{x, y};
                if (IsRoute(cell)) {
                    result.push_back(cell);
                }
            }
        }
        return result;
    }

    static constexpr std::array<SlideDirection, 4> AllDirections() noexcept {
        return {
            SlideDirection::Up,
            SlideDirection::Right,
            SlideDirection::Down,
            SlideDirection::Left
        };
    }

    static constexpr RouteConnection ToConnection(
        SlideDirection direction
    ) noexcept {
        switch (direction) {
        case SlideDirection::Up: return RouteConnection::North;
        case SlideDirection::Right: return RouteConnection::East;
        case SlideDirection::Down: return RouteConnection::South;
        case SlideDirection::Left: return RouteConnection::West;
        }
        return RouteConnection::None;
    }

    static constexpr RouteConnection Opposite(
        RouteConnection connection
    ) noexcept {
        switch (connection) {
        case RouteConnection::North: return RouteConnection::South;
        case RouteConnection::East: return RouteConnection::West;
        case RouteConnection::South: return RouteConnection::North;
        case RouteConnection::West: return RouteConnection::East;
        default: return RouteConnection::None;
        }
    }

private:
    static bool CellLess(SlideCell lhs, SlideCell rhs) noexcept {
        return lhs.y < rhs.y || (lhs.y == rhs.y && lhs.x < rhs.x);
    }

    static bool Contains(
        const std::vector<SlideCell>& cells,
        SlideCell target
    ) noexcept {
        return std::find(cells.begin(), cells.end(), target) != cells.end();
    }

    static int CountConnections(RouteConnection mask) noexcept {
        std::uint8_t value = static_cast<std::uint8_t>(mask);
        int count = 0;
        while (value != 0u) {
            count += value & 1u;
            value >>= 1u;
        }
        return count;
    }

    bool CanExit(SlideCell cell, SlideDirection direction) const noexcept {
        const RouteCellDefinition* definition = m_layout.TryGet(cell);
        return definition &&
            HasConnection(definition->connections, ToConnection(direction));
    }

    bool CanEnter(SlideCell cell, SlideDirection travel) const noexcept {
        const RouteCellDefinition* definition = m_layout.TryGet(cell);
        return definition && HasConnection(
            definition->connections,
            Opposite(ToConnection(travel))
        );
    }

    bool HasAnyReachableNeighbor(
        SlideCell start,
        SlideCell candidateBlock
    ) const {
        std::vector<bool> visited(
            static_cast<std::size_t>(Width() * Height()),
            false
        );
        std::queue<SlideCell> frontier;
        frontier.push(start);
        visited[m_layout.Index(start)] = true;
        std::size_t reached = 0;

        while (!frontier.empty()) {
            const SlideCell cell = frontier.front();
            frontier.pop();
            ++reached;
            const RouteCellDefinition* definition = m_layout.TryGet(cell);
            if (!definition) {
                continue;
            }

            for (SlideDirection direction : AllDirections()) {
                if (!HasConnection(
                        definition->connections,
                        ToConnection(direction))) {
                    continue;
                }
                const SlideCell step = BackshotSlideBoard::Step(direction);
                const SlideCell next{cell.x + step.x, cell.y + step.y};
                if (next == candidateBlock || IsBlocked(next) ||
                    !CanEnter(next, direction)) {
                    continue;
                }
                const std::size_t index = m_layout.Index(next);
                if (!visited[index]) {
                    visited[index] = true;
                    frontier.push(next);
                }
            }
        }
        return reached > 1;
    }

    BackshotRouteLayout m_layout;
    std::vector<SlideCell> m_temporaryBlockedCells;
};

} // namespace MiniGameCollection::Backshot
