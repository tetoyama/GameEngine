#pragma once

// Some movement values are authored in existing scenes. This wrapper preserves
// those scene fields while enforcing a runtime minimum for values whose older
// tuning is no longer sufficient for the current controller behaviour.
struct PlatformerMinimumFloat {
	float value = 0.0f;
	float minimum = 0.0f;

	constexpr PlatformerMinimumFloat(float initialValue, float minimumValue)
		: value(initialValue < minimumValue ? minimumValue : initialValue),
		  minimum(minimumValue) {}

	PlatformerMinimumFloat& operator=(float newValue) noexcept {
		value = newValue < minimum ? minimum : newValue;
		return *this;
	}

	constexpr operator float() const noexcept { return value; }
};

// Runtime tuning values are kept in one POD-like structure so the controller
// implementation remains readable and the eventual editor presentation can
// expose the same contract without touching engine internals.
struct PlatformerMovementSettings {
	float maxGroundSpeed = 7.0f;
	float groundAcceleration = 36.0f;
	float groundDeceleration = 44.0f;
	float airAcceleration = 14.0f;
	float airTurnAcceleration = 24.0f;
	float rotationSpeed = 14.0f;

	// Feel layer: preserve the authored base speed while adding a quick first
	// response and a small sustained-run reward. These are intentionally modest
	// so the existing course remains playable without widening every platform.
	float initialResponseAcceleration = 20.0f;
	float reverseAccelerationMultiplier = 1.75f;
	float runBuildSeconds = 0.58f;
	float runSpeedBonus = 1.15f;
	float runBuildDecay = 1.8f;
	float apexHorizontalAssist = 1.32f;
	float landingMomentumBoost = 0.28f;

	float firstJumpVelocity = 7.6f;
	float secondJumpVelocity = 8.4f;
	float thirdJumpVelocity = 10.0f;
	float stompBounceVelocity = 8.2f;
	float firstJumpForwardBoost = 0.30f;
	float secondJumpForwardBoost = 0.62f;
	float thirdJumpForwardBoost = 1.05f;
	// The previous 0.43 cut made short wall-jump taps lose most of their height.
	// Keep variable height, but retain enough launch velocity to feel deliberate.
	float variableJumpCut = 0.54f;
	float coyoteTime = 0.12f;
	float jumpBufferTime = 0.14f;

	float gravityAscending = 20.0f;
	float gravityDescending = 31.0f;
	float apexGravityScale = 0.50f;
	float apexVelocityThreshold = 1.30f;

	// The capsule foot is at the entity origin. Start the ray above the foot so it
	// can see a settled floor, but accept support only a short distance below it.
	// The former 0.30 m reach marked the player grounded well before contact and
	// made CameraZone/checkpoint trigger surfaces feel like they pulled the player.
	float groundProbeStart = 0.14f;
	float groundProbeDistance = 0.12f;
	// This is a velocity, not an acceleration. Keep only a small downward bias
	// after genuine contact so PhysX friction does not cancel horizontal motion.
	float groundSnapSpeed = 0.10f;
	float maxSlopeDegrees = 50.0f;

	float tripleJumpMinimumSpeed = 4.2f;
	float tripleJumpChainWindow = 0.32f;
	float tripleJumpDirectionDot = 0.25f;

	// Horizontal wall rays previously began inside the same capsule and therefore
	// created a permanent wall contact, allowing repeated wall kicks in open air.
	// Probe slightly above the capsule until the query API can exclude an actor
	// directly rather than relying only on scene-authored layer masks.
	float wallProbeHeight = 1.58f;
	float wallProbeDistance = 0.62f;
	float wallContactGrace = 0.16f;

	// Existing scenes still serialize the old 7.2 / 8.8 launch values. Clamp
	// assignments from those fields so the wall jump has a clear outward break
	// and reaches slightly above a second jump rather than feeling like a bump.
	PlatformerMinimumFloat wallKickHorizontalVelocity{8.6f, 8.6f};
	PlatformerMinimumFloat wallKickVerticalVelocity{10.6f, 10.6f};
	float wallKickInputInfluence = 1.20f;

	// The old control lock entered the generic deceleration branch and removed
	// several units of horizontal launch speed. Let normal air control blend in
	// immediately instead of braking the kick before it is visible.
	float wallKickControlLock = 0.0f;

	// TryWallKick already bypasses this block for a different wall and the ground
	// probe resets it when landing. A practically non-expiring value therefore
	// means: one kick per wall until the player lands or reaches another wall.
	float sameWallBlockTime = 1000000000.0f;

	float damageKnockbackHorizontal = 6.0f;
	float damageKnockbackVertical = 6.2f;
	float damageControlLock = 0.28f;
	float invulnerabilityTime = 1.15f;

	float fallRecoveryDelay = 0.42f;
	float respawnControlLock = 0.18f;
	float respawnInvulnerability = 1.0f;
};
