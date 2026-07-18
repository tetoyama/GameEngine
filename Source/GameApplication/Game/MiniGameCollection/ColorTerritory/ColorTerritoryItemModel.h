#pragma once

#include "Game/MiniGameCollection/ColorTerritory/ColorTerritoryModel.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <utility>

namespace MiniGameCollection::ColorTerritory {

enum class TerritoryItemType : std::uint8_t {
    Bomb,
    Star
};

enum class TerritoryItemPhase : std::uint8_t {
    Inactive,
    Falling,
    Active
};

struct TerritoryItemConfig {
    float rushStartRemainingSeconds = 20.0f;
    float minimumSpawnIntervalSeconds = 4.0f;
    float maximumSpawnIntervalSeconds = 6.0f;
    // Bombは2.8秒、Starは最低2.4秒の予告要件を満たす共通猶予。
    float fallingSeconds = 2.8f;
    float bombFuseSeconds = 3.4f;
    float bombClaimGraceSeconds = 1.2f;
    int bombTileRadius = 1;
    float bombStunRadius = 2.25f;
    float bombStunSeconds = 1.25f;
    float starGroundLifetimeSeconds = 5.0f;
    float starBuffSeconds = 6.0f;
    float starSpeedMultiplier = 1.55f;
    float starTouchStunSeconds = 0.45f;
    float starTouchCooldownSeconds = 0.85f;
};

struct TerritoryPlayerPowerState {
    float stunRemainingSeconds = 0.0f;
    float starRemainingSeconds = 0.0f;
    std::array<float, 4> touchCooldownSeconds{};

    void Reset() noexcept {
        stunRemainingSeconds = 0.0f;
        starRemainingSeconds = 0.0f;
        touchCooldownSeconds.fill(0.0f);
    }

    void Tick(float deltaTime) noexcept {
        const float delta = std::max(0.0f, deltaTime);
        stunRemainingSeconds = std::max(0.0f, stunRemainingSeconds - delta);
        starRemainingSeconds = std::max(0.0f, starRemainingSeconds - delta);
        for (float& cooldown : touchCooldownSeconds) {
            cooldown = std::max(0.0f, cooldown - delta);
        }
    }

    void ActivateStar(float durationSeconds) noexcept {
        starRemainingSeconds = std::max(
            starRemainingSeconds,
            std::max(0.0f, durationSeconds)
        );
        touchCooldownSeconds.fill(0.0f);
        // 取得した瞬間から無敵として扱い、既存の硬直も解除する。
        stunRemainingSeconds = 0.0f;
    }

    bool ApplyStun(float durationSeconds) noexcept {
        if (HasStar()) {
            return false;
        }
        const float previous = stunRemainingSeconds;
        stunRemainingSeconds = std::max(
            stunRemainingSeconds,
            std::max(0.0f, durationSeconds)
        );
        return stunRemainingSeconds > previous;
    }

    bool TryConsumeStarTouch(
        PlayerId target,
        float cooldownSeconds
    ) noexcept {
        if (!HasStar() || target >= touchCooldownSeconds.size() ||
            touchCooldownSeconds[target] > 0.0f) {
            return false;
        }

        touchCooldownSeconds[target] = std::max(0.0f, cooldownSeconds);

        // 接触妨害はスター1個につき1回だけ。
        // CPUが同じ相手へ追突し続ける状態を避け、命中後は通常の陣取りへ戻す。
        starRemainingSeconds = 0.0f;
        return true;
    }

    bool IsStunned() const noexcept {
        return stunRemainingSeconds > 0.0f;
    }

    bool HasStar() const noexcept {
        return starRemainingSeconds > 0.0f;
    }

    float ResolveSpeedMultiplier(const TerritoryItemConfig& config) const noexcept {
        return HasStar() ? std::max(1.0f, config.starSpeedMultiplier) : 1.0f;
    }
};

struct TerritoryItemForecast {
    TerritoryItemType type = TerritoryItemType::Bomb;
    TileCoord tile{};
};

class TerritoryItemRandom {
public:
    using ForecastObserver = std::function<void(const TerritoryItemForecast&)>;

    explicit TerritoryItemRandom(std::uint32_t seed = 0xC01017E1u) noexcept {
        Reset(seed);
    }

    static void SetForecastObserver(ForecastObserver observer) {
        Observer() = std::move(observer);
    }

    static void ClearForecastObserver() {
        Observer() = {};
    }

    void Reset(std::uint32_t seed) noexcept {
        m_state = seed != 0 ? seed : 0xC01017E1u;
        m_lastType = TerritoryItemType::Star;
        m_sameTypeCount = 0;
        m_lastTile = {};
    }

    float NextUnit() noexcept {
        return static_cast<float>(NextU32() & 0x00FFFFFFu) /
            static_cast<float>(0x01000000u);
    }

    float NextRange(float minimum, float maximum) noexcept {
        if (maximum < minimum) {
            std::swap(minimum, maximum);
        }
        return minimum + (maximum - minimum) * NextUnit();
    }

    TerritoryItemType NextType() noexcept {
        TerritoryItemType candidate = (NextU32() & 1u) == 0u
            ? TerritoryItemType::Bomb
            : TerritoryItemType::Star;
        if (candidate == m_lastType && m_sameTypeCount >= 1) {
            candidate = candidate == TerritoryItemType::Bomb
                ? TerritoryItemType::Star
                : TerritoryItemType::Bomb;
        }
        if (candidate == m_lastType) {
            ++m_sameTypeCount;
        } else {
            m_lastType = candidate;
            m_sameTypeCount = 0;
        }
        if (ForecastObserver& observer = Observer(); observer) {
            observer({.type = candidate, .tile = m_lastTile});
        }
        return candidate;
    }

    TileCoord NextTile(int width, int height) noexcept {
        const int safeWidth = std::max(1, width);
        const int safeHeight = std::max(1, height);
        const int marginX = safeWidth >= 5 ? 1 : 0;
        const int marginY = safeHeight >= 5 ? 1 : 0;
        const int usableWidth = std::max(1, safeWidth - marginX * 2);
        const int usableHeight = std::max(1, safeHeight - marginY * 2);
        m_lastTile = {
            marginX + static_cast<int>(NextU32() % static_cast<std::uint32_t>(usableWidth)),
            marginY + static_cast<int>(NextU32() % static_cast<std::uint32_t>(usableHeight))
        };
        return m_lastTile;
    }

private:
    static ForecastObserver& Observer() {
        static ForecastObserver observer;
        return observer;
    }

    std::uint32_t NextU32() noexcept {
        std::uint32_t value = m_state;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        m_state = value != 0 ? value : 0xC01017E1u;
        return m_state;
    }

    std::uint32_t m_state = 0xC01017E1u;
    TerritoryItemType m_lastType = TerritoryItemType::Star;
    TileCoord m_lastTile{};
    int m_sameTypeCount = 0;
};

} // namespace MiniGameCollection::ColorTerritory
