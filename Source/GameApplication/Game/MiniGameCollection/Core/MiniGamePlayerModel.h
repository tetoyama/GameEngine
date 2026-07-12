#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"
#include "Game/MiniGameCollection/Core/MiniGameMath.h"

#include <algorithm>
#include <cmath>

namespace MiniGameCollection {

struct MovementBounds {
    Vec2 minimum{-10.0f, -10.0f};
    Vec2 maximum{10.0f, 10.0f};
};

struct MiniGamePlayerConfig {
    float acceleration = 20.0f;
    float deceleration = 26.0f;
    float maximumSpeed = 5.0f;
    float turnResponsiveness = 12.0f;
    float knockbackDamping = 8.0f;
    float collisionRadius = 0.45f;
};

struct MiniGamePlayerInput {
    Vec2 move{};
    bool actionPressed = false;
};

struct MiniGamePlayerState {
    PlayerId playerId = InvalidPlayerId;
    Vec2 position{};
    Vec2 velocity{};
    Vec2 forward{0.0f, 1.0f};
    Vec2 knockbackVelocity{};
    bool inputEnabled = false;
    bool eliminated = false;
};

class MiniGamePlayerModel {
public:
    static void Tick(
        MiniGamePlayerState& state,
        const MiniGamePlayerInput& input,
        const MiniGamePlayerConfig& config,
        const MovementBounds& bounds,
        float deltaTime
    ) noexcept {
        const float dt = std::max(0.0f, deltaTime);
        if (dt <= 0.0f || state.eliminated) {
            return;
        }

        Vec2 desiredDirection{};
        if (state.inputEnabled) {
            desiredDirection = ClampLength(input.move, 1.0f);
        }

        const bool hasMovementInput = LengthSquared(desiredDirection) > 0.0001f;
        const Vec2 targetVelocity =
            hasMovementInput
                ? NormalizeOrZero(desiredDirection) * std::max(0.0f, config.maximumSpeed)
                : Vec2{};

        const float response = hasMovementInput
            ? std::max(0.0f, config.acceleration)
            : std::max(0.0f, config.deceleration);
        state.velocity = MoveTowards(state.velocity, targetVelocity, response * dt);

        if (hasMovementInput) {
            const Vec2 targetForward = NormalizeOrZero(desiredDirection);
            const float turnAmount = std::clamp(
                std::max(0.0f, config.turnResponsiveness) * dt,
                0.0f,
                1.0f
            );
            const Vec2 turned = NormalizeOrZero(Lerp(state.forward, targetForward, turnAmount));
            if (LengthSquared(turned) > 0.0001f) {
                state.forward = turned;
            }
        }

        state.position += (state.velocity + state.knockbackVelocity) * dt;
        state.position = ClampPosition(state.position, bounds, config.collisionRadius);

        const float damping = std::clamp(
            std::max(0.0f, config.knockbackDamping) * dt,
            0.0f,
            1.0f
        );
        state.knockbackVelocity = Lerp(state.knockbackVelocity, {}, damping);
    }

    static void ApplyKnockback(
        MiniGamePlayerState& state,
        Vec2 direction,
        float impulse,
        float maximumKnockbackSpeed = 5.0f
    ) noexcept {
        if (state.eliminated) {
            return;
        }
        state.knockbackVelocity = ClampLength(
            state.knockbackVelocity +
                NormalizeOrZero(direction) * std::max(0.0f, impulse),
            std::max(0.0f, maximumKnockbackSpeed)
        );
    }

    static void ResolveSoftContact(
        MiniGamePlayerState& lhs,
        MiniGamePlayerState& rhs,
        const MiniGamePlayerConfig& config,
        float pushImpulse = 1.25f
    ) noexcept {
        if (lhs.eliminated || rhs.eliminated) {
            return;
        }

        Vec2 separation = lhs.position - rhs.position;
        float distance = Length(separation);
        const float minimumDistance = std::max(0.0f, config.collisionRadius) * 2.0f;
        if (distance >= minimumDistance) {
            return;
        }

        if (distance <= 0.0001f) {
            separation = lhs.playerId <= rhs.playerId
                ? Vec2{-1.0f, 0.0f}
                : Vec2{1.0f, 0.0f};
            distance = 1.0f;
        }

        const Vec2 normal = separation / distance;
        const float penetration = minimumDistance - distance;
        const Vec2 correction = normal * (penetration * 0.5f);
        lhs.position += correction;
        rhs.position = rhs.position - correction;

        const float safeImpulse = std::max(0.0f, pushImpulse);
        ApplyKnockback(lhs, normal, safeImpulse);
        ApplyKnockback(rhs, normal * -1.0f, safeImpulse);
    }

private:
    static Vec2 MoveTowards(Vec2 current, Vec2 target, float maximumDelta) noexcept {
        const Vec2 difference = target - current;
        const float distance = Length(difference);
        if (distance <= maximumDelta || distance <= 0.0001f) {
            return target;
        }
        return current + difference / distance * maximumDelta;
    }

    static Vec2 ClampPosition(
        Vec2 position,
        const MovementBounds& bounds,
        float radius
    ) noexcept {
        const float safeRadius = std::max(0.0f, radius);
        position.x = std::clamp(
            position.x,
            bounds.minimum.x + safeRadius,
            bounds.maximum.x - safeRadius
        );
        position.y = std::clamp(
            position.y,
            bounds.minimum.y + safeRadius,
            bounds.maximum.y - safeRadius
        );
        return position;
    }
};

} // namespace MiniGameCollection
