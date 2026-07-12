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
		REFLECT_FIELD(float, followSharpness, 7.5f)
		REFLECT_FIELD(float, verticalFollowSharpness, 3.0f)
		REFLECT_FIELD(float, transitionSharpness, 4.8f)
		REFLECT_FIELD(float, occlusionShortenSharpness, 13.0f)
		REFLECT_FIELD(float, occlusionReturnSharpness, 4.0f)
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
		currentDistance += (distanceTarget - currentDistance) * ExpBlend(distanceSharpness, dt);
		currentDistance = (std::max)(minimumDistance, currentDistance);

		cameraPose->position = smoothedTarget + rayDirection * currentDistance;
		cameraComponent->isLock = true;
		cameraComponent->Target = smoothedTarget;
		cameraComponent->FOV = currentSettings.fov;
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
		occlusionShortenSharpness = (std::max)(0.1f, occlusionShortenSharpness);
		occlusionReturnSharpness = (std::max)(0.1f, occlusionReturnSharpness);
		occlusionEnterDelay = (std::max)(0.0f, occlusionEnterDelay);
		occlusionExitDelay = (std::max)(0.0f, occlusionExitDelay);
		occlusionProbeStart = (std::max)(0.0f, occlusionProbeStart);

		// Player, gameplay triggers, enemies, and the boss are never camera
		// occluders. Older scene saves contain mask 10, so enforce layer 4 here.
		selfLayerBit |= (1u << 1) | (1u << 3) | (1u << 4);

		// Older authored scenes stored 1.2. That distance is too close for this
		// character scale and makes a single transient ray hit visually violent.
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
		smoothedTarget.x += (desiredTarget.x - smoothedTarget.x) * horizontalBlend;
		smoothedTarget.z += (desiredTarget.z - smoothedTarget.z) * horizontalBlend;
		smoothedVertical += (desiredTarget.y - smoothedVertical) * verticalBlend;
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

	CameraSettings GetProfileSettings(Profile selected) const {
		CameraSettings result;
		switch(selected) {
		case Profile::Course:
			result = {DegreesToRadians(courseYawDegrees), courseDistance, courseHeight, 1.25f, 0.9f, 1.0f};
			break;
		case Profile::TripleJump:
			result = {DegreesToRadians(courseYawDegrees), courseDistance + 2.0f, courseHeight + 1.0f, 1.8f, 1.8f, 1.02f};
			break;
		case Profile::WallKick:
			result = {DegreesToRadians(wallYawDegrees), wallDistance, wallHeight, 2.0f, 0.0f, 0.95f};
			break;
		case Profile::Boss:
			result = {DegreesToRadians(courseYawDegrees), bossDistance, bossHeight, 2.0f, 0.0f, 1.08f};
			break;
		case Profile::Clear:
			result = {DegreesToRadians(courseYawDegrees + 25.0f), 7.0f, 5.0f, 1.8f, 0.0f, 0.9f};
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

	ComponentRef<TransformComponent> transform;
	ComponentRef<CameraComponent> camera;
	ComponentRef<PlatformerCharacterController> player;
	EntityRef bossTarget;
	Profile profile = Profile::Course;
	CameraSettings currentSettings;
	CameraSettings targetSettings;
	Vector3 smoothedTarget;
	float smoothedVertical = 0.0f;
	float currentDistance = 0.0f;
	float occlusionEnterTimer = 0.0f;
	float occlusionExitTimer = 0.0f;
	bool occlusionActive = false;
	uint32_t selfLayerBit = 1u << 1;
	uint32_t profileRevision = 0;
};