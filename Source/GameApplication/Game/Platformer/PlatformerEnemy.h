#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/ColliderComponent.h"
#include "Engine/Scene/Component/audioComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerFeedback.h"

#include <algorithm>
#include <cmath>

class PlatformerEnemy : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerEnemy)
		REFLECT_FIELD(Vector3, patrolAxis, Vector3(1.0f, 0.0f, 0.0f))
		REFLECT_FIELD(float, patrolDistance, 3.0f)
		REFLECT_FIELD(float, patrolSpeed, 1.6f)
		REFLECT_FIELD(float, stompHeightMargin, 0.35f)
		REFLECT_FIELD(float, defeatDuration, 0.42f)

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
		ImGui::Text("Platformer Enemy");
		INSPECTOR_FIELDS();
		ImGui::Text("Defeated: %s", defeated ? "true" : "false");
	}

	void OnStart() override {
		transform = GetComponentRef<TransformComponent>();
		collider = GetComponentRef<ColliderComponent>();
		particle = GetComponentRef<ParticleComponent>();
		audio = GetComponentRef<AudioComponent>();
		if(auto* t = transform.TryGet()) {
			origin = t->position;
			baseScale = t->scale;
		}
		if(patrolAxis.length() <= 0.0001f) patrolAxis = Vector3(1.0f, 0.0f, 0.0f);
		patrolAxis.y = 0.0f;
		patrolAxis = patrolAxis.normalize();
	}

	void OnFixedUpdate(float dt) override {
		if(defeated || dt <= 0.0f) return;
		auto* t = transform.TryGet();
		if(!t) return;

		const float signedDistance = (t->position - origin).dot(patrolAxis);
		if(signedDistance >= patrolDistance) direction = -1.0f;
		if(signedDistance <= -patrolDistance) direction = 1.0f;
		const Vector3 desired = patrolAxis * (patrolSpeed * direction);

		if(!collider.IsValid()) collider = GetComponentRef<ColliderComponent>();
		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				const physx::PxVec3 velocity = rigid->getLinearVelocity();
				rigid->setLinearVelocity(physx::PxVec3(desired.x, velocity.y, desired.z));
				return;
			}
		}
		t->position += desired * dt;
	}

	void OnUpdate(float dt) override {
		if(!defeated) return;
		defeatTimer += (std::max)(0.0f, dt);
		if(auto* t = transform.TryGet()) {
			const float normalized = defeatDuration > 0.0f ? std::clamp(defeatTimer / defeatDuration, 0.0f, 1.0f) : 1.0f;
			t->scale = Vector3(
				baseScale.x * (1.0f + normalized * 0.45f),
				baseScale.y * (1.0f - normalized),
				baseScale.z * (1.0f + normalized * 0.45f));
		}
		if(defeatTimer >= defeatDuration && !destroyQueued) destroyQueued = QueueDestroySelf();
	}

	void OnCollisionEnter(const HitInfo& hit) override {
		HandleContact(hit.other, true);
	}

	void OnCollisionStay(const HitInfo& hit) override {
		HandleContact(hit.other, false);
	}

private:
	void HandleContact(const EntityRef& other, bool entered) {
		if(defeated || !other.IsValid()) return;
		ComponentRef<PlatformerCharacterController> player(other);
		auto* controller = player.TryGet();
		if(!controller) {
			if(entered) direction *= -1.0f;
			return;
		}

		auto* enemyTransform = transform.TryGet();
		ComponentRef<TransformComponent> playerTransform(other);
		auto* playerPose = playerTransform.TryGet();
		if(!enemyTransform || !playerPose) return;

		const bool above = playerPose->position.y >= enemyTransform->position.y + stompHeightMargin;
		if(controller->IsDescending() && above) {
			Defeat(*controller, enemyTransform->position);
			return;
		}

		controller->ApplyDamage(enemyTransform->position);
	}

	void Defeat(PlatformerCharacterController& player, const Vector3& position) {
		if(defeated) return;
		defeated = true;
		defeatTimer = 0.0f;
		player.ApplyStompBounce();
		PlatformerFeedback::Burst(particle.TryGet(), position + Vector3(0.0f, 0.45f, 0.0f), 22, 3.3f, 4.5f, defeatDuration + 0.2f);
		PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene());

		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				rigid->setLinearVelocity(physx::PxVec3(0.0f));
				rigid->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, true);
			}
		}
	}

	ComponentRef<TransformComponent> transform;
	ComponentRef<ColliderComponent> collider;
	ComponentRef<ParticleComponent> particle;
	ComponentRef<AudioComponent> audio;
	Vector3 origin;
	Vector3 baseScale = Vector3(1.0f, 1.0f, 1.0f);
	float direction = 1.0f;
	float defeatTimer = 0.0f;
	bool defeated = false;
	bool destroyQueued = false;
};
