#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"
#include "Game/MiniGameCollection/Core/MiniGameMath.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace MiniGameCollection::SheepRoundup {

struct Bounds2 {
    Vec2 minimum{-10.0f, -10.0f};
    Vec2 maximum{10.0f, 10.0f};
};

struct SheepSteeringConfig {
    float fleeStartRadius = 3.4f;
    float fleeWeight = 1.3f;
    float separationRadius = 0.95f;
    float separationWeight = 0.82f;
    float cohesionRadius = 3.0f;
    float cohesionWeight = 0.12f;
    float wallAvoidanceDistance = 1.55f;
    float wallAvoidanceWeight = 2.2f;
    float turnResponsiveness = 6.0f;
    float maximumSpeed = 2.9f;
    float calmSpeed = 0.12f;
};

struct SheepSteeringInput {
    Vec2 position{};
    Vec2 previousDirection{0.0f, 1.0f};
    std::vector<Vec2> playerPositions;
    std::vector<Vec2> flockPositions;
    Bounds2 movementBounds{};
};

struct SheepSteeringOutput {
    Vec2 direction{};
    Vec2 velocity{};
    float threatStrength = 0.0f;
    bool avoidingWall = false;
};

class SheepSteeringModel {
public:
    static SheepSteeringOutput Compute(
        const SheepSteeringInput& input,
        const SheepSteeringConfig& config,
        float deltaTime
    ) {
        const float fleeRadius = std::max(0.01f, config.fleeStartRadius);
        Vec2 flee{};
        float threatStrength = 0.0f;
        float nearestThreatDistance = std::numeric_limits<float>::infinity();
        Vec2 nearestThreatDirection{};

        for (const Vec2 playerPosition : input.playerPositions) {
            const Vec2 away = input.position - playerPosition;
            const float distance = Length(away);
            if (distance >= fleeRadius || distance <= 0.00001f) {
                continue;
            }

            const float influence = std::clamp(
                1.0f - distance / fleeRadius,
                0.0f,
                1.0f
            );
            // 距離の近いプレイヤーほど急激に影響を強める。
            // 複数人に囲まれても合成方向が毎Frame反転しにくい。
            flee += NormalizeOrZero(away) * (influence * influence);
            threatStrength = std::max(threatStrength, influence);
            if (distance < nearestThreatDistance) {
                nearestThreatDistance = distance;
                nearestThreatDirection = NormalizeOrZero(away);
            }
        }

        if (LengthSquared(nearestThreatDirection) > 0.00001f) {
            flee += nearestThreatDirection * (threatStrength * 0.7f);
        }

        Vec2 cohesion{};
        Vec2 separation{};
        Vec2 flockCenter{};
        std::size_t neighborCount = 0;
        const float cohesionRadius = std::max(0.01f, config.cohesionRadius);
        const float cohesionRadiusSquared = cohesionRadius * cohesionRadius;
        const float separationRadius = std::max(0.01f, config.separationRadius);
        const float separationRadiusSquared = separationRadius * separationRadius;

        for (const Vec2 flockPosition : input.flockPositions) {
            const Vec2 away = input.position - flockPosition;
            const float distanceSquared = LengthSquared(away);
            if (distanceSquared <= 0.00001f) {
                continue;
            }

            if (distanceSquared <= cohesionRadiusSquared) {
                flockCenter += flockPosition;
                ++neighborCount;
            }

            if (distanceSquared <= separationRadiusSquared) {
                const float distance = std::sqrt(distanceSquared);
                const float strength = std::clamp(
                    1.0f - distance / separationRadius,
                    0.0f,
                    1.0f
                );
                separation += NormalizeOrZero(away) * strength;
            }
        }

        if (neighborCount > 0) {
            flockCenter = flockCenter / static_cast<float>(neighborCount);
            cohesion = NormalizeOrZero(flockCenter - input.position);
        }

        const WallAvoidance wall = ComputeWallAvoidance(
            input.position,
            input.movementBounds,
            config.wallAvoidanceDistance
        );

        Vec2 desired =
            flee * config.fleeWeight +
            separation * config.separationWeight +
            cohesion * config.cohesionWeight +
            wall.direction * config.wallAvoidanceWeight;

        if (LengthSquared(desired) <= 0.00001f) {
            desired = NormalizeOrZero(input.previousDirection);
            if (LengthSquared(desired) <= 0.00001f) {
                desired = {0.0f, 1.0f};
            }
        } else {
            desired = NormalizeOrZero(desired);
        }

        const Vec2 previous = NormalizeOrZero(input.previousDirection);
        const float turnAmount = std::clamp(
            std::max(0.0f, config.turnResponsiveness) * std::max(0.0f, deltaTime),
            0.0f,
            1.0f
        );
        Vec2 direction = NormalizeOrZero(Lerp(previous, desired, turnAmount));
        if (LengthSquared(direction) <= 0.00001f) {
            direction = desired;
        }

        const float movementIntent = std::clamp(
            threatStrength + wall.strength * 0.45f +
                std::min(1.0f, Length(separation)) * 0.25f,
            0.0f,
            1.0f
        );
        const float speed = std::lerp(
            std::max(0.0f, config.calmSpeed),
            std::max(0.0f, config.maximumSpeed),
            movementIntent
        );

        return {
            .direction = direction,
            .velocity = direction * speed,
            .threatStrength = threatStrength,
            .avoidingWall = wall.strength > 0.0f
        };
    }

    static Vec2 ClampInsideBounds(
        Vec2 position,
        const Bounds2& bounds,
        float padding = 0.0f
    ) noexcept {
        const float safePadding = std::max(0.0f, padding);
        position.x = std::clamp(
            position.x,
            bounds.minimum.x + safePadding,
            bounds.maximum.x - safePadding
        );
        position.y = std::clamp(
            position.y,
            bounds.minimum.y + safePadding,
            bounds.maximum.y - safePadding
        );
        return position;
    }

private:
    struct WallAvoidance {
        Vec2 direction{};
        float strength = 0.0f;
    };

    static WallAvoidance ComputeWallAvoidance(
        Vec2 position,
        const Bounds2& bounds,
        float avoidanceDistance
    ) noexcept {
        const float distance = std::max(0.01f, avoidanceDistance);
        Vec2 direction{};
        float strongest = 0.0f;

        const auto addAxis = [&](float wallDistance, Vec2 inward) {
            if (wallDistance >= distance) {
                return;
            }
            const float strength = std::clamp(
                1.0f - wallDistance / distance,
                0.0f,
                1.0f
            );
            direction += inward * strength;
            strongest = std::max(strongest, strength);
        };

        addAxis(position.x - bounds.minimum.x, {1.0f, 0.0f});
        addAxis(bounds.maximum.x - position.x, {-1.0f, 0.0f});
        addAxis(position.y - bounds.minimum.y, {0.0f, 1.0f});
        addAxis(bounds.maximum.y - position.y, {0.0f, -1.0f});

        return {
            .direction = NormalizeOrZero(direction),
            .strength = strongest
        };
    }
};

struct SheepTargetCandidate {
    std::size_t sheepIndex = 0;
    Vec2 sheepPosition{};
    Vec2 sheepVelocity{};
    bool alreadyScored = false;
};

struct SheepCpuContext {
    Vec2 cpuPosition{};
    Vec2 ownPenCenter{};
    float remainingTimeRatio = 1.0f;
    std::vector<Vec2> opponentPositions;
};

struct SheepCpuDecision {
    std::size_t sheepIndex = 0;
    Vec2 interceptPosition{};
    float utility = -std::numeric_limits<float>::infinity();
};

class SheepCpuEvaluator {
public:
    static std::optional<SheepCpuDecision> ChooseSheep(
        const std::vector<SheepTargetCandidate>& candidates,
        const SheepCpuContext& context,
        const CpuDifficultyProfile& difficulty,
        float standOffDistance = 1.6f
    ) {
        std::optional<SheepCpuDecision> best;
        const float prediction = std::max(0.0f, difficulty.predictionSeconds);
        const float endgame = 1.0f - std::clamp(context.remainingTimeRatio, 0.0f, 1.0f);

        for (const SheepTargetCandidate& candidate : candidates) {
            if (candidate.alreadyScored) {
                continue;
            }

            const Vec2 predictedSheep =
                candidate.sheepPosition + candidate.sheepVelocity * prediction;
            Vec2 oppositePen = NormalizeOrZero(predictedSheep - context.ownPenCenter);
            if (LengthSquared(oppositePen) <= 0.00001f) {
                oppositePen = {0.0f, 1.0f};
            }
            const Vec2 intercept = predictedSheep + oppositePen * standOffDistance;

            const float travelDistance = Distance(context.cpuPosition, intercept);
            const float penDistance = Distance(predictedSheep, context.ownPenCenter);
            const Vec2 towardPen = NormalizeOrZero(context.ownPenCenter - predictedSheep);
            const float deliveryMomentum = Dot(candidate.sheepVelocity, towardPen);

            float nearestOpponent = std::numeric_limits<float>::infinity();
            for (const Vec2 opponent : context.opponentPositions) {
                nearestOpponent = std::min(
                    nearestOpponent,
                    Distance(opponent, predictedSheep)
                );
            }

            float utility = 7.0f;
            utility -= travelDistance * 0.68f;
            utility -= penDistance * 0.24f;
            // すでに自分の囲いへ進んでいる羊を継続して押し、
            // 毎回別の羊へ切り替える不自然な挙動を抑える。
            utility += deliveryMomentum * 1.15f;

            if (nearestOpponent != std::numeric_limits<float>::infinity()) {
                const float contestValue = std::max(0.0f, 4.0f - nearestOpponent);
                const float contestBias =
                    -0.12f + endgame * difficulty.lateGameAggression * 0.72f;
                utility += contestValue * contestBias;
            }

            SheepCpuDecision decision{
                .sheepIndex = candidate.sheepIndex,
                .interceptPosition = intercept,
                .utility = utility
            };

            if (!best || decision.utility > best->utility) {
                best = decision;
            }
        }

        return best;
    }
};

} // namespace MiniGameCollection::SheepRoundup
