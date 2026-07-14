#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/ColliderComponent.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerFeedback.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <cmath>

class PlatformerMovingPlatform : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerMovingPlatform)
		REFLECT_FIELD(Vector3, localOffset, Vector3(0.0f, 3.0f, 0.0f))
		REFLECT_FIELD(float, cycleSeconds, 3.5f)
		REFLECT_FIELD(float, phaseOffset, 0.0f)
		REFLECT_FIELD(bool, carryPlayer, true)
		REFLECT_FIELD(bool, interactionFeedbackEnabled, true)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		cycleSeconds = (std::max)(0.05f, cycleSeconds);
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer Moving Platform");
		INSPECTOR_FIELDS();
		ImGui::Text("Rider: %s", rider.IsValid() ? "attached" : "none");
	}

	void OnStart() override {
		transform = GetComponentRef<TransformComponent>();
		camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		if(auto* t = transform.TryGet()) {
			startPosition = t->position;
			previousPosition = t->position;
		}
		elapsed = (std::max)(0.0f, cycleSeconds) * phaseOffset;
	}

	void OnFixedUpdate(float dt) override {
		auto* t = transform.TryGet();
		if(!t || cycleSeconds <= 0.001f) return;
		const float safeDt = (std::max)(0.0f, dt);
		elapsed += safeDt;
		endpointFeedbackCooldown = (std::max)(0.0f, endpointFeedbackCooldown - safeDt);

		const float normalized = std::fmod(elapsed, cycleSeconds) / cycleSeconds;
		const float eased = 0.5f - 0.5f * std::cos(normalized * DirectX::XM_2PI);
		const Vector3 next = startPosition + localOffset * eased;
		const Vector3 delta = next - previousPosition;
		t->position = next;
		previousPosition = next;

		const float alongMotion = delta.dot(localOffset);
		const int travelSign = alongMotion > 0.00001f ? 1 : (alongMotion < -0.00001f ? -1 : 0);
		if(interactionFeedbackEnabled && travelSign != 0 && previousTravelSign != 0 &&
		   travelSign != previousTravelSign && endpointFeedbackCooldown <= 0.0f) {
			EmitEndpointFeedback(*t, travelSign);
			endpointFeedbackCooldown = 0.22f;
		}
		if(travelSign != 0) previousTravelSign = travelSign;

		if(carryPlayer && riderGrace > 0.0f && rider.IsValid()) {
			riderGrace = (std::max)(0.0f, riderGrace - safeDt);
			MoveRider(delta);
		} else {
			riderGrace = (std::max)(0.0f, riderGrace - safeDt);
			if(riderGrace <= 0.0f) rider = {};
		}
	}

	void OnCollisionStay(const HitInfo& hit) override {
		if(!carryPlayer || !hit.other.IsValid()) return;
		ComponentRef<PlatformerCharacterController> controller(hit.other);
		ComponentRef<TransformComponent> playerTransform(hit.other);
		auto* platformPose = transform.TryGet();
		auto* playerPose = playerTransform.TryGet();
		if(!controller.IsValid() || !platformPose || !playerPose) return;
		if(playerPose->position.y + 0.05f < platformPose->position.y) return;

		const bool newRider = !rider.IsValid() || !(rider == hit.other) || riderGrace <= 0.0f;
		rider = hit.other;
		riderGrace = 0.12f;
		if(newRider && interactionFeedbackEnabled) EmitMountFeedback(*playerPose);
	}

	void OnCollisionExit(const HitInfo& hit) override {
		if(!(rider == hit.other)) return;
		riderGrace = 0.08f;
	}

private:
	void MoveRider(const Vector3& delta) {
		ComponentRef<TransformComponent> riderTransform(rider);
		ComponentRef<ColliderComponent> riderCollider(rider);
		auto* pose = riderTransform.TryGet();
		if(!pose) return;
		pose->position += delta;

		if(auto* col = riderCollider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				const physx::PxTransform current = rigid->getGlobalPose();
				rigid->setGlobalPose(physx::PxTransform(
					current.p + physx::PxVec3(delta.x, delta.y, delta.z), current.q));
			}
		}
	}

	void EmitMountFeedback(const TransformComponent& playerPose) {
		PlatformerFeedback::SharedDirectionalBurst(
			m_ref.GetScene(),
			playerPose.position + Vector3(0.0f, 0.05f, 0.0f),
			Vector3(0.0f, 1.0f, 0.0f),
			42,
			2.8f,
			2.6f,
			2.2f,
			0.52f,
			0.095f,
			DirectX::XMFLOAT4(0.30f, 0.92f, 1.0f, 1.0f),
			DirectX::XMFLOAT4(0.48f, 0.32f, 1.0f, 1.0f));
		Impulse(0.075f, 0.12f, 0.008f, Vector3(0.0f, 1.0f, 0.0f));
	}

	void EmitEndpointFeedback(const TransformComponent& platformPose, int travelSign) {
		Vector3 direction = localOffset;
		if(direction.length() <= 0.0001f) direction = Vector3(0.0f, 1.0f, 0.0f);
		else direction = direction.normalize() * static_cast<float>(-travelSign);
		PlatformerFeedback::SharedDirectionalBurst(
			m_ref.GetScene(),
			platformPose.position,
			direction,
			34,
			2.4f,
			2.2f,
			1.8f,
			0.48f,
			0.08f,
			DirectX::XMFLOAT4(0.18f, 0.72f, 1.0f, 1.0f),
			DirectX::XMFLOAT4(0.62f, 0.22f, 1.0f, 1.0f));
	}

	void Impulse(float strength, float duration, float fovKick, const Vector3& direction) {
		if(!camera.IsValid()) camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		if(auto* controller = camera.TryGet()) {
			controller->AddImpulse(strength, duration, fovKick, direction);
		}
	}

	ComponentRef<TransformComponent> transform;
	ComponentRef<PlatformerCameraController> camera;
	EntityRef rider;
	Vector3 startPosition;
	Vector3 previousPosition;
	float elapsed = 0.0f;
	float riderGrace = 0.0f;
	float endpointFeedbackCooldown = 0.0f;
	int previousTravelSign = 0;
};