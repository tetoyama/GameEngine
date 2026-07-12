#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"
#include "Game/MiniGameCollection/Core/MiniGameMath.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace MiniGameCollection::Backshot {

enum class ShotResolution : std::uint8_t {
    Miss,
    Cooldown,
    OutOfRange,
    OutsideForwardArc,
    Blocked,
    FrontOrSideGuard,
    RearElimination
};

struct BackshotConfig {
    float range = 8.0f;
    float forwardAimDotThreshold = 0.94f;
    float rearHitDotThreshold = -0.45f;
    float cooldownSeconds = 0.8f;
};

struct CombatantSnapshot {
    PlayerId playerId = InvalidPlayerId;
    Vec2 position{};
    Vec2 forward{0.0f, 1.0f};
    bool alive = true;
    float shotCooldownRemainingSeconds = 0.0f;
};

struct ShotResult {
    ShotResolution resolution = ShotResolution::Miss;
    PlayerId attacker = InvalidPlayerId;
    PlayerId victim = InvalidPlayerId;
    float distance = 0.0f;
    float attackerAimDot = -1.0f;
    float victimRearDot = 1.0f;
};

class BackshotHitResolver {
public:
    static ShotResult Resolve(
        const CombatantSnapshot& attacker,
        const CombatantSnapshot& victim,
        bool hasLineOfSight,
        const BackshotConfig& config
    ) noexcept {
        ShotResult result{
            .resolution = ShotResolution::Miss,
            .attacker = attacker.playerId,
            .victim = victim.playerId
        };

        if (!attacker.alive || !victim.alive ||
            attacker.playerId == InvalidPlayerId ||
            victim.playerId == InvalidPlayerId ||
            attacker.playerId == victim.playerId) {
            return result;
        }

        if (attacker.shotCooldownRemainingSeconds > 0.0f) {
            result.resolution = ShotResolution::Cooldown;
            return result;
        }

        const Vec2 attackerToVictim = victim.position - attacker.position;
        result.distance = Length(attackerToVictim);
        if (result.distance > std::max(0.0f, config.range) ||
            result.distance <= 0.00001f) {
            result.resolution = ShotResolution::OutOfRange;
            return result;
        }

        const Vec2 shotDirection = NormalizeOrZero(attackerToVictim);
        const Vec2 attackerForward = NormalizeOrZero(attacker.forward);
        result.attackerAimDot = Dot(attackerForward, shotDirection);
        if (result.attackerAimDot <
            std::clamp(config.forwardAimDotThreshold, -1.0f, 1.0f)) {
            result.resolution = ShotResolution::OutsideForwardArc;
            return result;
        }

        if (!hasLineOfSight) {
            result.resolution = ShotResolution::Blocked;
            return result;
        }

        const Vec2 victimToAttacker = NormalizeOrZero(attacker.position - victim.position);
        const Vec2 victimForward = NormalizeOrZero(victim.forward);
        result.victimRearDot = Dot(victimForward, victimToAttacker);

        if (result.victimRearDot <=
            std::clamp(config.rearHitDotThreshold, -1.0f, 1.0f)) {
            result.resolution = ShotResolution::RearElimination;
        } else {
            result.resolution = ShotResolution::FrontOrSideGuard;
        }
        return result;
    }
};

struct TargetCandidate {
    CombatantSnapshot combatant{};
    bool hasLineOfSight = true;
    bool isTargetingSelf = false;
    float distanceToWallBehindTarget = 10.0f;
};

struct BackshotCpuContext {
    CombatantSnapshot self{};
    std::vector<TargetCandidate> candidates;
    float distanceToWallBehindSelf = 10.0f;
    float remainingTimeRatio = 1.0f;
    std::size_t livingPlayerCount = 4;
};

struct BackshotCpuDecision {
    PlayerId target = InvalidPlayerId;
    Vec2 desiredPosition{};
    bool shouldShoot = false;
    float utility = -std::numeric_limits<float>::infinity();
    float rearDanger = 0.0f;
};

class BackshotCpuEvaluator {
public:
    static std::optional<BackshotCpuDecision> Evaluate(
        const BackshotCpuContext& context,
        const BackshotConfig& config,
        const CpuDifficultyProfile& difficulty
    ) {
        if (!context.self.alive || context.self.playerId == InvalidPlayerId) {
            return std::nullopt;
        }

        const Vec2 selfForward = NormalizeOrZero(context.self.forward);
        const float endgame = 1.0f - std::clamp(context.remainingTimeRatio, 0.0f, 1.0f);
        const float rearDanger = ComputeRearDanger(context);
        std::optional<BackshotCpuDecision> best;

        for (const TargetCandidate& candidate : context.candidates) {
            if (!candidate.combatant.alive ||
                candidate.combatant.playerId == context.self.playerId) {
                continue;
            }

            const Vec2 selfToTarget = candidate.combatant.position - context.self.position;
            const float distance = Length(selfToTarget);
            if (distance <= 0.00001f) {
                continue;
            }

            const Vec2 targetDirection = selfToTarget / distance;
            const Vec2 targetForward = NormalizeOrZero(candidate.combatant.forward);
            const Vec2 targetToSelf = targetDirection * -1.0f;
            const float targetRearDot = Dot(targetForward, targetToSelf);
            const float aimDot = Dot(selfForward, targetDirection);
            const bool rearOpportunity =
                targetRearDot <= std::clamp(config.rearHitDotThreshold, -1.0f, 1.0f);
            const bool inShotArc =
                aimDot >= std::clamp(config.forwardAimDotThreshold, -1.0f, 1.0f);
            const bool inRange = distance <= std::max(0.0f, config.range);
            const bool cooldownReady = context.self.shotCooldownRemainingSeconds <= 0.0f;

            float utility = 0.0f;
            utility += rearOpportunity ? 7.0f : 1.0f;
            utility += candidate.hasLineOfSight ? 1.2f : -2.5f;
            utility += candidate.isTargetingSelf ? 1.6f : 0.0f;
            utility += inRange ? 1.0f : -std::max(0.0f, distance - config.range) * 0.5f;
            utility += endgame * difficulty.lateGameAggression *
                (context.livingPlayerCount <= 2 ? 2.0f : 0.7f);
            utility -= rearDanger * 2.2f;

            const Vec2 behindTarget =
                candidate.combatant.position - targetForward * 2.0f;
            Vec2 desiredPosition = behindTarget;

            if (rearDanger > 0.55f && context.distanceToWallBehindSelf > 1.2f) {
                desiredPosition = context.self.position - selfForward * 2.0f;
                utility += 0.8f;
            }

            const bool shouldShoot =
                cooldownReady &&
                candidate.hasLineOfSight &&
                inRange &&
                inShotArc;

            BackshotCpuDecision decision{
                .target = candidate.combatant.playerId,
                .desiredPosition = desiredPosition,
                .shouldShoot = shouldShoot,
                .utility = utility,
                .rearDanger = rearDanger
            };

            if (!best || IsBetter(decision, *best)) {
                best = decision;
            }
        }

        return best;
    }

private:
    static float ComputeRearDanger(const BackshotCpuContext& context) {
        const Vec2 selfForward = NormalizeOrZero(context.self.forward);
        float danger = 0.0f;

        for (const TargetCandidate& candidate : context.candidates) {
            if (!candidate.combatant.alive) {
                continue;
            }
            const Vec2 selfToEnemy = candidate.combatant.position - context.self.position;
            const float distance = Length(selfToEnemy);
            if (distance <= 0.00001f) {
                continue;
            }

            const float rearDot = Dot(selfForward, selfToEnemy / distance);
            if (rearDot >= 0.0f) {
                continue;
            }

            const float proximity = std::clamp(1.0f - distance / 8.0f, 0.0f, 1.0f);
            const float behind = std::clamp(-rearDot, 0.0f, 1.0f);
            const float targeting = candidate.isTargetingSelf ? 1.0f : 0.55f;
            danger = std::max(danger, proximity * behind * targeting);
        }
        return danger;
    }

    static bool IsBetter(
        const BackshotCpuDecision& candidate,
        const BackshotCpuDecision& currentBest
    ) noexcept {
        constexpr float epsilon = 0.0001f;
        if (candidate.utility > currentBest.utility + epsilon) {
            return true;
        }
        if (candidate.utility + epsilon < currentBest.utility) {
            return false;
        }
        return candidate.target < currentBest.target;
    }
};

} // namespace MiniGameCollection::Backshot
