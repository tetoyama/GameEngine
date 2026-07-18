#pragma once

#include "Engine/Scene/Component/TransformComponent.h"
#include "Engine/Scene/Component/entityNameComponent.h"
#include "Engine/Scene/Component/audioComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerSceneAccess.h"
#include "Game/Platformer/PlatformerSoundLibrary.h"

#include <algorithm>
#include <cmath>
#include <string>

class PlatformerFeedback {
public:
	static bool Play(AudioComponent* audio, SceneContext* context, const char* soundPath = nullptr) {
		if(!audio || !context) return false;
		PlatformerSoundLibrary::EnsureGenerated();

		if(soundPath && soundPath[0] != '\0' && audio->FilePath != soundPath) {
			audio->Reset();
			audio->FilePath = soundPath;
			audio->Loop = false;
			audio->PlayOnStart = false;
		}

		if(!audio->m_AudioData && !audio->FilePath.empty()) {
			if(auto* resources = PlatformerSceneAccess::Resources(context)) {
				audio->m_AudioData = resources->Load<AudioData>(audio->FilePath);
			}
		}
		audio->isInitialized = audio->m_AudioData != nullptr;

		if(auto* audioContext = PlatformerSceneAccess::Audio(context)) {
			return audio->Play(audioContext);
		}
		return false;
	}

	// Backwards-compatible entry point. Every existing interaction now receives a
	// multi-layer burst instead of a flat ring of identical white particles.
	static void Burst(
		ParticleComponent* particle,
		const Vector3& origin,
		int count = 16,
		float horizontalSpeed = 2.5f,
		float upwardSpeed = 3.0f,
		float lifetime = 0.5f
	) {
		DirectX::XMFLOAT4 primary(0.82f, 0.68f, 0.42f, 1.0f);
		DirectX::XMFLOAT4 secondary(0.24f, 0.74f, 1.0f, 1.0f);
		if(horizontalSpeed >= 5.0f && upwardSpeed >= 4.5f) {
			primary = DirectX::XMFLOAT4(1.0f, 0.44f, 0.05f, 1.0f);
			secondary = DirectX::XMFLOAT4(0.92f, 0.04f, 0.03f, 1.0f);
		} else if(upwardSpeed >= 3.5f) {
			primary = DirectX::XMFLOAT4(0.22f, 0.88f, 1.0f, 1.0f);
			secondary = DirectX::XMFLOAT4(0.58f, 0.28f, 1.0f, 1.0f);
		}
		LayeredBurst(
			particle,
			origin,
			count,
			horizontalSpeed,
			upwardSpeed,
			lifetime,
			primary,
			secondary);
	}

	// Produces four readable layers in one ParticleComponent allocation:
	// a low dust ring, a broad middle shell, long sparks and bright core motes.
	static void LayeredBurst(
		ParticleComponent* particle,
		const Vector3& origin,
		int count,
		float horizontalSpeed,
		float upwardSpeed,
		float lifetime,
		const DirectX::XMFLOAT4& primary,
		const DirectX::XMFLOAT4& secondary
	) {
		if(!particle) return;
		// Existing gameplay events were authored for the old single-ring renderer.
		// Double only modest requests; large authored set pieces keep their explicit
		// budget and remain bounded by MAXPARTICLE.
		const int expandedCount = count < 96 ? count * 2 : count;
		const int safeCount = std::clamp(expandedCount, 1, MAXPARTICLE);
		particle->isLoop = false;
		particle->SpawnPosition = origin;
		particle->SpawnCount = safeCount;
		particle->particleLifeTime = (std::max)(0.05f, lifetime);
		particle->SpawnTimer = 0.0f;

		constexpr float goldenAngle = 2.39996323f;
		for(int i = 0; i < safeCount; ++i) {
			const int layer = i % 8;
			const float angle = goldenAngle * static_cast<float>(i);
			const float wave = 0.5f + 0.5f * std::sin(static_cast<float>(i) * 1.731f);

			float radialScale = 0.82f + wave * 0.30f;
			float verticalScale = 0.72f + 0.10f * static_cast<float>(i % 5);
			float sizeScale = 0.82f + 0.10f * static_cast<float>(i % 4);

			if(layer == 0 || layer == 4) {
				// Ground-hugging dust ring gives contact events a clear footprint.
				radialScale = 1.18f + wave * 0.38f;
				verticalScale = 0.18f + wave * 0.16f;
				sizeScale = 1.35f + wave * 0.45f;
			} else if(layer == 1 || layer == 5) {
				// Long thin sparks form the outer silhouette.
				radialScale = 1.55f + wave * 0.55f;
				verticalScale = 0.88f + wave * 0.30f;
				sizeScale = 0.48f + wave * 0.22f;
			} else if(layer == 2) {
				// Bright core motes linger near the interaction point.
				radialScale = 0.34f + wave * 0.22f;
				verticalScale = 1.18f + wave * 0.24f;
				sizeScale = 1.65f + wave * 0.40f;
			}

			const bool whiteHot = i % 11 == 0;
			const float colorBlend = static_cast<float>((i * 37) % 100) / 99.0f;
			DirectX::XMFLOAT4 color = LerpColor(primary, secondary, colorBlend);
			if(whiteHot) color = LerpColor(color, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.78f);

			particle->Particle[i].LifeTime = particle->particleLifeTime *
				(0.72f + 0.28f * static_cast<float>((i % 7) + 1) / 7.0f);
			particle->Particle[i].Position = origin;
			particle->Particle[i].Color = color;
			particle->Particle[i].SizeScale = sizeScale;
			particle->Particle[i].Speed = Vector3(
				std::cos(angle) * horizontalSpeed * radialScale,
				upwardSpeed * verticalScale,
				std::sin(angle) * horizontalSpeed * radialScale);
		}
	}

	// Directional impacts are used for wall kicks, platform mounting and camera
	// transitions. The forward cone is mixed with a smaller radial shock ring.
	static void DirectionalBurst(
		ParticleComponent* particle,
		const Vector3& origin,
		const Vector3& direction,
		int count,
		float forwardSpeed,
		float spreadSpeed,
		float upwardSpeed,
		float lifetime,
		const DirectX::XMFLOAT4& primary,
		const DirectX::XMFLOAT4& secondary
	) {
		if(!particle) return;
		const int expandedCount = count < 72 ? count * 2 : count;
		const int safeCount = std::clamp(expandedCount, 1, MAXPARTICLE);
		Vector3 forward = direction;
		if(forward.length() <= 0.0001f) forward = Vector3(0.0f, 1.0f, 0.0f);
		forward = forward.normalize();
		Vector3 side(-forward.z, 0.0f, forward.x);
		if(side.length() <= 0.0001f) side = Vector3(1.0f, 0.0f, 0.0f);
		else side = side.normalize();

		particle->isLoop = false;
		particle->SpawnPosition = origin;
		particle->SpawnCount = safeCount;
		particle->particleLifeTime = (std::max)(0.05f, lifetime);
		particle->SpawnTimer = 0.0f;

		constexpr float goldenAngle = 2.39996323f;
		for(int i = 0; i < safeCount; ++i) {
			const float angle = goldenAngle * static_cast<float>(i);
			const float wave = 0.5f + 0.5f * std::sin(static_cast<float>(i) * 2.173f);
			const bool shockRing = i % 5 == 0;
			const float sideAmount = std::cos(angle) * spreadSpeed * (0.45f + wave * 0.75f);
			const float radialAmount = std::sin(angle) * spreadSpeed * (0.25f + wave * 0.55f);
			Vector3 velocity;
			if(shockRing) {
				velocity = side * sideAmount * 1.65f +
					Vector3(-side.z, 0.0f, side.x) * radialAmount * 1.65f +
					Vector3(0.0f, upwardSpeed * 0.18f, 0.0f);
			} else {
				velocity = forward * (forwardSpeed * (0.72f + wave * 0.58f)) +
					side * sideAmount +
					Vector3(0.0f, upwardSpeed * (0.55f + wave * 0.65f), 0.0f);
			}

			DirectX::XMFLOAT4 color = LerpColor(
				primary,
				secondary,
				static_cast<float>((i * 29) % 100) / 99.0f);
			if(i % 13 == 0) color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

			particle->Particle[i].LifeTime = particle->particleLifeTime * (0.76f + wave * 0.24f);
			particle->Particle[i].Position = origin;
			particle->Particle[i].Color = color;
			particle->Particle[i].SizeScale = shockRing ? 1.35f : (0.55f + wave * 0.70f);
			particle->Particle[i].Speed = velocity;
		}
	}

	// Uses the scene's persistent PlatformerPlayerFeedback emitter. This gives
	// scripts without their own ParticleComponent (moving platforms, camera zones,
	// arena gates) a consistent visual response without creating runtime entities.
	static void SharedBurst(
		SceneContext* context,
		const Vector3& worldOrigin,
		int count,
		float horizontalSpeed,
		float upwardSpeed,
		float lifetime,
		float particleSize,
		const DirectX::XMFLOAT4& primary,
		const DirectX::XMFLOAT4& secondary
	) {
		WithSharedEmitter(context, worldOrigin, particleSize,
			[&](ParticleComponent* particle) {
				LayeredBurst(
					particle,
					Vector3(),
					count,
					horizontalSpeed,
					upwardSpeed,
					lifetime,
					primary,
					secondary);
			});
	}

	static void SharedDirectionalBurst(
		SceneContext* context,
		const Vector3& worldOrigin,
		const Vector3& direction,
		int count,
		float forwardSpeed,
		float spreadSpeed,
		float upwardSpeed,
		float lifetime,
		float particleSize,
		const DirectX::XMFLOAT4& primary,
		const DirectX::XMFLOAT4& secondary
	) {
		WithSharedEmitter(context, worldOrigin, particleSize,
			[&](ParticleComponent* particle) {
				DirectionalBurst(
					particle,
					Vector3(),
					direction,
					count,
					forwardSpeed,
					spreadSpeed,
					upwardSpeed,
					lifetime,
					primary,
					secondary);
			});
	}

private:
	static DirectX::XMFLOAT4 LerpColor(
		const DirectX::XMFLOAT4& a,
		const DirectX::XMFLOAT4& b,
		float t
	) {
		t = std::clamp(t, 0.0f, 1.0f);
		return DirectX::XMFLOAT4(
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t);
	}

	template<class EmitFn>
	static void WithSharedEmitter(
		SceneContext* context,
		const Vector3& worldOrigin,
		float particleSize,
		EmitFn&& emit
	) {
		if(!context || !context->component) return;
		const auto entities = context->component->FindEntitiesWithComponent<NameComponent>();
		for(Entity entity : entities) {
			auto* name = context->component->GetComponent<NameComponent>(entity);
			if(!name || name->name != "PlatformerPlayerFeedback") continue;

			auto* pose = context->component->GetComponent<TransformComponent>(entity);
			auto* particle = context->component->GetComponent<ParticleComponent>(entity);
			if(!pose || !particle) return;

			for(auto& state : particle->Particle) state.LifeTime = 0.0f;
			pose->position = worldOrigin;
			pose->scale = Vector3(1.0f, 1.0f, 1.0f);
			particle->particleSize = (std::max)(0.02f, particleSize);
			emit(particle);
			return;
		}
	}
};