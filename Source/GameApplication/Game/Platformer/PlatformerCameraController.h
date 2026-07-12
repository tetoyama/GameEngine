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
		REFLECT_FIELD(float, occlusionReturnSharpness, 6.0f)
		REFLECT_FIELD(float, minimumDistance, 1.2f)
		REFLECT_FIELD(float, collisionPadding, 0.18f)
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
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer Camera Controller");
		INSPECTOR_FIELDS();
		int value = static_cast<int>(profile);
		const char* profiles[] = {"Course", "Triple Jump", "Wall Kick", "Boss", "Clear"};
		if(ImGui::Combo("Profile", &value, profiles, 5)) SetProfile(static_cast<Profile>(value));
	}

	void OnStart() override {
		transform = GetComponentRef<TransformComponent>();
		camera = GetComponentRef<CameraComponent>();
		player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		if(auto* playerPose = ComponentRef<TransformComponent>(player.GetEntityRef()).TryGet()) {
			smoothedTarget = playerPose->position + Vector3(0.0f, 1.2f, 0.0f);
			smoothedVertical = smoothedTarget.y;
		}
		currentDistance = GetProfileSettings(profile).distance;
		currentSettings = GetProfileSettings(profile);
		targetSettings = currentSettings;
	}

	void OnFixedUpdate(float dt) override {
		auto* cameraPose = transform.TryGet();
		auto* cameraComponent = camera.TryGet();
		if(!cameraPose || !cameraComponent) return;
		if(!player.IsValid()) player = PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(m_ref.GetScene());
		ComponentRef<TransformComponent> playerTransform(player.GetEntityRef());
		auto* playerPose = playerTransform.TryGet();
		if(!playerPose) return;

		targetSettings = GetProfileSettings(profile);
		const float settingsBlend = ExpBlend(transitionSharpness, dt);
		currentSettings.distance += (targetSettings.distance - currentSettings.distance) * settingsBlend;
		currentSettings.height += (targetSettings.height - currentSettings.height) * settingsBlend;
		currentSettings.lookHeight += (targetSettings.lookHeight - currentSettings.lookHeight) * settingsBlend;
		currentSettings.lookAhead += (targetSettings.lookAhead - currentSettings.lookAhead) * settingsBlend;
		currentSettings.yawRadians = LerpAngle(currentSettings.yawRadians, targetSettings.yawRadians, settingsBlend);
		currentSettings.fov += (targetSettings.fov - currentSettings.fov) * settingsBlend;

		Vector3 desiredTarget = playerPose->position;
		Vector3 forward = playerPose->front();
		forward.y = 0.0f;
		if(forward.length() > 0.0001f) forward = forward.normalize();
		desiredTarget += forward * currentSettings.lookAhead;
		desiredTarget.y += currentSettings.lookHeight;

		if(profile == Profile::Boss && bossTarget.IsValid()) {
			ComponentRef<TransformComponent> bossTransform(bossTarget);
			if(auto* bossPose = bossTransform.TryGet()) {
				const Vector3 midpoint = (playerPose->position + bossPose->position) * 0.5f;
				desiredTarget.x = midpoint.x;
				desiredTarget.z = midpoint.z;
				desiredTarget.y = (std::max)(playerPose->position.y, bossPose->position.y) + currentSettings.lookHeight;
			}
		}

		const float horizontalBlend = ExpBlend(followSharpness, dt);
		const float verticalBlend = ExpBlend(verticalFollowSharpness, dt);
		smoothedTarget.x += (desiredTarget.x - smoothedTarget.x) * horizontalBlend;
		smoothedTarget.z += (desiredTarget.z - smoothedTarget.z) * horizontalBlend;
		smoothedVertical += (desiredTarget.y - smoothedVertical) * verticalBlend;
		smoothedTarget.y = smoothedVertical;

		const Vector3 offset(
			std::sin(currentSettings.yawRadians) * currentSettings.distance,
			currentSettings.height,
			-std::cos(currentSettings.yawRadians) * currentSettings.distance);
		const Vector3 desiredCamera = smoothedTarget + offset;
		Vector3 ray = desiredCamera - smoothedTarget;
		float desiredDistance = ray.length();
		Vector3 rayDirection = desiredDistance > 0.0001f ? ray / desiredDistance : Vector3(0.0f, 0.0f, -1.0f);
		float unoccludedDistance = desiredDistance;

		if(auto* physics = PlatformerSceneAccess::Physics(m_ref.GetScene())) {
			const RayHit hit = physics->RaycastWithMask(
				physx::PxVec3(smoothedTarget.x, smoothedTarget.y, smoothedTarget.z),
				physx::PxVec3(rayDirection.x, rayDirection.y, rayDirection.z),
				desiredDistance,
				selfLayerBit);
			if(hit.hit) unoccludedDistance = (std::max)(minimumDistance, hit.distance - collisionPadding);
		}

		if(currentDistance <= 0.0f) currentDistance = unoccludedDistance;
		if(unoccludedDistance < currentDistance) currentDistance = unoccludedDistance;
		else currentDistance += (unoccludedDistance - currentDistance) * ExpBlend(occlusionReturnSharpness, dt);
		currentDistance = std::clamp(currentDistance, minimumDistance, (std::max)(minimumDistance, desiredDistance));

		cameraPose->position = smoothedTarget + rayDirection * currentDistance;
		cameraComponent->isLock = true;
		cameraComponent->Target = smoothedTarget;
		cameraComponent->FOV = currentSettings.fov;
		ApplyLookRotation(*cameraPose, smoothedTarget);
	}

	void SetProfile(Profile next) {
		if(profile == next) return;
		profile = next;
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
	uint32_t selfLayerBit = 1u << 1;
	uint32_t profileRevision = 0;
};
