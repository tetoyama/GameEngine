#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/ColliderComponent.h"
#include "Engine/Scene/Component/materialComponent.h"
#include "Engine/Scene/Component/audioComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerCharacterController.h"
#include "Game/Platformer/PlatformerFeedback.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <cmath>

class PlatformerEnemy : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerEnemy)
		REFLECT_FIELD(Vector3, patrolAxis, Vector3(1.0f, 0.0f, 0.0f))
		REFLECT_FIELD(float, patrolDistance, 3.0f)
		REFLECT_FIELD(float, patrolSpeed, 1.6f)
		REFLECT_FIELD(float, stompHeightMargin, 0.12f)
		REFLECT_FIELD(float, defeatDuration, 0.42f)
		REFLECT_FIELD(bool, turnDustEnabled, true)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		// Existing scenes stored 0.35, which required the player's feet to be too
		// close to the exact top. Keep a broad beginner-friendly upper-contact band.
		stompHeightMargin = std::clamp(stompHeightMargin, 0.08f, 0.18f);
		defeatDuration = (std::max)(0.20f, defeatDuration);
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
		material = GetComponentRef<MaterialComponent>();
		particle = GetComponentRef<ParticleComponent>();
		audio = GetComponentRef<AudioComponent>();
		camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());

		AlignColliderToModelCenter();

		if(auto* t = transform.TryGet()) {
			origin = t->position;
			baseScale = t->scale;
		}
		if(auto* mat = material.TryGet()) baseMaterial = mat->Material;
		if(patrolAxis.length() <= 0.0001f) patrolAxis = Vector3(1.0f, 0.0f, 0.0f);
		patrolAxis.y = 0.0f;
		patrolAxis = patrolAxis.normalize();
	}

	void OnFixedUpdate(float dt) override {
		if(defeated || dt <= 0.0f) return;
		auto* t = transform.TryGet();
		if(!t) return;
		turnDustCooldown = (std::max)(0.0f, turnDustCooldown - dt);

		const float signedDistance = (t->position - origin).dot(patrolAxis);
		const float previousDirection = direction;
		if(signedDistance >= patrolDistance) direction = -1.0f;
		if(signedDistance <= -patrolDistance) direction = 1.0f;
		if(previousDirection != direction && turnDustEnabled && turnDustCooldown <= 0.0f) {
			EmitTurnDust();
			turnDustCooldown = 0.18f;
		}

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
		const float normalized = defeatDuration > 0.0f
			? std::clamp(defeatTimer / defeatDuration, 0.0f, 1.0f)
			: 1.0f;
		const float impactPhase = std::clamp(normalized / 0.32f, 0.0f, 1.0f);
		const float impactPop = std::sin(impactPhase * DirectX::XM_PI);

		if(auto* t = transform.TryGet()) {
			t->scale = Vector3(
				baseScale.x * (1.0f + impactPop * 0.62f + normalized * 0.62f),
				baseScale.y * (std::max)(0.0f, 1.0f - impactPop * 0.68f - normalized),
				baseScale.z * (1.0f + impactPop * 0.62f + normalized * 0.62f));
			t->AddRotationY(dt * (4.5f + normalized * 14.0f));
		}
		if(auto* mat = material.TryGet()) {
			const float flash = (1.0f - normalized) * (1.0f - normalized);
			mat->Material.BaseColor.x = baseMaterial.BaseColor.x + (1.0f - baseMaterial.BaseColor.x) * flash;
			mat->Material.BaseColor.y = baseMaterial.BaseColor.y + (0.48f - baseMaterial.BaseColor.y) * flash;
			mat->Material.BaseColor.z = baseMaterial.BaseColor.z + (0.06f - baseMaterial.BaseColor.z) * flash;
			mat->Material.EmissiveColor = float3(1.0f, 0.12f, 0.01f);
			mat->Material.EmissiveIntensity = baseMaterial.EmissiveIntensity + flash * 7.2f;
		}
		if(defeatTimer >= defeatDuration && !destroyQueued) destroyQueued = QueueDestroySelf();
	}

	void OnStop() override {
		if(auto* t = transform.TryGet()) t->scale = baseScale;
		if(auto* mat = material.TryGet()) mat->Material = baseMaterial;
	}

	void OnCollisionEnter(const HitInfo& hit) override {
		HandleContact(hit.other, true);
	}

	void OnCollisionStay(const HitInfo& hit) override {
		HandleContact(hit.other, false);
	}

private:
	void AlignColliderToModelCenter() {
		auto* t = transform.TryGet();
		auto* col = collider.TryGet();
		if(!t || !col || col->colliders.empty()) return;

		ColliderShape& shape = col->colliders.front();
		if(shape.type != ColliderType::Box) return;

		const float authoredOffsetY = shape.offset.y;
		if(std::abs(authoredOffsetY) <= 0.0001f) return;

		const float worldLift = authoredOffsetY * t->scale.y;
		visualCenterLift = worldLift;
		t->position.y += worldLift;
		shape.offset.y = 0.0f;

		if(shape.pxShape) {
			physx::PxTransform localPose = shape.pxShape->getLocalPose();
			localPose.p.y = 0.0f;
			shape.pxShape->setLocalPose(localPose);
		}

		if(auto* rigid = col->pRigidbodyDynamic) {
			physx::PxTransform actorPose = rigid->getGlobalPose();
			actorPose.p.y += worldLift;
			rigid->setGlobalPose(actorPose, true);
		}
		col->needsUpdate = true;
	}

	void EmitTurnDust() {
		auto* effect = particle.TryGet();
		if(!effect) return;
		effect->particleSize = 0.075f;
		PlatformerFeedback::DirectionalBurst(
			effect,
			Vector3(0.0f, -0.35f, 0.0f),
			patrolAxis * -direction,
			24,
			2.2f,
			1.8f,
			1.2f,
			0.42f,
			DirectX::XMFLOAT4(0.95f, 0.52f, 0.18f, 1.0f),
			DirectX::XMFLOAT4(0.42f, 0.16f, 0.06f, 1.0f));
	}

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

		const float stompReferenceY = enemyTransform->position.y - visualCenterLift;
		const float verticalVelocity = controller->GetVerticalVelocity();
		const bool descendingOrNearApex = verticalVelocity <= 0.65f;
		const bool above = playerPose->position.y >= stompReferenceY + stompHeightMargin;
		const float dx = playerPose->position.x - enemyTransform->position.x;
		const float dz = playerPose->position.z - enemyTransform->position.z;
		const float horizontalDistance = std::sqrt(dx * dx + dz * dz);
		const float catchRadius = (std::max)(
			0.72f, (std::max)(baseScale.x, baseScale.z) * 0.72f);
		if(descendingOrNearApex && above && horizontalDistance <= catchRadius) {
			Defeat(*controller);
			return;
		}

		controller->ApplyDamage(enemyTransform->position);
	}

	void Defeat(PlatformerCharacterController& player) {
		if(defeated) return;
		defeated = true;
		defeatTimer = 0.0f;
		player.ApplyStompBounce();
		if(auto* effect = particle.TryGet()) {
			for(auto& state : effect->Particle) state.LifeTime = 0.0f;
			effect->particleSize = 0.19f;
			PlatformerFeedback::LayeredBurst(
				effect,
				Vector3(0.0f, 0.45f, 0.0f),
				144,
				8.2f,
				9.4f,
				defeatDuration + 0.72f,
				DirectX::XMFLOAT4(1.0f, 0.42f, 0.04f, 1.0f),
				DirectX::XMFLOAT4(0.72f, 0.02f, 0.02f, 1.0f));
		}
		PlatformerFeedback::Play(audio.TryGet(), m_ref.GetScene());
		if(!camera.IsValid()) camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		if(auto* cameraController = camera.TryGet()) {
			cameraController->AddImpulse(0.48f, 0.26f, 0.060f, Vector3(0.0f, -1.0f, 0.0f));
		}

		if(auto* col = collider.TryGet()) {
			if(auto* rigid = col->pRigidbodyDynamic) {
				rigid->setLinearVelocity(physx::PxVec3(0.0f));
				rigid->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, true);
			}
		}
	}

	ComponentRef<TransformComponent> transform;
	ComponentRef<ColliderComponent> collider;
	ComponentRef<MaterialComponent> material;
	ComponentRef<ParticleComponent> particle;
	ComponentRef<AudioComponent> audio;
	ComponentRef<PlatformerCameraController> camera;
	Vector3 origin;
	Vector3 baseScale = Vector3(1.0f, 1.0f, 1.0f);
	MATERIAL baseMaterial{};
	float visualCenterLift = 0.0f;
	float direction = 1.0f;
	float defeatTimer = 0.0f;
	float turnDustCooldown = 0.0f;
	bool defeated = false;
	bool destroyQueued = false;
};