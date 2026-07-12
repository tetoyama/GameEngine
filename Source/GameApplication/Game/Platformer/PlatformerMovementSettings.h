#pragma once

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

	float firstJumpVelocity = 7.6f;
	float secondJumpVelocity = 8.4f;
	float thirdJumpVelocity = 10.0f;
	float stompBounceVelocity = 8.2f;
	float variableJumpCut = 0.48f;
	float coyoteTime = 0.12f;
	float jumpBufferTime = 0.14f;

	float gravityAscending = 20.0f;
	float gravityDescending = 31.0f;
	float apexGravityScale = 0.55f;
	float apexVelocityThreshold = 1.15f;

	float groundProbeStart = 0.16f;
	float groundProbeDistance = 0.52f;
	// This is a velocity, not an acceleration. A large value creates a large
	// contact impulse every fixed tick and lets PhysX friction cancel locomotion.
	// Keep only a small downward bias so the character remains attached to flat
	// ground without making grounded movement feel locked.
	float groundSnapSpeed = 0.12f;
	float maxSlopeDegrees = 50.0f;

	float tripleJumpMinimumSpeed = 4.2f;
	float tripleJumpChainWindow = 0.32f;
	float tripleJumpDirectionDot = 0.25f;

	float wallProbeHeight = 0.95f;
	float wallProbeDistance = 0.62f;
	float wallContactGrace = 0.14f;
	float wallKickHorizontalVelocity = 7.2f;
	float wallKickVerticalVelocity = 8.8f;
	float wallKickInputInfluence = 2.2f;
	float wallKickControlLock = 0.12f;
	float sameWallBlockTime = 0.38f;

	float damageKnockbackHorizontal = 6.0f;
	float damageKnockbackVertical = 6.2f;
	float damageControlLock = 0.28f;
	float invulnerabilityTime = 1.15f;

	float fallRecoveryDelay = 0.42f;
	float respawnControlLock = 0.22f;
	float respawnInvulnerability = 1.0f;
};