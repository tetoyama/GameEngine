#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"
#include "Game/MiniGameCollection/Core/MiniGameMath.h"

#include "Scene/Component/materialComponent.h"
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
        if (!transformValue || !materialValue) {
            return false;
        }

        Voice voice{
            .transform = std::move(transform),
            .material = std::move(material),
            .baseColor = materialValue->Material.BaseColor
        };
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

        Voice* voice = nullptr;
        for (Voice& candidate : m_voices) {
            if (!candidate.active) {
                voice = &candidate;
                break;
            }
            if (!voice || candidate.playSerial < voice->playSerial) {
                voice = &candidate;
            }
        }
        if (!voice) {
            return false;
        }

        const float strength = std::clamp(intensity, 0.25f, 2.5f);
        voice->sceneToken = sceneToken;
        voice->position = position;
        voice->durationSeconds = 0.34f + strength * 0.07f;
        voice->remainingSeconds = voice->durationSeconds;
        voice->intensity = strength;
        voice->playSerial = serial;
        voice->active = true;

        if (TransformComponent* transform = voice->transform.TryGet()) {
            transform->position = Vector3(position.x, 0.75f, position.y);
            transform->scale = Vector3(0.12f, 0.12f, 0.12f);
        }
        if (MaterialComponent* material = voice->material.TryGet()) {
            material->Material.BaseColor = DirectX::XMFLOAT4(
                1.0f,
                0.72f + 0.1f * std::min(1.0f, strength),
                0.18f,
                0.92f
            );
            material->Material.EmissiveColor =
                DirectX::XMFLOAT3(1.0f, 0.55f, 0.08f);
            material->Material.EmissiveIntensity = 1.2f * strength;
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
            const float scale =
                (0.2f + normalized * 1.65f) * voice.intensity;

            if (TransformComponent* transform = voice.transform.TryGet()) {
                transform->position = Vector3(
                    voice.position.x,
                    0.65f + pulse * 0.55f,
                    voice.position.y
                );
                transform->scale = Vector3(scale, scale * 0.45f, scale);
                transform->SetRotationEuler(Vector3(
                    normalized * 1.7f,
                    normalized * 3.2f,
                    normalized * 0.9f
                ));
            }
            if (MaterialComponent* material = voice.material.TryGet()) {
                material->Material.BaseColor.w =
                    std::clamp(1.0f - normalized, 0.0f, 1.0f);
                material->Material.EmissiveIntensity =
                    (1.0f - normalized) * 1.4f * voice.intensity;
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
        DirectX::XMFLOAT4 baseColor{1.0f, 0.75f, 0.2f, 1.0f};
        SceneToken sceneToken = 0;
        Vec2 position{};
        float durationSeconds = 0.0f;
        float remainingSeconds = 0.0f;
        float intensity = 1.0f;
        std::uint64_t playSerial = 0;
        bool active = false;
    };

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
    }

    std::vector<Voice> m_voices;
};

} // namespace MiniGameCollection::Presentation
