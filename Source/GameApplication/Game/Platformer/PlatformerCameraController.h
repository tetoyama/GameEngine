#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/CameraComponent.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <cmath>

class PlatformerCameraController : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerCameraController)
		REFLECT_FIELD(float, followSharpness, 6.0f)
		REFLECT_FIELD(float, verticalFollowSharpness, 3.0f)
		REFLECT_FIELD(float, transitionSharpness, 2.2f)
		REFLECT_FIELD(float, targetHorizontalMaxSpeed, 11.0f)
		REFLECT_FIELD(float, targetVerticalMaxSpeed, 5.5f)
		REFLECT_FIELD(float, occlusionShortenSharpness, 8.0f)
		REFLECT_FIELD(float, occlusionReturnSharpness, 3.5f)
		REFLECT_FIELD(float, occlusionShortenMaxSpeed, 8.0f)
		REFLECT_FIELD(float, occlusionReturnMaxSpeed, 4.0f)
		REFLECT_FIELD(float, occlusionEnterDelay, 0.08f)
		REFLECT_FIELD(float, occlusionExitDelay, 0.12f)
		REFLECT_FIELD(float, occlusionProbeStart, 0.45f)
		REFLECT_FIELD(float, minimumDistance, 2.5f)
		REFLECT_FIELD(float, collisionPadding, 0.24f)
		REFLECT_FIELD(float, courseYawDegrees, 0.0f)
		REFLECT_FIELD(float, courseDistance, 8.5f)
		REFLECT_FIELD(float, courseHeight, 3.8f)
		REFLECT_FIELD(float, wallYawDegrees, 90.0f)
		REFLECT_FIELD(float, wallDistance, 11.5f)
		REFLECT_FIELD(float, wallHeight, 4.5f)
		REFLECT_FIELD(float, bossDistance, 13.5f)
		REFLECT_FIELD(float, bossHeight, 7.0f)
		REFLECT_FIELD(bool, comfortCamera, true)
		REFLECT_FIELD(float, impulsePositionScale, 0.075f)
		REFLECT_FIELD(float, impulseFrequency, 7.0f)
		REFLECT_FIELD(float, impulseResponseSharpness, 10.0f)
		REFLECT_FIELD(float, maxImpulseFovKick, 0.010f)

public:
	enum class Profile : int {
		Course = 0,
		TripleJump,
		WallKick,
		Boss,
		Clear
	};

	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		node["SelfLayerBit"] = selfLayerBit;
		node["Profile"] = static_cast<int>(profile);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		if(node["SelfLayerBit"]) selfLayerBit = node["SelfLayerBit"].as<uint32_t>();
		if(node["Profile"]) profile = static_cast<Profile>(std::clamp(node["Profile"].as<int>(), 0, 4));
		ValidateSettings();
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer Camera Controller");
		INSPECTOR_FIELDS();
		int value = static_cast<int>(profile);
		const char* profiles[] = {"Course", "Triple Jump", "Wall Kick", "Boss", "Clear"};
		if(ImGui::Combo("Profile", &value, profiles, 5)) SetProfile(static_cast<Profile>(value));
		ImGui::Text("Distance: %.2f", currentDistance);
		ImGui::Text("Occluded: %s", occlusionActive ? "true" : "false");
		ImGui::Text("Impulse: %.2f", impulseStrength * ImpulseEnvelope());
	}

	void OnStart() override {
		ValidateSettings();
		transform = GetComponentRef<TransformComponent>();
		camera = GetComponentRef<CameraComponent>();
		player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		if(auto* playerPose = ComponentRef<TransformComponent>(player.GetEntityRef()).TryGet()) {
			smoothedTarget = playerPose->position + Vector3(0.0f, 1.2f, 0.0f);
			smoothedVertical = smoothedTarget.y;
		}

		currentSettings = GetProfileSettings(profile);
		targetSettings = currentSettings;
		currentDistance = OffsetLength(currentSettings);
		occlusionEnterTimer = 0.0f;
		occlusionExitTimer = 0.0f;
		occlusionActive = false;
		impulseTime = 0.0f;
		impulseDuration = 0.0f;
		impulseStrength = 0.0f;
		impulseFovKick = 0.0f;
		impulseDirection = Vector3();
		filteredImpulseOffset = Vector3();
		filteredImpulseFov = 0.0f;
	}

	void OnFixedUpdate(float dt) override {
		if(dt <= 0.0f) return;
		ValidateSettings();

		auto* cameraPose = transform.TryGet();
		auto* cameraComponent = camera.TryGet();
		if(!cameraPose || !cameraComponent) return;

		if(!player.IsValid()) player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		ComponentRef<TransformComponent> playerTransform(player.GetEntityRef());
		auto* playerPose = playerTransform.TryGet();
		if(!playerPose) return;

		UpdateProfileSettings(dt);
		UpdateTarget(*playerPose, dt);

		const Vector3 offset(
			std::sin(currentSettings.yawRadians) * currentSettings.distance,
			currentSettings.height,
			-std::cos(currentSettings.yawRadians) * currentSettings.distance);
		const Vector3 desiredCamera = smoothedTarget + offset;
		const Vector3 desiredRay = desiredCamera - smoothedTarget;
		const float desiredDistance = desiredRay.length();
		const Vector3 rayDirection = desiredDistance > 0.0001f
			? desiredRay / desiredDistance
			: Vector3(0.0f, 0.0f, -1.0f);

		float obstructionDistance = desiredDistance;
		const bool obstructed = ProbeOcclusion(rayDirection, desiredDistance, obstructionDistance);
		const float distanceTarget = ResolveDistanceTarget(obstructed, obstructionDistance, desiredDistance, dt);

		if(currentDistance <= 0.0f) currentDistance = desiredDistance;
		const bool shortening = distanceTarget < currentDistance;
		const float distanceSharpness = shortening ? occlusionShortenSharpness : occlusionReturnSharpness;
		const float distanceMaxSpeed = shortening ? occlusionShortenMaxSpeed : occlusionReturnMaxSpeed;
		const float desiredStep = (distanceTarget - currentDistance) * ExpBlend(distanceSharpness, dt);
		const float limitedStep = std::clamp(desiredStep, -distanceMaxSpeed * dt, distanceMaxSpeed * dt);
		currentDistance += limitedStep;
		currentDistance = (std::max)(minimumDistance, currentDistance);

		Vector3 cameraPosition = smoothedTarget + rayDirection * currentDistance;
		float fov = currentSettings.fov;
		ApplyImpulse(dt, cameraPosition, fov);

		cameraPose->position = cameraPosition;
		cameraComponent->isLock = true;
		cameraComponent->Target = smoothedTarget;
		cameraComponent->FOV = fov;
		ApplyLookRotation(*cameraPose, smoothedTarget);
	}

	void SetProfile(Profile next) {
		if(profile == next) return;
		profile = next;
		occlusionEnterTimer = 0.0f;
		occlusionExitTimer = 0.0f;
		occlusionActive = false;
		++profileRevision;
	}

	Profile GetProfile() const { return profile; }
	uint32_t GetProfileRevision() const { return profileRevision; }
	void SetBossTarget(const EntityRef& entity) { bossTarget = entity; }
	void ClearBossTarget() { bossTarget = {}; }

	void AddImpulse(
		float strength,
		float duration,
		float fovKick = 0.0f,
		const Vector3& direction = Vector3()
	) {
		const float maximumStrength = comfortCamera ? 0.72f : 1.5f;
		strength = std::clamp(strength, 0.0f, maximumStrength);
		duration = std::clamp(duration, 0.06f, comfortCamera ? 0.24f : 0.5f);
		if(strength <= 0.0f) return;

		const float retained = impulseStrength * ImpulseEnvelope();
		impulseStrength = (std::min)(maximumStrength,
			retained + strength * (1.0f - (std::min)(retained, 1.0f) * 0.45f));
		impulseDuration = (std::max)(duration, impulseTime);
		impulseTime = impulseDuration;

		const float limitedFovKick = std::clamp(fovKick, -maxImpulseFovKick, maxImpulseFovKick);
		if(std::abs(limitedFovKick) >= std::abs(impulseFovKick * ImpulseEnvelope())) {
			impulseFovKick = limitedFovKick;
		}

		if(direction.length() > 0.0001f) {
			const Vector3 normalized = direction.normalize();
			const Vector3 combined = impulseDirection + normalized * strength;
			impulseDirection = combined.length() > 0.0001f ? combined.normalize() : normalized;
		}
		impulsePhase += 0.23f;
	}

private:
	struct CameraSettings {
		float yawRadians = 0.0f;
		float distance = 8.5f;
		float height = 3.8f;
		float lookHeight = 1.2f;
		float lookAhead = 0.8f;
		float fov = 1.0f;
	};

	void ValidateSettings() {
		followSharpness = (std::max)(0.1f, followSharpness);
		verticalFollowSharpness = (std::max)(0.1f, verticalFollowSharpness);
		transitionSharpness = (std::max)(0.1f, transitionSharpness);
		targetHorizontalMaxSpeed = (std::max)(0.1f, targetHorizontalMaxSpeed);
		targetVerticalMaxSpeed = (std::max)(0.1f, targetVerticalMaxSpeed);
		occlusionShortenSharpness = (std::max)(0.1f, occlusionShortenSharpness);
		occlusionReturnSharpness = (std::max)(0.1f, occlusionReturnSharpness);
		occlusionShortenMaxSpeed = (std::max)(0.1f, occlusionShortenMaxSpeed);
		occlusionReturnMaxSpeed = (std::max)(0.1f, occlusionReturnMaxSpeed);
		occlusionEnterDelay = (std::max)(0.0f, occlusionEnterDelay);
		occlusionExitDelay = (std::max)(0.0f, occlusionExitDelay);
		occlusionProbeStart = (std::max)(0.0f, occlusionProbeStart);
		impulsePositionScale = (std::max)(0.0f, impulsePositionScale);
		impulseFrequency = (std::max)(1.0f, impulseFrequency);
		impulseResponseSharpness = (std::max)(0.1f, impulseResponseSharpness);
		maxImpulseFovKick = (std::max)(0.0f, maxImpulseFovKick);

		if(comfortCamera) {
			// Older scene data stores the original aggressive values. Clamp them here
			// so opening an existing scene also receives the comfort-safe behavior.
			followSharpness = (std::min)(followSharpness, 6.0f);
			transitionSharpness = (std::min)(transitionSharpness, 2.2f);
			occlusionShortenSharpness = (std::min)(occlusionShortenSharpness, 8.0f);
			occlusionReturnSharpness = (std::min)(occlusionReturnSharpness, 3.5f);
			impulsePositionScale = (std::min)(impulsePositionScale, 0.075f);
			impulseFrequency = std::clamp(impulseFrequency, 4.0f, 8.0f);
			maxImpulseFovKick = (std::min)(maxImpulseFovKick, 0.010f);
		}

		// Player, gameplay triggers, enemies, and the boss are never camera
		// occluders. Older scene saves contain mask 10, so enforce layer 4 here.
		selfLayerBit |= (1u << 1) | (1u << 3) | (1u << 4);

		minimumDistance = (std::max)(2.5f, minimumDistance);
		collisionPadding = (std::max)(0.05f, collisionPadding);
	}

	void UpdateProfileSettings(float dt) {
		targetSettings = GetProfileSettings(profile);
		const float blend = ExpBlend(transitionSharpness, dt);
		currentSettings.distance += (targetSettings.distance - currentSettings.distance) * blend;
		currentSettings.height += (targetSettings.height - currentSettings.height) * blend;
		currentSettings.lookHeight += (targetSettings.lookHeight - currentSettings.lookHeight) * blend;
		currentSettings.lookAhead += (targetSettings.lookAhead - currentSettings.lookAhead) * blend;
		currentSettings.yawRadians = LerpAngle(currentSettings.yawRadians, targetSettings.yawRadians, blend);
		currentSettings.fov += (targetSettings.fov - currentSettings.fov) * blend;
	}

	void UpdateTarget(const TransformComponent& playerPose, float dt) {
		Vector3 desiredTarget = playerPose.position;
		Vector3 forward = playerPose.front();
		forward.y = 0.0f;
		if(forward.length() > 0.0001f) forward = forward.normalize();
		desiredTarget += forward * currentSettings.lookAhead;
		desiredTarget.y += currentSettings.lookHeight;

		if(profile == Profile::Boss && bossTarget.IsValid()) {
			ComponentRef<TransformComponent> bossTransform(bossTarget);
			if(auto* bossPose = bossTransform.TryGet()) {
				const Vector3 midpoint = (playerPose.position + bossPose->position) * 0.5f;
				desiredTarget.x = midpoint.x;
				desiredTarget.z = midpoint.z;
				desiredTarget.y = (std::max)(playerPose.position.y, bossPose->position.y) + currentSettings.lookHeight;
			}
		}

		const float horizontalBlend = ExpBlend(followSharpness, dt);
		const float verticalBlend = ExpBlend(verticalFollowSharpness, dt);
		const float desiredX = smoothedTarget.x + (desiredTarget.x - smoothedTarget.x) * horizontalBlend;
		const float desiredZ = smoothedTarget.z + (desiredTarget.z - smoothedTarget.z) * horizontalBlend;
		const float desiredY = smoothedVertical + (desiredTarget.y - smoothedVertical) * verticalBlend;

		smoothedTarget.x = MoveTowards(smoothedTarget.x, desiredX, targetHorizontalMaxSpeed * dt);
		smoothedTarget.z = MoveTowards(smoothedTarget.z, desiredZ, targetHorizontalMaxSpeed * dt);
		smoothedVertical = MoveTowards(smoothedVertical, desiredY, targetVerticalMaxSpeed * dt);
		smoothedTarget.y = smoothedVertical;
	}

	bool ProbeOcclusion(
		const Vector3& rayDirection,
		float desiredDistance,
		float& obstructionDistance
	) const {
		auto* physics = PlatformerSceneAccess::Physics(m_ref.GetScene());
		if(!physics || desiredDistance <= minimumDistance) return false;

		const float startDistance = (std::min)(occlusionProbeStart, desiredDistance * 0.25f);
		const Vector3 origin = smoothedTarget + rayDirection * startDistance;
		const float probeLength = desiredDistance - startDistance;
		if(probeLength <= 0.001f) return false;

		const RayHit hit = physics->RaycastWithMask(
			physx::PxVec3(origin.x, origin.y, origin.z),
			physx::PxVec3(rayDirection.x, rayDirection.y, rayDirection.z),
			probeLength,
			selfLayerBit);
		if(!hit.hit || hit.distance <= 0.02f) return false;

		obstructionDistance = (std::max)(
			minimumDistance,
			startDistance + hit.distance - collisionPadding);
		return obstructionDistance < desiredDistance - 0.15f;
	}

	float ResolveDistanceTarget(
		bool obstructed,
		float obstructionDistance,
		float desiredDistance,
		float dt
	) {
		if(obstructed) {
			occlusionExitTimer = 0.0f;
			occlusionEnterTimer += dt;
			if(occlusionEnterTimer >= occlusionEnterDelay) occlusionActive = true;
			return occlusionActive ? obstructionDistance : (std::min)(currentDistance, desiredDistance);
		}

		occlusionEnterTimer = 0.0f;
		if(occlusionActive) {
			occlusionExitTimer += dt;
			if(occlusionExitTimer < occlusionExitDelay) return currentDistance;
		}

		occlusionActive = false;
		occlusionExitTimer = 0.0f;
		return desiredDistance;
	}

	void ApplyImpulse(float dt, Vector3& cameraPosition, float& fov) {
		Vector3 desiredOffset;
		float desiredFov = 0.0f;

		if(impulseTime > 0.0f && impulseStrength > 0.0f) {
			const float envelope = ImpulseEnvelope();
			impulsePhase += dt * impulseFrequency * DirectX::XM_2PI;

			// Use a slow, coherent movement rather than independent high-frequency
			// noise on position, target, roll, and FOV. The stable look target is the
			// most important part of keeping the camera comfortable.
			const Vector3 wave(
				std::sin(impulsePhase),
				std::sin(impulsePhase * 0.73f + 1.2f) * 0.45f,
				std::sin(impulsePhase * 0.51f + 2.1f) * 0.30f);
			const float positionAmount = impulsePositionScale * impulseStrength * envelope;
			desiredOffset = wave * positionAmount;
			if(impulseDirection.length() > 0.0001f) {
				desiredOffset += impulseDirection * (positionAmount * 0.35f);
			}
			desiredFov = impulseFovKick * envelope;

			impulseTime = (std::max)(0.0f, impulseTime - dt);
			if(impulseTime <= 0.0f) {
				impulseDuration = 0.0f;
				impulseStrength = 0.0f;
				impulseFovKick = 0.0f;
				impulseDirection = Vector3();
			}
		}

		const float response = ExpBlend(impulseResponseSharpness, dt);
		filteredImpulseOffset = Vec3Lerp(filteredImpulseOffset, desiredOffset, response);
		filteredImpulseFov += (desiredFov - filteredImpulseFov) * response;
		if(filteredImpulseOffset.length() < 0.0001f) filteredImpulseOffset = Vector3();
		if(std::abs(filteredImpulseFov) < 0.00001f) filteredImpulseFov = 0.0f;

		cameraPosition += filteredImpulseOffset;
		fov += filteredImpulseFov;
	}

	float ImpulseEnvelope() const {
		if(impulseDuration <= 0.0001f || impulseTime <= 0.0f) return 0.0f;
		const float normalized = std::clamp(impulseTime / impulseDuration, 0.0f, 1.0f);
		return normalized * normalized;
	}

	CameraSettings GetProfileSettings(Profile selected) const {
		CameraSettings result;
		switch(selected) {
		case Profile::Course:
			result = {DegreesToRadians(courseYawDegrees), courseDistance, courseHeight, 1.25f, 0.9f, 1.0f};
			break;
		case Profile::TripleJump:
			result = {DegreesToRadians(courseYawDegrees), courseDistance + 2.0f, courseHeight + 1.0f, 1.8f, 1.8f, 1.01f};
			break;
		case Profile::WallKick:
			result = {DegreesToRadians(wallYawDegrees), wallDistance, wallHeight, 2.0f, 0.0f, 0.98f};
			break;
		case Profile::Boss:
			result = {DegreesToRadians(courseYawDegrees), bossDistance, bossHeight, 2.0f, 0.0f, 1.04f};
			break;
		case Profile::Clear:
			result = {DegreesToRadians(courseYawDegrees + 25.0f), 7.0f, 5.0f, 1.8f, 0.0f, 0.96f};
			break;
		}
		return result;
	}

	static float OffsetLength(const CameraSettings& settings) {
		return std::sqrt(settings.distance * settings.distance + settings.height * settings.height);
	}

	static void ApplyLookRotation(TransformComponent& transform, const Vector3& target) {
		const Vector3 forward = (target - transform.position).normalize();
		if(forward.length() <= 0.0001f) return;
		const float yaw = std::atan2(forward.x, forward.z);
		const float pitch = std::asin(std::clamp(-forward.y, -1.0f, 1.0f));
		DirectX::XMFLOAT4 rotation;
		DirectX::XMStoreFloat4(&rotation, DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, 0.0f));
		transform.SetRotation(rotation);
	}

	static float ExpBlend(float sharpness, float dt) {
		return 1.0f - std::exp(-(std::max)(0.0f, sharpness) * (std::max)(0.0f, dt));
	}

	static float DegreesToRadians(float degrees) {
		return degrees * DirectX::XM_PI / 180.0f;
	}

	static float LerpAngle(float current, float target, float t) {
		float delta = std::fmod(target - current + DirectX::XM_PI, DirectX::XM_2PI);
		if(delta < 0.0f) delta += DirectX::XM_2PI;
		delta -= DirectX::XM_PI;
		return current + delta * std::clamp(t, 0.0f, 1.0f);
	}

	static float MoveTowards(float current, float target, float maxDelta) {
		if(std::abs(target - current) <= maxDelta) return target;
		return current + (target > current ? maxDelta : -maxDelta);
	}

	ComponentRef<TransformComponent> transform;
	ComponentRef<CameraComponent> camera;
	ComponentRef<PlatformerCharacterController> player;
	EntityRef bossTarget;
	Profile profile = Profile::Course;
	CameraSettings currentSettings;
	CameraSettings targetSettings;
	Vector3 smoothedTarget;
	Vector3 impulseDirection;
	Vector3 filteredImpulseOffset;
	float smoothedVertical = 0.0f;
	float currentDistance = 0.0f;
	float occlusionEnterTimer = 0.0f;
	float occlusionExitTimer = 0.0f;
	float impulseTime = 0.0f;
	float impulseDuration = 0.0f;
	float impulseStrength = 0.0f;
	float impulseFovKick = 0.0f;
	float impulsePhase = 0.0f;
	float filteredImpulseFov = 0.0f;
	bool occlusionActive = false;
	uint32_t selfLayerBit = 1u << 1;
	uint32_t profileRevision = 0;
};
