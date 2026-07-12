#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/ColliderComponent.h"
#include "Game/Platformer/PlatformerCharacterController.h"

#include <algorithm>
#include <cmath>

class PlatformerMovingPlatform : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerMovingPlatform)
		REFLECT_FIELD(Vector3, localOffset, Vector3(0.0f, 3.0f, 0.0f))
		REFLECT_FIELD(float, cycleSeconds, 3.5f)
		REFLECT_FIELD(float, phaseOffset, 0.0f)
		REFLECT_FIELD(bool, carryPlayer, true)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer Moving Platform");
		INSPECTOR_FIELDS();
	}

	void OnStart() override {
		transform = GetComponentRef<TransformComponent>();
		if(auto* t = transform.TryGet()) {
			startPosition = t->position;
			previousPosition = t->position;
		}
		elapsed = (std::max)(0.0f, cycleSeconds) * phaseOffset;
	}

	void OnFixedUpdate(float dt) override {
		auto* t = transform.TryGet();
		if(!t || cycleSeconds <= 0.001f) return;
		elapsed += (std::max)(0.0f, dt);
		const float normalized = std::fmod(elapsed, cycleSeconds) / cycleSeconds;
		const float eased = 0.5f - 0.5f * std::cos(normalized * DirectX::XM_2PI);
		const Vector3 next = startPosition + localOffset * eased;
		const Vector3 delta = next - previousPosition;
		t->position = next;
		previousPosition = next;

		if(carryPlayer && riderGrace > 0.0f && rider.IsValid()) {
			riderGrace = (std::max)(0.0f, riderGrace - dt);
			MoveRider(delta);
		} else {
			riderGrace = (std::max)(0.0f, riderGrace - dt);
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
		rider = hit.other;
		riderGrace = 0.12f;
	}

	void OnCollisionExit(const HitInfo& hit) override {
		if(rider == hit.other) riderGrace = 0.08f;
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
				rigid->setGlobalPose(physx::PxTransform(current.p + physx::PxVec3(delta.x, delta.y, delta.z), current.q));
			}
		}
	}

	ComponentRef<TransformComponent> transform;
	EntityRef rider;
	Vector3 startPosition;
	Vector3 previousPosition;
	float elapsed = 0.0f;
	float riderGrace = 0.0f;
};
