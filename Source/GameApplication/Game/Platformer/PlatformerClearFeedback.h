#pragma once

#include "Engine/Scene/Component/CustomScriptComponent.h"
#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/entityNameComponent.h"
#include "Engine/Scene/Component/audioComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerCameraController.h"
#include "Game/Platformer/PlatformerFeedback.h"
#include "Game/Platformer/PlatformerGameManager.h"
#include "Game/Platformer/PlatformerPlayerFeedback.h"
#include "Game/Platformer/PlatformerSceneAccess.h"
#include "Game/Platformer/PlatformerSoundLibrary.h"

#include <algorithm>
#include <cmath>

class PlatformerClearFeedback : public CustomScriptComponent {
	BEGIN_REFLECT(PlatformerClearFeedback)
		REFLECT_FIELD(float, clearVolume, 0.32f)
		REFLECT_FIELD(float, defeatExplosionDuration, 1.55f)
		REFLECT_FIELD(float, defeatBurstInterval, 0.13f)
		REFLECT_FIELD(float, confettiDuration, 6.0f)
		REFLECT_FIELD(float, confettiInterval, 0.09f)
		REFLECT_FIELD(int, confettiPerBurst, 24)

public:
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		ValidateSettings();
		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Platformer Clear Feedback");
		INSPECTOR_FIELDS();
		ImGui::Text("Clear SFX Played: %s", played ? "true" : "false");
		ImGui::Text("Defeat Celebration: %s", defeatCelebrationStarted ? "true" : "false");
		ImGui::Text("Confetti: %.2f / %.2f", confettiElapsed, confettiDuration);
	}

	void OnStart() override {
		ValidateSettings();
		manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		audio = GetComponentRef<AudioComponent>();
		ResolveEmitter();
		ResolveBossOrigin();
		played = false;
		defeatCelebrationStarted = false;
		clearCelebrationStarted = false;
		finalExplosionTriggered = false;
		defeatElapsed = 0.0f;
		defeatBurstTimer = 0.0f;
		confettiElapsed = 0.0f;
		confettiTimer = 0.0f;
		explosionBurstIndex = 0;
		confettiBurstIndex = 0;
	}

	void OnUpdate(float dt) override {
		const float safeDt = (std::max)(0.0f, dt);
		ResolveReferences();
		auto* game = manager.TryGet();
		if(!game) return;

		if(game->GetState() == PlatformerGameManager::RunState::BossDefeated &&
		   !defeatCelebrationStarted) {
			BeginBossDefeatCelebration(*game);
		}

		if(defeatCelebrationStarted && !game->IsCleared()) {
			TickBossDefeatCelebration(*game, safeDt);
		}

		if(game->IsCleared()) {
			if(!clearCelebrationStarted) BeginClearCelebration();
			TickConfetti(safeDt);
			PlayClearSound();
		}
	}

	void OnStop() override {
		ClearParticles();
	}

private:
	void ValidateSettings() {
		clearVolume = std::clamp(clearVolume, 0.0f, 1.0f);
		defeatExplosionDuration = std::clamp(defeatExplosionDuration, 0.8f, 3.0f);
		defeatBurstInterval = std::clamp(defeatBurstInterval, 0.06f, 0.30f);
		confettiDuration = std::clamp(confettiDuration, 2.0f, 12.0f);
		confettiInterval = std::clamp(confettiInterval, 0.04f, 0.25f);
		confettiPerBurst = std::clamp(confettiPerBurst, 8, 64);
	}

	void ResolveReferences() {
		if(!manager.IsValid()) {
			manager = PlatformerSceneAccess::FindFirst<PlatformerGameManager>(m_ref.GetScene());
		}
		if(!camera.IsValid()) {
			camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		}
		if(!emitterTransform.IsValid() || !emitterParticle.IsValid()) ResolveEmitter();
		if(!bossTransform.IsValid()) ResolveBossOrigin();
	}

	void ResolveEmitter() {
		auto feedback = PlatformerSceneAccess::FindFirst<PlatformerPlayerFeedback>(m_ref.GetScene());
		if(!feedback.IsValid()) return;
		emitterTransform = ComponentRef<TransformComponent>(feedback.GetEntityRef());
		emitterParticle = ComponentRef<ParticleComponent>(feedback.GetEntityRef());
	}

	void ResolveBossOrigin() {
		SceneContext* context = m_ref.GetScene();
		if(!context || !context->component) return;
		const auto entities = context->component->FindEntitiesWithComponent<NameComponent>();
		for(Entity entity : entities) {
			auto* name = context->component->GetComponent<NameComponent>(entity);
			if(!name || name->name != "PlatformerBoss") continue;
			bossTransform = ComponentRef<TransformComponent>(entity, context);
			if(auto* pose = bossTransform.TryGet()) {
				celebrationOrigin = pose->position + Vector3(0.0f, 0.85f, 0.0f);
			}
			return;
		}
	}

	void SetEmitterOrigin(const Vector3& origin) {
		celebrationOrigin = origin;
		if(auto* pose = emitterTransform.TryGet()) {
			pose->position = origin;
			pose->scale = Vector3(1.0f, 1.0f, 1.0f);
		}
	}

	void BeginBossDefeatCelebration(PlatformerGameManager& game) {
		defeatCelebrationStarted = true;
		defeatElapsed = 0.0f;
		defeatBurstTimer = 0.0f;
		explosionBurstIndex = 0;
		finalExplosionTriggered = false;

		if(auto* bossPose = bossTransform.TryGet()) {
			celebrationOrigin = bossPose->position + Vector3(0.0f, 0.85f, 0.0f);
		}
		SetEmitterOrigin(celebrationOrigin);
		ClearParticles();
		ConfigureExplosionParticles();

		EmitExplosionBurst(
			Vector3(0.0f, 0.15f, 0.0f),
			150, 13.5f, 13.0f, 1.30f, 0,
			false);
		EmitExplosionBurst(
			Vector3(0.0f, 0.05f, 0.0f),
			92, 17.0f, 6.5f, 1.00f, 1,
			true);
		EmitExplosionBurst(
			Vector3(0.0f, 0.65f, 0.0f),
			62, 9.0f, 18.0f, 1.15f, 2,
			true);

		game.RequestHitStop(0.22f);
		CameraImpulse(2.05f, 0.72f, 0.050f, Vector3(0.0f, 1.0f, 0.0f));
		PlatformerFeedback::Play(
			audio.TryGet(),
			m_ref.GetScene(),
			PlatformerSoundLibrary::ImpactPath);
	}

	void TickBossDefeatCelebration(PlatformerGameManager& game, float dt) {
		defeatElapsed += dt;
		defeatBurstTimer -= dt;
		SetEmitterOrigin(celebrationOrigin);

		const float normalized = std::clamp(
			defeatElapsed / (std::max)(0.01f, defeatExplosionDuration),
			0.0f,
			1.0f);

		if(!finalExplosionTriggered && normalized >= 0.62f) {
			finalExplosionTriggered = true;
			ClearParticles();
			ConfigureExplosionParticles();
			EmitExplosionBurst(
				Vector3(0.0f, 0.35f, 0.0f),
				220, 20.0f, 17.5f, 1.35f, 3,
				false);
			EmitExplosionBurst(
				Vector3(0.0f, 0.35f, 0.0f),
				118, 24.0f, 5.0f, 0.90f, 4,
				true);
			game.RequestHitStop(0.18f);
			CameraImpulse(2.35f, 0.78f, -0.045f, Vector3(0.0f, -0.35f, 1.0f));
			PlatformerFeedback::Play(
				audio.TryGet(),
				m_ref.GetScene(),
				PlatformerSoundLibrary::ClearPath);
		}

		if(defeatBurstTimer > 0.0f || normalized >= 0.94f) return;

		const float angle = static_cast<float>(explosionBurstIndex) * 2.39996323f;
		const float radius = 0.55f + 0.42f * static_cast<float>(explosionBurstIndex % 4);
		const Vector3 offset(
			std::cos(angle) * radius,
			0.30f + 0.32f * static_cast<float>(explosionBurstIndex % 3),
			std::sin(angle) * radius);

		EmitExplosionBurst(
			offset,
			34 + static_cast<int>(normalized * 20.0f),
			8.0f + normalized * 7.0f,
			8.5f + normalized * 5.5f,
			0.62f,
			explosionBurstIndex % 5,
			true);

		CameraImpulse(
			0.42f + normalized * 0.42f,
			0.18f,
			0.006f + normalized * 0.010f,
			Vector3(std::cos(angle), 0.25f, std::sin(angle)));

		++explosionBurstIndex;
		defeatBurstTimer = defeatBurstInterval * (1.0f - normalized * 0.38f);
	}

	void BeginClearCelebration() {
		clearCelebrationStarted = true;
		confettiElapsed = 0.0f;
		confettiTimer = 0.0f;
		confettiBurstIndex = 0;
		SetEmitterOrigin(celebrationOrigin);
		ClearParticles();
		ConfigureConfettiParticles();
		EmitConfettiBurst(96);
		CameraImpulse(1.05f, 0.52f, 0.020f, Vector3(0.0f, 1.0f, 0.0f));
	}

	void TickConfetti(float dt) {
		if(confettiElapsed >= confettiDuration) return;
		confettiElapsed += dt;
		confettiTimer -= dt;
		SetEmitterOrigin(celebrationOrigin);
		if(confettiTimer > 0.0f) return;

		ConfigureConfettiParticles();
		EmitConfettiBurst(confettiPerBurst);
		if(confettiBurstIndex % 8 == 0) {
			CameraImpulse(0.16f, 0.16f, 0.002f, Vector3(0.0f, 1.0f, 0.0f));
		}
		confettiTimer = confettiInterval *
			(0.82f + 0.12f * static_cast<float>(confettiBurstIndex % 4));
	}

	void PlayClearSound() {
		if(played) return;
		auto* audioComponent = audio.TryGet();
		if(!audioComponent) return;
		audioComponent->Volume = clearVolume;
		played = PlatformerFeedback::Play(
			audioComponent,
			m_ref.GetScene(),
			PlatformerSoundLibrary::ClearPath);
	}

	void ConfigureExplosionParticles() {
		if(auto* p = emitterParticle.TryGet()) {
			p->isLoop = false;
			p->particleSize = 0.20f;
			p->particleLifeTime = 1.35f;
			p->AddSpeed = Vector3(0.0f, -12.0f, 0.0f);
			p->MulSpeed = Vector3(0.94f, 0.94f, 0.94f);
			p->SpawnTimer = 0.0f;
		}
	}

	void ConfigureConfettiParticles() {
		if(auto* p = emitterParticle.TryGet()) {
			p->isLoop = false;
			p->particleSize = 0.12f;
			p->particleLifeTime = 5.2f;
			p->AddSpeed = Vector3(0.0f, -12.5f, 0.0f);
			p->MulSpeed = Vector3(0.985f, 0.992f, 0.985f);
			p->SpawnTimer = 0.0f;
		}
	}

	void EmitExplosionBurst(
		const Vector3& worldOffset,
		int count,
		float horizontalWorldSpeed,
		float upwardWorldSpeed,
		float lifetime,
		int paletteOffset,
		bool append
	) {
		auto* p = emitterParticle.TryGet();
		if(!p) return;
		if(!append) ClearParticles();

		const float size = (std::max)(0.02f, p->particleSize);
		const float inverseSize = 1.0f / size;
		const int safeCount = std::clamp(count, 1, MAXPARTICLE);
		int written = 0;
		for(int slot = 0; slot < MAXPARTICLE && written < safeCount; ++slot) {
			PARTICLE& state = p->Particle[slot];
			if(append && state.LifeTime > 0.0f) continue;

			const float angle =
				DirectX::XM_2PI * static_cast<float>(written) / static_cast<float>(safeCount) +
				static_cast<float>(explosionBurstIndex) * 0.37f;
			const float radial = horizontalWorldSpeed *
				(0.62f + 0.13f * static_cast<float>((written % 4) + 1));
			const float vertical = upwardWorldSpeed *
				(0.58f + 0.11f * static_cast<float>(written % 5));

			state.LifeTime = lifetime *
				(0.78f + 0.07f * static_cast<float>(written % 4));
			state.Position = worldOffset * inverseSize;
			state.Speed = Vector3(
				std::cos(angle) * radial * inverseSize,
				vertical * inverseSize,
				std::sin(angle) * radial * inverseSize);
			state.Color = ExplosionColor(paletteOffset + written);
			state.SizeScale = 0.72f + 0.18f * static_cast<float>(written % 6);
			++written;
		}
		p->SpawnCount = (std::max)(p->SpawnCount, written);
		p->particleLifeTime = (std::max)(p->particleLifeTime, lifetime);
	}

	void EmitConfettiBurst(int count) {
		auto* p = emitterParticle.TryGet();
		if(!p) return;
		const float size = (std::max)(0.02f, p->particleSize);
		const float inverseSize = 1.0f / size;
		const int safeCount = std::clamp(count, 1, MAXPARTICLE);
		int written = 0;

		for(int slot = 0; slot < MAXPARTICLE && written < safeCount; ++slot) {
			PARTICLE& state = p->Particle[slot];
			if(state.LifeTime > 0.0f) continue;

			const int sequence = confettiBurstIndex * safeCount + written;
			const float angle = static_cast<float>(sequence) * 2.39996323f;
			const float radius = 1.6f + 0.42f * static_cast<float>(sequence % 18);
			const float height = 5.2f + 0.48f * static_cast<float>(sequence % 8);
			const Vector3 worldPosition(
				std::cos(angle) * radius,
				height,
				std::sin(angle) * radius);
			const float drift = 0.55f + 0.16f * static_cast<float>(sequence % 6);

			state.LifeTime = 3.8f + 0.28f * static_cast<float>(sequence % 6);
			state.Position = worldPosition * inverseSize;
			state.Speed = Vector3(
				std::cos(angle + DirectX::XM_PIDIV2) * drift * inverseSize,
				(0.35f + 0.18f * static_cast<float>(sequence % 5)) * inverseSize,
				std::sin(angle + DirectX::XM_PIDIV2) * drift * inverseSize);
			state.Color = ConfettiColor(sequence);
			state.SizeScale = 0.62f + 0.13f * static_cast<float>(sequence % 7);
			++written;
		}
		p->SpawnCount = (std::max)(p->SpawnCount, written);
		++confettiBurstIndex;
	}

	void ClearParticles() {
		if(auto* p = emitterParticle.TryGet()) {
			for(auto& state : p->Particle) {
				state.LifeTime = 0.0f;
				state.Color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
				state.SizeScale = 1.0f;
			}
			p->SpawnCount = 0;
			p->SpawnTimer = 0.0f;
		}
	}

	void CameraImpulse(
		float strength,
		float duration,
		float fovKick,
		const Vector3& direction
	) {
		if(!camera.IsValid()) {
			camera = PlatformerSceneAccess::FindFirst<PlatformerCameraController>(m_ref.GetScene());
		}
		if(auto* controller = camera.TryGet()) {
			controller->AddImpulse(strength, duration, fovKick, direction);
		}
	}

	static DirectX::XMFLOAT4 ExplosionColor(int index) {
		switch(index % 5) {
		case 0: return DirectX::XMFLOAT4(1.00f, 0.96f, 0.72f, 1.0f);
		case 1: return DirectX::XMFLOAT4(1.00f, 0.58f, 0.08f, 1.0f);
		case 2: return DirectX::XMFLOAT4(1.00f, 0.20f, 0.04f, 1.0f);
		case 3: return DirectX::XMFLOAT4(1.00f, 0.24f, 0.78f, 1.0f);
		default: return DirectX::XMFLOAT4(0.38f, 0.82f, 1.00f, 1.0f);
		}
	}

	static DirectX::XMFLOAT4 ConfettiColor(int index) {
		switch(index % 7) {
		case 0: return DirectX::XMFLOAT4(1.00f, 0.34f, 0.62f, 0.95f);
		case 1: return DirectX::XMFLOAT4(1.00f, 0.82f, 0.22f, 0.95f);
		case 2: return DirectX::XMFLOAT4(0.34f, 0.90f, 1.00f, 0.95f);
		case 3: return DirectX::XMFLOAT4(0.64f, 0.48f, 1.00f, 0.95f);
		case 4: return DirectX::XMFLOAT4(1.00f, 0.58f, 0.34f, 0.95f);
		case 5: return DirectX::XMFLOAT4(0.64f, 1.00f, 0.54f, 0.95f);
		default: return DirectX::XMFLOAT4(1.00f, 0.96f, 0.90f, 0.95f);
		}
	}

	ComponentRef<PlatformerGameManager> manager;
	ComponentRef<PlatformerCameraController> camera;
	ComponentRef<AudioComponent> audio;
	ComponentRef<TransformComponent> bossTransform;
	ComponentRef<TransformComponent> emitterTransform;
	ComponentRef<ParticleComponent> emitterParticle;
	Vector3 celebrationOrigin;
	float defeatElapsed = 0.0f;
	float defeatBurstTimer = 0.0f;
	float confettiElapsed = 0.0f;
	float confettiTimer = 0.0f;
	int explosionBurstIndex = 0;
	int confettiBurstIndex = 0;
	bool played = false;
	bool defeatCelebrationStarted = false;
	bool clearCelebrationStarted = false;
	bool finalExplosionTriggered = false;
};
