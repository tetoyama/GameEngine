#pragma once

#include "Game/MiniGameCollection/Presentation/MiniGamePresentationService.h"

#include "Scene/Component/EffectComponent.h"
#include "Scene/Component/audioComponent.h"
#include "Scene/Component/materialComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Reference/ComponentRef.h"
#include "Scene/scene.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MiniGameCollection::Presentation {

class MiniGamePresentationEngineBackend final
    : public IMiniGamePresentationBackend {
public:
    explicit MiniGamePresentationEngineBackend(SceneContext* persistentScene)
        : m_scene(persistentScene) {
    }

    bool RegisterAudioVoice(
        std::string cueId,
        ComponentRef<AudioComponent> component
    ) {
        if (cueId.empty() || !component.IsValid()) {
            return false;
        }
        m_audioVoices[std::move(cueId)].push_back({
            .component = std::move(component)
        });
        return true;
    }

    bool RegisterEffectVoice(
        std::string cueId,
        ComponentRef<EffectComponent> effect,
        ComponentRef<TransformComponent> transform
    ) {
        if (cueId.empty() || !effect.IsValid() || !transform.IsValid()) {
            return false;
        }
        m_effectVoices[std::move(cueId)].push_back({
            .effect = std::move(effect),
            .transform = std::move(transform)
        });
        return true;
    }

    bool RegisterUiTarget(
        std::string targetId,
        ComponentRef<TransformComponent> transform
    ) {
        TransformComponent* value = transform.TryGet();
        if (targetId.empty() || !value) {
            return false;
        }
        m_uiTargets[std::move(targetId)] = {
            .transform = std::move(transform),
            .baseScale = value->scale
        };
        return true;
    }

    bool RegisterCamera(ComponentRef<TransformComponent> transform) {
        TransformComponent* value = transform.TryGet();
        if (!value) {
            return false;
        }
        m_camera = {
            .transform = std::move(transform),
            .basePosition = value->position,
            .registered = true
        };
        return true;
    }

    bool RegisterScreenFlash(ComponentRef<MaterialComponent> material) {
        MaterialComponent* value = material.TryGet();
        if (!value) {
            return false;
        }
        m_screenFlash = {
            .material = std::move(material),
            .baseAlpha = value->Material.BaseColor.w,
            .registered = true
        };
        value->Material.BaseColor.w = 0.0f;
        return true;
    }

    void PlayOneShotEffect(const EffectCueRequest& request) override {
        auto found = m_effectVoices.find(std::string(request.cueId));
        if (found == m_effectVoices.end() || !m_scene) {
            return;
        }

        EffectVoice* voice = SelectEffectVoice(found->second);
        if (!voice) {
            return;
        }

        TransformComponent* transform = voice->transform.TryGet();
        EffectComponent* effect = voice->effect.TryGet();
        if (!transform || !effect) {
            return;
        }

        transform->position.x = request.position.x;
        transform->position.z = request.position.y;
        const float scale = std::max(0.01f, request.intensity);
        transform->scale = Vector3(scale, scale, scale);

        if (effect->Playing) {
            effect->Stop(m_scene);
        }
        if (effect->Play(m_scene)) {
            voice->sceneToken = request.sceneToken;
            voice->playSerial = m_nextPlaySerial++;
        }
    }

    void PlayOneShotSound(const SoundCueRequest& request) override {
        auto found = m_audioVoices.find(std::string(request.cueId));
        if (found == m_audioVoices.end() || !m_scene ||
            !m_scene->manager || !m_scene->manager->audio) {
            return;
        }

        AudioVoice* voice = SelectAudioVoice(found->second);
        if (!voice) {
            return;
        }

        AudioComponent* audio = voice->component.TryGet();
        if (!audio) {
            return;
        }

        if (!audio->m_AudioData && !audio->FilePath.empty() &&
            m_scene->manager->resource) {
            audio->m_AudioData =
                m_scene->manager->resource->Load<AudioData>(audio->FilePath);
        }

        audio->Loop = false;
        audio->Volume = std::clamp(request.volume, 0.0f, 1.0f);
        audio->SetPitch(request.pitch);
        if (audio->Play(m_scene->manager->audio)) {
            voice->sceneToken = request.sceneToken;
            voice->playSerial = m_nextPlaySerial++;
        }
    }

    void PlayCameraShake(const CameraShakeRequest& request) override {
        if (!m_camera.registered || request.sceneToken == 0 ||
            request.durationSeconds <= 0.0f || request.amplitude <= 0.0f) {
            return;
        }
        m_activeShakes.push_back({
            .sceneToken = request.sceneToken,
            .durationSeconds = request.durationSeconds,
            .remainingSeconds = request.durationSeconds,
            .amplitude = request.amplitude,
            .frequency = std::max(1.0f, request.frequency),
            .phase = static_cast<float>(m_nextPlaySerial++ % 997u) * 0.137f
        });
    }

    void PlayScreenFlash(const ScreenFlashRequest& request) override {
        if (!m_screenFlash.registered || request.sceneToken == 0 ||
            request.durationSeconds <= 0.0f || request.intensity <= 0.0f) {
            return;
        }
        m_activeFlashes.push_back({
            .sceneToken = request.sceneToken,
            .durationSeconds = request.durationSeconds,
            .remainingSeconds = request.durationSeconds,
            .intensity = std::clamp(request.intensity, 0.0f, 1.0f)
        });
    }

    void PlayUiTween(const UiTweenRequest& request) override {
        auto found = m_uiTargets.find(std::string(request.targetId));
        if (found == m_uiTargets.end() || request.sceneToken == 0) {
            return;
        }

        UiTween& tween = found->second.tween;
        tween.sceneToken = request.sceneToken;
        tween.durationSeconds = std::max(0.001f, request.durationSeconds);
        tween.elapsedSeconds = 0.0f;
        tween.fromScale = request.fromScale;
        tween.toScale = request.toScale;
        tween.active = true;
        ApplyUiScale(found->second, request.fromScale);
    }

    void CancelAllForScene(SceneToken sceneToken) override {
        for (auto& [cueId, voices] : m_audioVoices) {
            (void)cueId;
            for (AudioVoice& voice : voices) {
                if (voice.sceneToken != sceneToken) {
                    continue;
                }
                if (AudioComponent* audio = voice.component.TryGet()) {
                    audio->Stop();
                }
                voice.sceneToken = 0;
            }
        }

        for (auto& [cueId, voices] : m_effectVoices) {
            (void)cueId;
            for (EffectVoice& voice : voices) {
                if (voice.sceneToken != sceneToken) {
                    continue;
                }
                if (EffectComponent* effect = voice.effect.TryGet(); effect && m_scene) {
                    effect->Stop(m_scene);
                }
                voice.sceneToken = 0;
            }
        }

        std::erase_if(
            m_activeShakes,
            [sceneToken](const ActiveShake& shake) {
                return shake.sceneToken == sceneToken;
            }
        );
        std::erase_if(
            m_activeFlashes,
            [sceneToken](const ActiveFlash& flash) {
                return flash.sceneToken == sceneToken;
            }
        );

        for (auto& [targetId, target] : m_uiTargets) {
            (void)targetId;
            if (target.tween.sceneToken == sceneToken) {
                target.tween = {};
                if (TransformComponent* transform = target.transform.TryGet()) {
                    transform->scale = target.baseScale;
                }
            }
        }

        RestoreCamera();
        RestoreFlash();
    }

    void Tick(float unscaledDeltaTime) {
        const float delta = std::max(0.0f, unscaledDeltaTime);
        UpdateUiTweens(delta);
        UpdateCameraShake(delta);
        UpdateScreenFlash(delta);
        ReleaseCompletedVoices();
    }

    std::size_t CountActiveAudioVoices() const {
        std::size_t active = 0;
        for (const auto& [cueId, voices] : m_audioVoices) {
            (void)cueId;
            active += static_cast<std::size_t>(std::count_if(
                voices.begin(),
                voices.end(),
                [](const AudioVoice& voice) {
                    const AudioComponent* audio = voice.component.TryGet();
                    return audio && audio->Playing;
                }
            ));
        }
        return active;
    }

    std::size_t CountActiveEffectVoices() const {
        std::size_t active = 0;
        for (const auto& [cueId, voices] : m_effectVoices) {
            (void)cueId;
            active += static_cast<std::size_t>(std::count_if(
                voices.begin(),
                voices.end(),
                [](const EffectVoice& voice) {
                    const EffectComponent* effect = voice.effect.TryGet();
                    return effect && effect->Playing;
                }
            ));
        }
        return active;
    }

private:
    struct AudioVoice {
        ComponentRef<AudioComponent> component;
        SceneToken sceneToken = 0;
        std::uint64_t playSerial = 0;
    };

    struct EffectVoice {
        ComponentRef<EffectComponent> effect;
        ComponentRef<TransformComponent> transform;
        SceneToken sceneToken = 0;
        std::uint64_t playSerial = 0;
    };

    struct UiTween {
        SceneToken sceneToken = 0;
        float durationSeconds = 0.0f;
        float elapsedSeconds = 0.0f;
        float fromScale = 1.0f;
        float toScale = 1.0f;
        bool active = false;
    };

    struct UiTarget {
        ComponentRef<TransformComponent> transform;
        Vector3 baseScale{1.0f, 1.0f, 1.0f};
        UiTween tween{};
    };

    struct CameraTarget {
        ComponentRef<TransformComponent> transform;
        Vector3 basePosition{};
        bool registered = false;
    };

    struct FlashTarget {
        ComponentRef<MaterialComponent> material;
        float baseAlpha = 1.0f;
        bool registered = false;
    };

    struct ActiveShake {
        SceneToken sceneToken = 0;
        float durationSeconds = 0.0f;
        float remainingSeconds = 0.0f;
        float amplitude = 0.0f;
        float frequency = 1.0f;
        float phase = 0.0f;
    };

    struct ActiveFlash {
        SceneToken sceneToken = 0;
        float durationSeconds = 0.0f;
        float remainingSeconds = 0.0f;
        float intensity = 0.0f;
    };

    static AudioVoice* SelectAudioVoice(std::vector<AudioVoice>& voices) {
        AudioVoice* oldest = nullptr;
        for (AudioVoice& voice : voices) {
            AudioComponent* audio = voice.component.TryGet();
            if (!audio) {
                continue;
            }
            audio->RefreshPlaybackState();
            if (!audio->Playing) {
                return &voice;
            }
            if (!oldest || voice.playSerial < oldest->playSerial) {
                oldest = &voice;
            }
        }
        if (oldest) {
            if (AudioComponent* audio = oldest->component.TryGet()) {
                audio->Stop();
            }
        }
        return oldest;
    }

    static EffectVoice* SelectEffectVoice(std::vector<EffectVoice>& voices) {
        EffectVoice* oldest = nullptr;
        for (EffectVoice& voice : voices) {
            EffectComponent* effect = voice.effect.TryGet();
            if (!effect) {
                continue;
            }
            if (!effect->Playing) {
                return &voice;
            }
            if (!oldest || voice.playSerial < oldest->playSerial) {
                oldest = &voice;
            }
        }
        return oldest;
    }

    static float EaseOutBack(float value) noexcept {
        const float t = std::clamp(value, 0.0f, 1.0f);
        constexpr float overshoot = 1.70158f;
        const float shifted = t - 1.0f;
        return 1.0f +
            (overshoot + 1.0f) * shifted * shifted * shifted +
            overshoot * shifted * shifted;
    }

    static void ApplyUiScale(UiTarget& target, float scalar) {
        if (TransformComponent* transform = target.transform.TryGet()) {
            transform->scale = Vector3(
                target.baseScale.x * scalar,
                target.baseScale.y * scalar,
                target.baseScale.z * scalar
            );
        }
    }

    void UpdateUiTweens(float deltaTime) {
        for (auto& [targetId, target] : m_uiTargets) {
            (void)targetId;
            UiTween& tween = target.tween;
            if (!tween.active) {
                continue;
            }
            tween.elapsedSeconds = std::min(
                tween.durationSeconds,
                tween.elapsedSeconds + deltaTime
            );
            const float normalized =
                tween.elapsedSeconds / tween.durationSeconds;
            const float eased = EaseOutBack(normalized);
            ApplyUiScale(
                target,
                std::lerp(tween.fromScale, tween.toScale, eased)
            );
            if (tween.elapsedSeconds >= tween.durationSeconds) {
                tween.active = false;
                ApplyUiScale(target, tween.toScale);
            }
        }
    }

    void UpdateCameraShake(float deltaTime) {
        if (!m_camera.registered) {
            return;
        }
        TransformComponent* camera = m_camera.transform.TryGet();
        if (!camera) {
            return;
        }

        Vector3 offset{};
        for (ActiveShake& shake : m_activeShakes) {
            shake.remainingSeconds = std::max(
                0.0f,
                shake.remainingSeconds - deltaTime
            );
            const float elapsed = shake.durationSeconds - shake.remainingSeconds;
            const float envelope = shake.durationSeconds > 0.0f
                ? shake.remainingSeconds / shake.durationSeconds
                : 0.0f;
            const float phase = elapsed * shake.frequency * 6.28318530718f + shake.phase;
            offset.x += std::sin(phase) * shake.amplitude * envelope;
            offset.y += std::cos(phase * 1.37f) * shake.amplitude * envelope;
        }
        std::erase_if(
            m_activeShakes,
            [](const ActiveShake& shake) {
                return shake.remainingSeconds <= 0.0f;
            }
        );

        camera->position = Vector3(
            m_camera.basePosition.x + offset.x,
            m_camera.basePosition.y + offset.y,
            m_camera.basePosition.z + offset.z
        );
    }

    void UpdateScreenFlash(float deltaTime) {
        if (!m_screenFlash.registered) {
            return;
        }
        MaterialComponent* material = m_screenFlash.material.TryGet();
        if (!material) {
            return;
        }

        float alpha = 0.0f;
        for (ActiveFlash& flash : m_activeFlashes) {
            flash.remainingSeconds = std::max(
                0.0f,
                flash.remainingSeconds - deltaTime
            );
            const float envelope = flash.durationSeconds > 0.0f
                ? flash.remainingSeconds / flash.durationSeconds
                : 0.0f;
            alpha = std::max(alpha, flash.intensity * envelope);
        }
        std::erase_if(
            m_activeFlashes,
            [](const ActiveFlash& flash) {
                return flash.remainingSeconds <= 0.0f;
            }
        );
        material->Material.BaseColor.w = std::clamp(alpha, 0.0f, 1.0f);
    }

    void ReleaseCompletedVoices() {
        for (auto& [cueId, voices] : m_audioVoices) {
            (void)cueId;
            for (AudioVoice& voice : voices) {
                if (AudioComponent* audio = voice.component.TryGet()) {
                    audio->RefreshPlaybackState();
                    if (!audio->Playing) {
                        voice.sceneToken = 0;
                    }
                }
            }
        }
        for (auto& [cueId, voices] : m_effectVoices) {
            (void)cueId;
            for (EffectVoice& voice : voices) {
                if (EffectComponent* effect = voice.effect.TryGet(); effect && !effect->Playing) {
                    voice.sceneToken = 0;
                }
            }
        }
    }

    void RestoreCamera() {
        if (m_camera.registered) {
            if (TransformComponent* transform = m_camera.transform.TryGet()) {
                transform->position = m_camera.basePosition;
            }
        }
    }

    void RestoreFlash() {
        if (m_screenFlash.registered) {
            if (MaterialComponent* material = m_screenFlash.material.TryGet()) {
                material->Material.BaseColor.w = 0.0f;
            }
        }
    }

    SceneContext* m_scene = nullptr;
    std::unordered_map<std::string, std::vector<AudioVoice>> m_audioVoices;
    std::unordered_map<std::string, std::vector<EffectVoice>> m_effectVoices;
    std::unordered_map<std::string, UiTarget> m_uiTargets;
    CameraTarget m_camera{};
    FlashTarget m_screenFlash{};
    std::vector<ActiveShake> m_activeShakes;
    std::vector<ActiveFlash> m_activeFlashes;
    std::uint64_t m_nextPlaySerial = 1;
};

} // namespace MiniGameCollection::Presentation
