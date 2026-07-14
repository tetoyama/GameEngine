#pragma once

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
			particle->Particle[i].Color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			particle->Particle[i].SizeScale = 1.0f;
			particle->Particle[i].Speed = Vector3(
				std::cos(angle) * radial,
				upwardSpeed * (0.75f + 0.08f * static_cast<float>(i % 4)),
				std::sin(angle) * radial);
		}
	}
};
