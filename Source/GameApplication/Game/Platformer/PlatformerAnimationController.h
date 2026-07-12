#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/modelRendererComponent.h"
#include "Game/Platformer/PlatformerCharacterController.h"

#include <algorithm>
#include <cmath>
#include <string>

class PlatformerAnimationController : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerAnimationController)
		REFLECT_FIELD(std::string, idleAnimation, std::string("Asset/Model/Akai_Idle.fbx"))
		REFLECT_FIELD(std::string, runAnimation, std::string("Asset/Model/Akai_Run.fbx"))
		REFLECT_FIELD(std::string, jumpUpAnimation, std::string("Asset/Model/Jumping Up.fbx"))
		REFLECT_FIELD(std::string, fallAnimation, std::string("Asset/Model/Jumping Down.fbx"))
		REFLECT_FIELD(float, blendSpeed, 8.0f)
		REFLECT_FIELD(float, runPlaybackScale, 1.0f)
		REFLECT_FIELD(float, squashAmount, 0.12f)

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
		ImGui::Text("Platformer Animation Controller");
		INSPECTOR_FIELDS();
	}

	void OnStart() override {
		character = GetComponentRef<PlatformerCharacterController>();
		transform = GetComponentRef<TransformComponent>();
		model = GetComponentRef<ModelRendererComponent>();
		if(auto* t = transform.TryGet()) baseScale = t->scale;
		lastJumpRevision = character.IsValid() ? character->GetJumpEventRevision() : 0;
		lastLandRevision = character.IsValid() ? character->GetLandEventRevision() : 0;
		lastWallRevision = character.IsValid() ? character->GetWallKickEventRevision() : 0;
		lastDamageRevision = character.IsValid() ? character->GetDamageEventRevision() : 0;
	}

	void OnFixedUpdate(float dt) override {
		auto* controller = character.TryGet();
		auto* t = transform.TryGet();
		if(!controller || !t) return;

		if(!animationsReady) InitializeAnimations();
		UpdateFeedbackState(*controller);
		UpdateTransformFallback(*controller, *t, dt);
		UpdateAnimationWeights(*controller, dt);
	}

	void OnStop() override {
		if(auto* t = transform.TryGet()) t->scale = baseScale;
	}

private:
	enum AnimationSlot : size_t {
		Run = 0,
		Idle = 1,
		JumpUp = 2,
		Fall = 3
	};

	void InitializeAnimations() {
		auto* renderer = model.TryGet();
		if(!renderer || !renderer->model) return;
		auto& clips = renderer->model->m_Animation;
		if(clips.find("PlatformerIdle") == clips.end()) renderer->model->LoadAnimation(idleAnimation.c_str(), "PlatformerIdle");
		if(clips.find("PlatformerRun") == clips.end()) renderer->model->LoadAnimation(runAnimation.c_str(), "PlatformerRun");
		if(clips.find("PlatformerJumpUp") == clips.end()) renderer->model->LoadAnimation(jumpUpAnimation.c_str(), "PlatformerJumpUp");
		if(clips.find("PlatformerFall") == clips.end()) renderer->model->LoadAnimation(fallAnimation.c_str(), "PlatformerFall");

		renderer->blendedAnimations.clear();
		renderer->blendedAnimations.push_back({"PlatformerRun", 0.0f, 0.0f, true});
		renderer->blendedAnimations.push_back({"PlatformerIdle", 1.0f, 0.0f, true});
		renderer->blendedAnimations.push_back({"PlatformerJumpUp", 0.0f, 0.0f, false});
		renderer->blendedAnimations.push_back({"PlatformerFall", 0.0f, 0.0f, false});
		animationsReady = true;
	}

	void UpdateFeedbackState(const PlatformerCharacterController& controller) {
		if(controller.GetJumpEventRevision() != lastJumpRevision) {
			lastJumpRevision = controller.GetJumpEventRevision();
			jumpPulse = controller.GetJumpStage() == 3 ? 1.0f : 0.55f;
		}
		if(controller.GetLandEventRevision() != lastLandRevision) {
			lastLandRevision = controller.GetLandEventRevision();
			landPulse = 1.0f;
		}
		if(controller.GetWallKickEventRevision() != lastWallRevision) {
			lastWallRevision = controller.GetWallKickEventRevision();
			wallSpin = 1.0f;
		}
		if(controller.GetDamageEventRevision() != lastDamageRevision) {
			lastDamageRevision = controller.GetDamageEventRevision();
			damagePulse = 1.0f;
		}
	}

	void UpdateTransformFallback(const PlatformerCharacterController& controller, TransformComponent& t, float dt) {
		jumpPulse = (std::max)(0.0f, jumpPulse - dt * 3.5f);
		landPulse = (std::max)(0.0f, landPulse - dt * 6.0f);
		wallSpin = (std::max)(0.0f, wallSpin - dt * 3.0f);
		damagePulse = (std::max)(0.0f, damagePulse - dt * 4.0f);

		const float verticalStretch = jumpPulse * squashAmount;
		const float landingSquash = landPulse * squashAmount;
		const float damageSquash = damagePulse * squashAmount * 0.5f;
		const Vector3 targetScale(
			baseScale.x * (1.0f + landingSquash + damageSquash - verticalStretch * 0.5f),
			baseScale.y * (1.0f + verticalStretch - landingSquash),
			baseScale.z * (1.0f + landingSquash + damageSquash - verticalStretch * 0.5f));
		const float scaleBlend = 1.0f - std::exp(-12.0f * dt);
		t.scale = Vec3Lerp(t.scale, targetScale, scaleBlend);

		if(wallSpin > 0.0f) {
			t.AddRotationY(dt * DirectX::XM_2PI * (0.65f + wallSpin));
		}
		if(controller.GetJumpStage() == 3 && !controller.IsGrounded()) {
			t.AddRotationY(dt * DirectX::XM_2PI * 1.2f);
		}
	}

	void UpdateAnimationWeights(const PlatformerCharacterController& controller, float dt) {
		auto* renderer = model.TryGet();
		if(!renderer || renderer->blendedAnimations.size() < 4) return;

		float run = std::clamp(controller.GetHorizontalSpeed() / 7.0f, 0.0f, 1.0f);
		float idle = 1.0f - run;
		float up = (!controller.IsGrounded() && controller.GetVerticalVelocity() >= -0.05f) ? 1.0f : 0.0f;
		float fall = (!controller.IsGrounded() && controller.GetVerticalVelocity() < -0.05f) ? 1.0f : 0.0f;
		if(up > 0.0f || fall > 0.0f) {
			run = 0.0f;
			idle = 0.0f;
		}

		const float amount = 1.0f - std::exp(-(std::max)(0.1f, blendSpeed) * dt);
		Blend(renderer->blendedAnimations[Run].weight, run, amount);
		Blend(renderer->blendedAnimations[Idle].weight, idle, amount);
		Blend(renderer->blendedAnimations[JumpUp].weight, up, amount);
		Blend(renderer->blendedAnimations[Fall].weight, fall, amount);
	}

	static void Blend(float& current, float target, float amount) {
		current += (target - current) * std::clamp(amount, 0.0f, 1.0f);
		current = std::clamp(current, 0.0f, 1.0f);
	}

	ComponentRef<PlatformerCharacterController> character;
	ComponentRef<TransformComponent> transform;
	ComponentRef<ModelRendererComponent> model;
	Vector3 baseScale = Vector3(1.0f, 1.0f, 1.0f);
	bool animationsReady = false;
	float jumpPulse = 0.0f;
	float landPulse = 0.0f;
	float wallSpin = 0.0f;
	float damagePulse = 0.0f;
	uint32_t lastJumpRevision = 0;
	uint32_t lastLandRevision = 0;
	uint32_t lastWallRevision = 0;
	uint32_t lastDamageRevision = 0;
};
