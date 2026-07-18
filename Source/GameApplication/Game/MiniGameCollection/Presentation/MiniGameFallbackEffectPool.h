#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"
#include "Game/MiniGameCollection/Core/MiniGameMath.h"

#include "Scene/Component/LightComponent.h"
#include "Scene/Component/materialComponent.h"
#include "Scene/Component/particleComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Reference/ComponentRef.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace MiniGameCollection::Presentation {

class MiniGameFallbackEffectPool {
public:
    bool RegisterVoice(
        ComponentRef<TransformComponent> transform,
        ComponentRef<MaterialComponent> material
    ) {
        TransformComponent* transformValue = transform.TryGet();
        MaterialComponent* materialValue = material.TryGet();
        SceneContext* context = transform.GetScene();
        const Entity entity = transform.GetEntityID();
        if (!transformValue || !materialValue || !context || !context->component) {
            return false;
        }

        ComponentRef<ParticleComponent> particle(entity, context);
        if (!particle.IsValid()) {
            particle = context->component->AddComponent<ParticleComponent>(entity);
        }
        ComponentRef<LightComponent> light(entity, context);
        if (!light.IsValid()) {
            light = context->component->AddComponent<LightComponent>(entity);
        }

        Voice voice{
            .transform = std::move(transform),
            .material = std::move(material),
            .particle = std::move(particle),
            .light = std::move(light),
            .baseColor = materialValue->Material.BaseColor
        };
        ConfigureVoiceComponents(voice);
        ResetVoice(voice);
        m_voices.push_back(std::move(voice));
        return true;
    }

    bool Play(
        SceneToken sceneToken,
        Vec2 position,
        float intensity,
        std::uint64_t serial
    ) {
        if (sceneToken == 0 || m_voices.empty()) {
            return false;
        }

        Voice* voice = SelectVoice();
        if (!voice) {
            return false;
        }

        constexpr float Tau = 6.28318530717958647692f;
        const float strength = std::clamp(intensity, 0.25f, 2.5f);
        const float phase = static_cast<float>(serial % 4093u) * 0.173f;
        const DirectX::XMFLOAT4 color = ResolveEffectColor(strength, serial);

        voice->sceneToken = sceneToken;
        voice->position = position;
        voice->durationSeconds = 0.30f + strength * 0.075f;
        voice->remainingSeconds = voice->durationSeconds;
        voice->intensity = strength;
        voice->playSerial = serial;
        voice->color = color;
        voice->active = true;

        if (TransformComponent* transform = voice->transform.TryGet()) {
            transform->position = Vector3(position.x, 0.68f, position.y);
            transform->scale = Vector3(0.10f, 0.10f, 0.10f);
            transform->SetRotationEuler(Vector3(0.0f, phase, 0.0f));
        }
        if (MaterialComponent* material = voice->material.TryGet()) {
            material->ShaderID = 1;
            material->Material.BaseColor = color;
            material->Material.Metallic = 0.18f;
            material->Material.Roughness = 0.18f;
            material->Material.EmissiveColor = DirectX::XMFLOAT3(
                color.x,
                color.y,
                color.z
            );
            material->Material.EmissiveIntensity = 2.4f * strength;
            material->Material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
            material->Material.MaterialFlags &= ~MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
        }

        if (ParticleComponent* particle = voice->particle.TryGet()) {
            const int particleCount = std::clamp(
                8 + static_cast<int>(std::lround(strength * 7.0f)),
                8,
                28
            );
            particle->isLoop = false;
            particle->SpawnCount = particleCount;
            particle->particleLifeTime = voice->durationSeconds * 0.95f;
            particle->particleSize = 0.16f + strength * 0.025f;
            particle->SpawnTimer = 0.0f;
            particle->AddSpeed = Vector3(0.0f, -7.5f, 0.0f);
            particle->MulSpeed = Vector3(0.76f, 0.76f, 0.76f);

            for (int index = 0; index < MAXPARTICLE; ++index) {
                PARTICLE& value = particle->Particle[index];
                if (index >= particleCount) {
                    value.LifeTime = 0.0f;
                    continue;
                }
                const float normalized = static_cast<float>(index) /
                    static_cast<float>(particleCount);
                const float angle = phase + normalized * Tau;
                const float alternating = index % 2 == 0 ? 1.0f : 0.72f;
                const float radialSpeed = (5.5f + strength * 2.5f) * alternating;
                const float upSpeed = 3.4f + strength * 1.4f +
                    static_cast<float>(index % 3) * 0.45f;
                value.Position = Vector3(0.0f, 0.0f, 0.0f);
                value.Speed = Vector3(
                    std::cos(angle) * radialSpeed,
                    upSpeed,
                    std::sin(angle) * radialSpeed
                );
                value.LifeTime = particle->particleLifeTime *
                    (0.72f + static_cast<float>(index % 5) * 0.055f);
            }
        }

        if (LightComponent* light = voice->light.TryGet()) {
            light->light.Enable = 1;
            light->light.LightType = LIGHT_TYPE_POINT;
            light->light.CastShadow = 0;
            light->light.Position = DirectX::XMFLOAT4(
                position.x,
                1.1f,
                position.y,
                1.0f
            );
            light->light.Diffuse = DirectX::XMFLOAT4(
                color.x * (0.65f + strength * 0.32f),
                color.y * (0.65f + strength * 0.32f),
                color.z * (0.65f + strength * 0.32f),
                1.0f
            );
            light->light.Ambient = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            light->light.Param.x = 4.0f + strength * 2.2f;
            light->dirty = true;
        }
        return true;
    }

    void Tick(float unscaledDeltaTime) {
        const float delta = std::max(0.0f, unscaledDeltaTime);
        for (Voice& voice : m_voices) {
            if (!voice.active) {
                continue;
            }

            voice.remainingSeconds = std::max(
                0.0f,
                voice.remainingSeconds - delta
            );
            const float normalized = voice.durationSeconds > 0.0f
                ? 1.0f - voice.remainingSeconds / voice.durationSeconds
                : 1.0f;
            const float pulse = std::sin(normalized * 3.14159265358979323846f);
            const float coreScale =
                (0.15f + normalized * 1.95f) * voice.intensity;
            const float fade = std::clamp(1.0f - normalized, 0.0f, 1.0f);

            if (TransformComponent* transform = voice.transform.TryGet()) {
                transform->position = Vector3(
                    voice.position.x,
                    0.58f + pulse * 0.62f,
                    voice.position.y
                );
                transform->scale = Vector3(
                    coreScale,
                    std::max(0.05f, coreScale * (0.26f + pulse * 0.18f)),
                    coreScale
                );
                transform->SetRotationEuler(Vector3(
                    normalized * 1.3f,
                    normalized * 4.4f,
                    normalized * 0.8f
                ));
            }
            if (MaterialComponent* material = voice.material.TryGet()) {
                material->Material.BaseColor.w = fade;
                material->Material.EmissiveIntensity =
                    (0.2f + fade * 3.8f) * voice.intensity;
            }
            if (LightComponent* light = voice.light.TryGet()) {
                const float lightEnvelope = fade * fade;
                light->light.Enable = lightEnvelope > 0.015f ? 1 : 0;
                light->light.Position = DirectX::XMFLOAT4(
                    voice.position.x,
                    0.85f + pulse * 0.5f,
                    voice.position.y,
                    1.0f
                );
                light->light.Diffuse = DirectX::XMFLOAT4(
                    voice.color.x * lightEnvelope * (0.8f + voice.intensity * 0.45f),
                    voice.color.y * lightEnvelope * (0.8f + voice.intensity * 0.45f),
                    voice.color.z * lightEnvelope * (0.8f + voice.intensity * 0.45f),
                    1.0f
                );
                light->light.Param.x = 3.0f + voice.intensity * 2.4f;
                light->dirty = true;
            }

            if (voice.remainingSeconds <= 0.0f) {
                ResetVoice(voice);
            }
        }
    }

    void CancelAllForScene(SceneToken sceneToken) {
        for (Voice& voice : m_voices) {
            if (voice.sceneToken == sceneToken) {
                ResetVoice(voice);
            }
        }
    }

    std::size_t CountActive() const noexcept {
        return static_cast<std::size_t>(std::count_if(
            m_voices.begin(),
            m_voices.end(),
            [](const Voice& voice) { return voice.active; }
        ));
    }

private:
    struct Voice {
        ComponentRef<TransformComponent> transform;
        ComponentRef<MaterialComponent> material;
        ComponentRef<ParticleComponent> particle;
        ComponentRef<LightComponent> light;
        DirectX::XMFLOAT4 baseColor{1.0f, 0.75f, 0.2f, 1.0f};
        DirectX::XMFLOAT4 color{1.0f, 0.75f, 0.2f, 1.0f};
        SceneToken sceneToken = 0;
        Vec2 position{};
        float durationSeconds = 0.0f;
        float remainingSeconds = 0.0f;
        float intensity = 1.0f;
        std::uint64_t playSerial = 0;
        bool active = false;
    };

    Voice* SelectVoice() {
        Voice* selected = nullptr;
        for (Voice& candidate : m_voices) {
            if (!candidate.active) {
                return &candidate;
            }
            if (!selected || candidate.playSerial < selected->playSerial) {
                selected = &candidate;
            }
        }
        if (selected) {
            ResetVoice(*selected);
        }
        return selected;
    }

    static DirectX::XMFLOAT4 ResolveEffectColor(
        float strength,
        std::uint64_t serial
    ) noexcept {
        if (strength >= 1.65f) {
            return {1.0f, 0.22f, 0.07f, 1.0f};
        }
        if (strength >= 1.05f) {
            return {1.0f, 0.68f, 0.12f, 1.0f};
        }
        switch (serial % 3u) {
        case 0: return {0.22f, 0.72f, 1.0f, 1.0f};
        case 1: return {0.45f, 1.0f, 0.68f, 1.0f};
        default: return {1.0f, 0.86f, 0.28f, 1.0f};
        }
    }

    static void ConfigureVoiceComponents(Voice& voice) {
        if (ParticleComponent* particle = voice.particle.TryGet()) {
            particle->isLoop = false;
            particle->SpawnInterval = 0.0f;
            particle->SpawnCount = 0;
            particle->particleLifeTime = 0.4f;
            particle->particleSize = 0.18f;
            particle->SpawnTimer = 0.0f;
            particle->SpawnPosition = Vector3(0.0f, 0.0f, 0.0f);
            particle->SpawnPositionRandom = Vector3(0.0f, 0.0f, 0.0f);
            particle->StartSpeed = Vector3(0.0f, 0.0f, 0.0f);
            particle->StartSpeedRandom = Vector3(0.0f, 0.0f, 0.0f);
            particle->AddSpeed = Vector3(0.0f, -7.5f, 0.0f);
            particle->MulSpeed = Vector3(0.76f, 0.76f, 0.76f);
        }
        if (LightComponent* light = voice.light.TryGet()) {
            light->light.Enable = 0;
            light->light.LightType = LIGHT_TYPE_POINT;
            light->light.CastShadow = 0;
            light->light.Ambient = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            light->light.Param = DirectX::XMFLOAT4(5.0f, 0.0f, 0.0f, 0.0f);
            light->dirty = true;
        }
    }

    static void ResetVoice(Voice& voice) {
        voice.sceneToken = 0;
        voice.durationSeconds = 0.0f;
        voice.remainingSeconds = 0.0f;
        voice.intensity = 1.0f;
        voice.active = false;
        if (TransformComponent* transform = voice.transform.TryGet()) {
            transform->position = Vector3(0.0f, -1000.0f, 0.0f);
            transform->scale = Vector3(0.0f, 0.0f, 0.0f);
        }
        if (MaterialComponent* material = voice.material.TryGet()) {
            material->Material.BaseColor = voice.baseColor;
            material->Material.BaseColor.w = 0.0f;
            material->Material.EmissiveIntensity = 0.0f;
        }
        if (ParticleComponent* particle = voice.particle.TryGet()) {
            particle->SpawnCount = 0;
            particle->SpawnTimer = 0.0f;
            for (PARTICLE& value : particle->Particle) {
                value.LifeTime = 0.0f;
            }
        }
        if (LightComponent* light = voice.light.TryGet()) {
            light->light.Enable = 0;
            light->light.Diffuse = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            light->dirty = true;
        }
    }

    std::vector<Voice> m_voices;
};

} // namespace MiniGameCollection::Presentation
