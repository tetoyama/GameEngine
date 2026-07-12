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
    float fleeStartRadius = 4.0f;
    float fleeWeight = 1.0f;
    float cohesionRadius = 3.25f;
    float cohesionWeight = 0.22f;
    float wallAvoidanceDistance = 1.4f;
    float wallAvoidanceWeight = 1.8f;
    float turnResponsiveness = 4.5f;
    float maximumSpeed = 3.2f;
    float calmSpeed = 0.35f;
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

        for (const Vec2 playerPosition : input.playerPositions) {
            const Vec2 away = input.position - playerPosition;
            const float distance = Length(away);
            if (distance >= fleeRadius || distance <= 0.00001f) {
                continue;
            }

            const float influence = 1.0f - distance / fleeRadius;
            flee += NormalizeOrZero(away) * influence;
            threatStrength += influence;
        }
        threatStrength = std::clamp(threatStrength, 0.0f, 1.0f);

        Vec2 cohesion{};
        Vec2 flockCenter{};
        std::size_t neighborCount = 0;
        const float cohesionRadiusSquared =
            config.cohesionRadius * config.cohesionRadius;

        for (const Vec2 flockPosition : input.flockPositions) {
            const float distanceSquared = DistanceSquared(input.position, flockPosition);
            if (distanceSquared <= 0.00001f ||
                distanceSquared > cohesionRadiusSquared) {
                continue;
            }
            flockCenter += flockPosition;
            ++neighborCount;
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

        const float speed = std::lerp(
            std::max(0.0f, config.calmSpeed),
            std::max(0.0f, config.maximumSpeed),
            std::clamp(threatStrength + wall.strength * 0.35f, 0.0f, 1.0f)
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
            const float strength = std::clamp(1.0f - wallDistance / distance, 0.0f, 1.0f);
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
            float nearestOpponent = std::numeric_limits<float>::infinity();
            for (const Vec2 opponent : context.opponentPositions) {
                nearestOpponent = std::min(
                    nearestOpponent,
                    Distance(opponent, predictedSheep)
                );
            }

            float utility = 7.0f;
            utility -= travelDistance * 0.75f;
            utility -= penDistance * 0.18f;
            if (nearestOpponent != std::numeric_limits<float>::infinity()) {
                const float contestValue = std::max(0.0f, 4.0f - nearestOpponent);
                utility += contestValue * (0.35f + endgame * difficulty.lateGameAggression);
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
