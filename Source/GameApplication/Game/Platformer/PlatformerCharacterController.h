#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/ColliderComponent.h"
#include "Engine/Scene/Component/CameraComponent.h"
#include "Game/Platformer/PlatformerMovementSettings.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

class PlatformerCharacterController : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerCharacterController)
		REFLECT_FIELD(float, maxGroundSpeed, 7.0f)
		REFLECT_FIELD(float, groundAcceleration, 36.0f)
		REFLECT_FIELD(float, groundDeceleration, 44.0f)
		REFLECT_FIELD(float, airAcceleration, 14.0f)
		REFLECT_FIELD(float, airTurnAcceleration, 24.0f)
		REFLECT_FIELD(float, rotationSpeed, 14.0f)
		REFLECT_FIELD(float, firstJumpVelocity, 7.6f)
		REFLECT_FIELD(float, secondJumpVelocity, 8.4f)
		REFLECT_FIELD(float, thirdJumpVelocity, 10.0f)
		REFLECT_FIELD(float, stompBounceVelocity, 8.2f)
		REFLECT_FIELD(float, coyoteTime, 0.12f)
		REFLECT_FIELD(float, jumpBufferTime, 0.14f)
		REFLECT_FIELD(float, gravityAscending, 20.0f)
		REFLECT_FIELD(float, gravityDescending, 31.0f)
		REFLECT_FIELD(float, apexGravityScale, 0.55f)
		REFLECT_FIELD(float, maxSlopeDegrees, 50.0f)
		REFLECT_FIELD(float, tripleJumpMinimumSpeed, 4.2f)
		REFLECT_FIELD(float, tripleJumpChainWindow, 0.32f)
		REFLECT_FIELD(float, wallProbeDistance, 0.62f)
		REFLECT_FIELD(float, wallContactGrace, 0.14f)
		REFLECT_FIELD(float, wallKickHorizontalVelocity, 7.2f)
		REFLECT_FIELD(float, wallKickVerticalVelocity, 8.8f)
		REFLECT_FIELD(float, killY, -14.0f)
		REFLECT_FIELD(int, maxHealth, 3)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		node["SelfLayerBit"] = selfLayerBit;
		node["Checkpoint"] = checkpointPosition;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		if(node["SelfLayerBit"]) selfLayerBit = node["SelfLayerBit"].as<uint32_t>();
		if(node["Checkpoint"]) checkpointPosition = node["Checkpoint"].as<Vector3>();
		ApplyReflectedSettings();
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer Character Controller");
		INSPECTOR_FIELDS();
		ImGui::Text("Grounded: %s", grounded ? "true" : "false");
		ImGui::Text("Jump Stage: %d", jumpStage);
		ImGui::Text("Vertical Velocity: %.2f", verticalVelocity);
		ImGui::Text("Health: %d / %d", health, maxHealth);
	}

	void OnStart() override {
		ApplyReflectedSettings();
		transform = GetComponentRef<TransformComponent>();
		collider = GetComponentRef<ColliderComponent>();
		cameraTransform = PlatformerSceneAccess::FindFirst<CameraComponent>(m_ref.GetScene()).GetEntityRef().IsValid()
			? ComponentRef<TransformComponent>(PlatformerSceneAccess::FindFirst<CameraComponent>(m_ref.GetScene()).GetEntityRef())
			: ComponentRef<TransformComponent>{};

		if(auto* t = transform.TryGet()) {
			if(checkpointPosition.length() <= 0.0001f) checkpointPosition = t->position;
		}

		health = (std::max)(1, maxHealth);
		controlEnabled = true;
		cleared = false;
		grounded = false;
		wasGrounded = false;
		verticalVelocity = 0.0f;
		jumpStage = 0;
		jumpBufferTimer = 0.0f;
		coyoteTimer = 0.0f;
		wallGraceTimer = 0.0f;
		invulnerabilityTimer = 0.0f;
		controlLockTimer = 0.0f;
		fallRecoveryTimer = 0.0f;
		respawning = false;
		ConfigureRigidbody();
	}

	void OnUpdate(float dt) override {
		if(GetKeyDown(VK_SPACE)) {
			jumpBufferTimer = settings.jumpBufferTime;
		}
		jumpHeld = GetKey(VK_SPACE);
		if(GetKeyUp(VK_SPACE)) jumpReleaseQueued = true;
	}

	void OnFixedUpdate(float dt) override {
		if(dt <= 0.0f) return;
		ApplyReflectedSettings();
		TickTimers(dt);

		auto* t = transform.TryGet();
		if(!t) return;

		if(t->position.y < killY && !respawning) {
			BeginFallRecovery();
		}

		if(respawning) {
			fallRecoveryTimer -= dt;
			StopRigidbodyMotion();
			if(fallRecoveryTimer <= 0.0f) FinishFallRecovery();
			return;
		}

		if(!collider.IsValid()) collider = GetComponentRef<ColliderComponent>();
		auto* col = collider.TryGet();
		if(!col || !col->pRigidbodyDynamic) return;
		auto* rigid = col->pRigidbodyDynamic;
		rigid->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);

		const Vector3 inputDirection = ReadCameraRelativeInput();
		const bool hasInput = inputDirection.length() > 0.0001f;
		physx::PxVec3 velocityPx = rigid->getLinearVelocity();
		Vector3 velocity(velocityPx.x, velocityPx.y, velocityPx.z);
		verticalVelocity = velocity.y;

		UpdateGroundProbe(*t);
		UpdateWallProbe(*t, inputDirection, velocity);
		UpdateTripleJumpLanding(inputDirection, velocity);

		if(jumpBufferTimer > 0.0f && controlEnabled && controlLockTimer <= 0.0f) {
			if(!grounded && wallGraceTimer > 0.0f && TryWallKick(inputDirection, velocity)) {
				jumpBufferTimer = 0.0f;
			} else if(coyoteTimer > 0.0f) {
				PerformGroundJump(inputDirection, velocity);
				jumpBufferTimer = 0.0f;
			}
		}

		if(jumpReleaseQueued) {
			if(verticalVelocity > 0.0f && canCutJump) {
				verticalVelocity *= settings.variableJumpCut;
				canCutJump = false;
			}
			jumpReleaseQueued = false;
		}

		if(controlEnabled && controlLockTimer <= 0.0f) {
			UpdateHorizontalVelocity(inputDirection, hasInput, velocity, dt);
		} else if(cleared) {
			velocity.x = MoveTowards(velocity.x, 0.0f, settings.groundDeceleration * dt);
			velocity.z = MoveTowards(velocity.z, 0.0f, settings.groundDeceleration * dt);
		}

		if(grounded && groundDetachTimer <= 0.0f && verticalVelocity <= 0.0f) {
			const Vector3 projected = ProjectOnPlane(Vector3(velocity.x, 0.0f, velocity.z), groundNormal);
			velocity.x = projected.x;
			velocity.z = projected.z;
			verticalVelocity = (std::min)(projected.y, -settings.groundSnapSpeed);
		} else {
			ApplyGravity(dt);
		}

		velocity.y = verticalVelocity;
		rigid->setLinearVelocity(physx::PxVec3(velocity.x, velocity.y, velocity.z));

		if(hasInput && controlEnabled && controlLockTimer <= 0.0f) {
			RotateToward(*t, inputDirection, dt);
		}

		wasGrounded = grounded;
	}

	void OnStop() override {
		if(auto* col = collider.TryGet()) {
			if(col->pRigidbodyDynamic) {
				col->pRigidbodyDynamic->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, false);
			}
		}
	}

	void SetCheckpoint(const Vector3& position) {
		checkpointPosition = position;
	}

	const Vector3& GetCheckpoint() const { return checkpointPosition; }
	bool IsGrounded() const { return grounded; }
	bool IsDescending() const { return !grounded && verticalVelocity < -0.05f; }
	bool IsControlEnabled() const { return controlEnabled && controlLockTimer <= 0.0f && !respawning; }
	bool IsInvulnerable() const { return invulnerabilityTimer > 0.0f; }
	bool IsCleared() const { return cleared; }
	float GetVerticalVelocity() const { return verticalVelocity; }
	float GetHorizontalSpeed() const { return horizontalSpeed; }
	int GetJumpStage() const { return jumpStage; }
	int GetHealth() const { return health; }
	int GetMaxHealth() const { return (std::max)(1, maxHealth); }
	const Vector3& GetLastWallNormal() const { return lastWallNormal; }

	uint32_t GetJumpEventRevision() const { return jumpEventRevision; }
	uint32_t GetLandEventRevision() const { return landEventRevision; }
	uint32_t GetWallKickEventRevision() const { return wallKickEventRevision; }
	uint32_t GetStompEventRevision() const { return stompEventRevision; }
	uint32_t GetDamageEventRevision() const { return damageEventRevision; }
	uint32_t GetRespawnEventRevision() const { return respawnEventRevision; }

	void SetControlEnabled(bool enabled) {
		controlEnabled = enabled;
	}

	void BeginClear() {
		cleared = true;
		controlEnabled = false;
		controlLockTimer = 9999.0f;
	}

	void ApplyStompBounce(float velocity = -1.0f) {
		verticalVelocity = velocity > 0.0f ? velocity : settings.stompBounceVelocity;
		grounded = false;
		groundDetachTimer = 0.08f;
		canCutJump = false;
		jumpStage = 0;
		++stompEventRevision;
	}

	bool ApplyDamage(const Vector3& damageSource) {
		if(IsInvulnerable() || respawning || cleared) return false;
		auto* t = transform.TryGet();
		if(!t) return false;

		--health;
		invulnerabilityTimer = settings.invulnerabilityTime;
		controlLockTimer = settings.damageControlLock;
		controlEnabled = true;
		grounded = false;
		jumpStage = 0;

		Vector3 away = t->position - damageSource;
		away.y = 0.0f;
		if(away.length() <= 0.0001f) away = Vector3(0.0f, 0.0f, -1.0f);
		away = away.normalize();

		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				rigid->setLinearVelocity(physx::PxVec3(
					away.x * settings.damageKnockbackHorizontal,
					settings.damageKnockbackVertical,
					away.z * settings.damageKnockbackHorizontal));
			}
		}
		verticalVelocity = settings.damageKnockbackVertical;
		++damageEventRevision;

		if(health <= 0) {
			health = (std::max)(1, maxHealth);
			BeginFallRecovery();
		}
		return true;
	}

private:
	void ApplyReflectedSettings() {
		settings.maxGroundSpeed = maxGroundSpeed;
		settings.groundAcceleration = groundAcceleration;
		settings.groundDeceleration = groundDeceleration;
		settings.airAcceleration = airAcceleration;
		settings.airTurnAcceleration = airTurnAcceleration;
		settings.rotationSpeed = rotationSpeed;
		settings.firstJumpVelocity = firstJumpVelocity;
		settings.secondJumpVelocity = secondJumpVelocity;
		settings.thirdJumpVelocity = thirdJumpVelocity;
		settings.stompBounceVelocity = stompBounceVelocity;
		settings.coyoteTime = coyoteTime;
		settings.jumpBufferTime = jumpBufferTime;
		settings.gravityAscending = gravityAscending;
		settings.gravityDescending = gravityDescending;
		settings.apexGravityScale = apexGravityScale;
		settings.maxSlopeDegrees = maxSlopeDegrees;
		settings.tripleJumpMinimumSpeed = tripleJumpMinimumSpeed;
		settings.tripleJumpChainWindow = tripleJumpChainWindow;
		settings.wallProbeDistance = wallProbeDistance;
		settings.wallContactGrace = wallContactGrace;
		settings.wallKickHorizontalVelocity = wallKickHorizontalVelocity;
		settings.wallKickVerticalVelocity = wallKickVerticalVelocity;
	}

	void ConfigureRigidbody() {
		if(!collider.IsValid()) collider = GetComponentRef<ColliderComponent>();
		if(auto* col = collider.TryGet()) {
			if(col->pRigidbodyDynamic) {
				col->pRigidbodyDynamic->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
			}
		}
	}

	void TickTimers(float dt) {
		jumpBufferTimer = (std::max)(0.0f, jumpBufferTimer - dt);
		coyoteTimer = (std::max)(0.0f, coyoteTimer - dt);
		wallGraceTimer = (std::max)(0.0f, wallGraceTimer - dt);
		tripleChainTimer = (std::max)(0.0f, tripleChainTimer - dt);
		invulnerabilityTimer = (std::max)(0.0f, invulnerabilityTimer - dt);
		controlLockTimer = (std::max)(0.0f, controlLockTimer - dt);
		groundDetachTimer = (std::max)(0.0f, groundDetachTimer - dt);
		sameWallBlockTimer = (std::max)(0.0f, sameWallBlockTimer - dt);
		if(tripleChainTimer <= 0.0f && grounded) jumpStage = 0;
	}

	Vector3 ReadCameraRelativeInput() {
		Vector3 raw;
		if(GetKey('W')) raw.z += 1.0f;
		if(GetKey('S')) raw.z -= 1.0f;
		if(GetKey('D')) raw.x += 1.0f;
		if(GetKey('A')) raw.x -= 1.0f;
		if(raw.length() <= 0.0001f) return {};
		raw = raw.normalize();

		if(!cameraTransform.IsValid()) {
			auto camera = PlatformerSceneAccess::FindFirst<CameraComponent>(m_ref.GetScene());
			if(camera.IsValid()) cameraTransform = ComponentRef<TransformComponent>(camera.GetEntityRef());
		}

		if(auto* cam = cameraTransform.TryGet()) {
			Vector3 forward = cam->front();
			Vector3 right = cam->right();
			forward.y = 0.0f;
			right.y = 0.0f;
			if(forward.length() > 0.0001f) forward = forward.normalize();
			if(right.length() > 0.0001f) right = right.normalize();
			Vector3 result = forward * raw.z + right * raw.x;
			return result.length() > 0.0001f ? result.normalize() : Vector3{};
		}
		return raw;
	}

	void UpdateGroundProbe(const TransformComponent& t) {
		auto* physics = PlatformerSceneAccess::Physics(m_ref.GetScene());
		if(!physics) {
			grounded = false;
			return;
		}

		const RayHit hit = physics->RaycastWithMask(
			physx::PxVec3(t.position.x, t.position.y + settings.groundProbeStart, t.position.z),
			physx::PxVec3(0.0f, -1.0f, 0.0f),
			settings.groundProbeStart + settings.groundProbeDistance,
			selfLayerBit);

		const float minNormalY = std::cos(settings.maxSlopeDegrees * DirectX::XM_PI / 180.0f);
		const bool validGround = hit.hit && hit.normal.y >= minNormalY;
		groundNormal = validGround ? Vector3(hit.normal.x, hit.normal.y, hit.normal.z) : Vector3(0.0f, 1.0f, 0.0f);
		grounded = validGround && groundDetachTimer <= 0.0f && verticalVelocity <= 1.0f;
		if(grounded) coyoteTimer = settings.coyoteTime;
	}

	void UpdateWallProbe(const TransformComponent& t, const Vector3& inputDirection, const Vector3& velocity) {
		if(grounded) {
			wallGraceTimer = 0.0f;
			lastWallNormal = {};
			sameWallBlockTimer = 0.0f;
			return;
		}
		auto* physics = PlatformerSceneAccess::Physics(m_ref.GetScene());
		if(!physics) return;

		Vector3 velocityDirection(velocity.x, 0.0f, velocity.z);
		if(velocityDirection.length() > 0.0001f) velocityDirection = velocityDirection.normalize();

		const Vector3 candidates[6] = {
			inputDirection,
			velocityDirection,
			Vector3(1.0f, 0.0f, 0.0f),
			Vector3(-1.0f, 0.0f, 0.0f),
			Vector3(0.0f, 0.0f, 1.0f),
			Vector3(0.0f, 0.0f, -1.0f)
		};

		float bestDistance = settings.wallProbeDistance + 1.0f;
		Vector3 bestNormal;
		for(const Vector3& candidate : candidates) {
			if(candidate.length() <= 0.0001f) continue;
			const Vector3 direction = candidate.normalize();
			const RayHit hit = physics->RaycastWithMask(
				physx::PxVec3(t.position.x, t.position.y + settings.wallProbeHeight, t.position.z),
				physx::PxVec3(direction.x, 0.0f, direction.z),
				settings.wallProbeDistance,
				selfLayerBit);
			if(!hit.hit || std::abs(hit.normal.y) > 0.35f || hit.distance >= bestDistance) continue;
			bestDistance = hit.distance;
			bestNormal = Vector3(hit.normal.x, 0.0f, hit.normal.z).normalize();
		}

		if(bestNormal.length() > 0.0001f) {
			lastWallNormal = bestNormal;
			wallGraceTimer = settings.wallContactGrace;
		}
	}

	void UpdateTripleJumpLanding(const Vector3& inputDirection, const Vector3& velocity) {
		horizontalSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
		if(grounded && !wasGrounded) {
			++landEventRevision;
			Vector3 currentDirection = inputDirection;
			if(currentDirection.length() <= 0.0001f) currentDirection = Vector3(velocity.x, 0.0f, velocity.z).normalize();
			const bool speedOkay = horizontalSpeed >= settings.tripleJumpMinimumSpeed;
			const bool directionOkay = lastJumpDirection.length() <= 0.0001f ||
				currentDirection.length() <= 0.0001f ||
				lastJumpDirection.dot(currentDirection) >= settings.tripleJumpDirectionDot;
			if(jumpStage > 0 && speedOkay && directionOkay) {
				tripleChainTimer = settings.tripleJumpChainWindow;
			} else {
				jumpStage = 0;
				tripleChainTimer = 0.0f;
			}
		}
	}

	void PerformGroundJump(const Vector3& inputDirection, Vector3& velocity) {
		Vector3 currentDirection = inputDirection;
		if(currentDirection.length() <= 0.0001f) currentDirection = Vector3(velocity.x, 0.0f, velocity.z).normalize();
		const float speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
		const bool chained = tripleChainTimer > 0.0f &&
			speed >= settings.tripleJumpMinimumSpeed &&
			(lastJumpDirection.length() <= 0.0001f || currentDirection.length() <= 0.0001f ||
			 lastJumpDirection.dot(currentDirection) >= settings.tripleJumpDirectionDot);

		jumpStage = chained ? (std::min)(3, jumpStage + 1) : 1;
		verticalVelocity = jumpStage == 3 ? settings.thirdJumpVelocity
			: jumpStage == 2 ? settings.secondJumpVelocity
			: settings.firstJumpVelocity;
		lastJumpDirection = currentDirection;
		tripleChainTimer = 0.0f;
		coyoteTimer = 0.0f;
		grounded = false;
		groundDetachTimer = 0.08f;
		canCutJump = true;
		++jumpEventRevision;
	}

	bool TryWallKick(const Vector3& inputDirection, Vector3& velocity) {
		if(lastWallNormal.length() <= 0.0001f) return false;
		const bool sameWall = lastWallKickNormal.length() > 0.0001f &&
			lastWallKickNormal.dot(lastWallNormal) > 0.92f;
		if(sameWall && sameWallBlockTimer > 0.0f) return false;

		Vector3 kick = lastWallNormal * settings.wallKickHorizontalVelocity;
		if(inputDirection.length() > 0.0001f) kick += inputDirection * settings.wallKickInputInfluence;
		const float awaySpeed = kick.dot(lastWallNormal);
		if(awaySpeed < settings.wallKickHorizontalVelocity * 0.65f) {
			kick += lastWallNormal * (settings.wallKickHorizontalVelocity * 0.65f - awaySpeed);
		}

		velocity.x = kick.x;
		velocity.z = kick.z;
		verticalVelocity = settings.wallKickVerticalVelocity;
		lastWallKickNormal = lastWallNormal;
		wallGraceTimer = 0.0f;
		sameWallBlockTimer = settings.sameWallBlockTime;
		controlLockTimer = settings.wallKickControlLock;
		groundDetachTimer = 0.08f;
		grounded = false;
		jumpStage = 0;
		canCutJump = true;
		++wallKickEventRevision;
		return true;
	}

	void UpdateHorizontalVelocity(const Vector3& inputDirection, bool hasInput, Vector3& velocity, float dt) {
		Vector3 current(velocity.x, 0.0f, velocity.z);
		Vector3 target = hasInput ? inputDirection * settings.maxGroundSpeed : Vector3{};
		float acceleration = grounded
			? (hasInput ? settings.groundAcceleration : settings.groundDeceleration)
			: settings.airAcceleration;
		if(!grounded && hasInput && current.length() > 0.0001f && current.normalize().dot(inputDirection) < 0.0f) {
			acceleration = settings.airTurnAcceleration;
		}
		const Vector3 changed = MoveTowards(current, target, acceleration * dt);
		velocity.x = changed.x;
		velocity.z = changed.z;
		horizontalSpeed = changed.length();
	}

	void ApplyGravity(float dt) {
		float gravity = verticalVelocity > 0.0f ? settings.gravityAscending : settings.gravityDescending;
		if(std::abs(verticalVelocity) < settings.apexVelocityThreshold) gravity *= settings.apexGravityScale;
		verticalVelocity -= gravity * dt;
	}

	void RotateToward(TransformComponent& t, const Vector3& direction, float dt) {
		const float targetYaw = std::atan2(direction.x, direction.z);
		const DirectX::XMVECTOR targetQ = DirectX::XMQuaternionRotationRollPitchYaw(0.0f, targetYaw, 0.0f);
		const float blend = 1.0f - std::exp(-settings.rotationSpeed * dt);
		const DirectX::XMVECTOR result = DirectX::XMQuaternionSlerp(t.rotationVector(), targetQ, blend);
		DirectX::XMFLOAT4 q;
		DirectX::XMStoreFloat4(&q, result);
		t.SetRotation(q);
	}

	void BeginFallRecovery() {
		if(respawning || cleared) return;
		respawning = true;
		controlEnabled = false;
		fallRecoveryTimer = settings.fallRecoveryDelay;
		jumpBufferTimer = 0.0f;
		jumpStage = 0;
		StopRigidbodyMotion();
	}

	void FinishFallRecovery() {
		auto* t = transform.TryGet();
		if(!t) return;
		t->position = checkpointPosition;
		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				rigid->setGlobalPose(physx::PxTransform(checkpointPosition.x, checkpointPosition.y, checkpointPosition.z));
				rigid->setLinearVelocity(physx::PxVec3(0.0f));
				rigid->setAngularVelocity(physx::PxVec3(0.0f));
				rigid->wakeUp();
			}
		}
		verticalVelocity = 0.0f;
		respawning = false;
		controlEnabled = true;
		controlLockTimer = settings.respawnControlLock;
		invulnerabilityTimer = settings.respawnInvulnerability;
		groundDetachTimer = 0.0f;
		++respawnEventRevision;
	}

	void StopRigidbodyMotion() {
		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				rigid->setLinearVelocity(physx::PxVec3(0.0f));
				rigid->setAngularVelocity(physx::PxVec3(0.0f));
			}
		}
		verticalVelocity = 0.0f;
	}

	static Vector3 ProjectOnPlane(const Vector3& value, const Vector3& normal) {
		return value - normal * value.dot(normal);
	}

	static float MoveTowards(float current, float target, float maxDelta) {
		const float difference = target - current;
		if(std::abs(difference) <= maxDelta) return target;
		return current + (difference > 0.0f ? maxDelta : -maxDelta);
	}

	static Vector3 MoveTowards(const Vector3& current, const Vector3& target, float maxDelta) {
		const Vector3 delta = target - current;
		const float distance = delta.length();
		if(distance <= maxDelta || distance <= 0.0001f) return target;
		return current + delta * (maxDelta / distance);
	}

	PlatformerMovementSettings settings;
	ComponentRef<TransformComponent> transform;
	ComponentRef<ColliderComponent> collider;
	ComponentRef<TransformComponent> cameraTransform;

	uint32_t selfLayerBit = 1u << 1;
	Vector3 checkpointPosition;
	Vector3 groundNormal = Vector3(0.0f, 1.0f, 0.0f);
	Vector3 lastWallNormal;
	Vector3 lastWallKickNormal;
	Vector3 lastJumpDirection;

	float verticalVelocity = 0.0f;
	float horizontalSpeed = 0.0f;
	float jumpBufferTimer = 0.0f;
	float coyoteTimer = 0.0f;
	float wallGraceTimer = 0.0f;
	float tripleChainTimer = 0.0f;
	float sameWallBlockTimer = 0.0f;
	float controlLockTimer = 0.0f;
	float invulnerabilityTimer = 0.0f;
	float groundDetachTimer = 0.0f;
	float fallRecoveryTimer = 0.0f;

	bool grounded = false;
	bool wasGrounded = false;
	bool jumpHeld = false;
	bool jumpReleaseQueued = false;
	bool canCutJump = false;
	bool controlEnabled = true;
	bool respawning = false;
	bool cleared = false;
	int jumpStage = 0;
	int health = 3;

	uint32_t jumpEventRevision = 0;
	uint32_t landEventRevision = 0;
	uint32_t wallKickEventRevision = 0;
	uint32_t stompEventRevision = 0;
	uint32_t damageEventRevision = 0;
	uint32_t respawnEventRevision = 0;
};
