#pragma once

#include "Engine/Scene/Component/audioComponent.h"
#include "Engine/Scene/Component/particleComponent.h"
#include "Game/Platformer/PlatformerSceneAccess.h"

#include <algorithm>
#include <cmath>

class PlatformerFeedback {
public:
	static void Play(AudioComponent* audio, SceneContext* context) {
		if(!audio || !context) return;
		if(auto* audioContext = PlatformerSceneAccess::Audio(context)) {
			audio->Play(audioContext);
		}
	}

	static void Burst(
		ParticleComponent* particle,
		const Vector3& origin,
		int count = 16,
		float horizontalSpeed = 2.5f,
		float upwardSpeed = 3.0f,
		float lifetime = 0.5f
	) {
		if(!particle) return;
		const int safeCount = std::clamp(count, 1, MAXPARTICLE);
		particle->isLoop = false;
		particle->SpawnPosition = origin;
		particle->SpawnCount = safeCount;
		particle->particleLifeTime = (std::max)(0.05f, lifetime);
		particle->SpawnTimer = 0.0f;

		for(int i = 0; i < safeCount; ++i) {
			const float angle = DirectX::XM_2PI * static_cast<float>(i) / static_cast<float>(safeCount);
			const float radial = horizontalSpeed * (0.75f + 0.25f * static_cast<float>((i % 3) + 1) / 3.0f);
			particle->Particle[i].LifeTime = particle->particleLifeTime;
			particle->Particle[i].Position = origin;
			particle->Particle[i].Speed = Vector3(
				std::cos(angle) * radial,
				upwardSpeed * (0.75f + 0.08f * static_cast<float>(i % 4)),
				std::sin(angle) * radial);
		}
	}
};
