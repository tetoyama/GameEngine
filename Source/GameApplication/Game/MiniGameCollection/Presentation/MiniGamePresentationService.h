#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"
#include "Game/MiniGameCollection/Core/MiniGameMath.h"

#include <algorithm>
#include <string_view>

namespace MiniGameCollection::Presentation {

struct EffectCueRequest {
    std::string_view cueId;
    Vec2 position{};
    float intensity = 1.0f;
    SceneToken sceneToken = 0;
};

struct SoundCueRequest {
    std::string_view cueId;
    float volume = 1.0f;
    float pitch = 1.0f;
    SceneToken sceneToken = 0;
};

struct CameraShakeRequest {
    float durationSeconds = 0.0f;
    float amplitude = 0.0f;
    float frequency = 0.0f;
    SceneToken sceneToken = 0;
};

struct ScreenFlashRequest {
    float durationSeconds = 0.0f;
    float intensity = 0.0f;
    SceneToken sceneToken = 0;
};

struct UiTweenRequest {
    std::string_view targetId;
    float durationSeconds = 0.0f;
    float fromScale = 1.0f;
    float toScale = 1.0f;
    SceneToken sceneToken = 0;
};

class IMiniGamePresentationBackend {
public:
    virtual ~IMiniGamePresentationBackend() = default;

    virtual void PlayOneShotEffect(const EffectCueRequest& request) = 0;
    virtual void PlayOneShotSound(const SoundCueRequest& request) = 0;
    virtual void PlayCameraShake(const CameraShakeRequest& request) = 0;
    virtual void PlayScreenFlash(const ScreenFlashRequest& request) = 0;
    virtual void PlayUiTween(const UiTweenRequest& request) = 0;
    virtual void CancelAllForScene(SceneToken sceneToken) = 0;
};

class MiniGamePresentationService {
public:
    explicit MiniGamePresentationService(IMiniGamePresentationBackend& backend)
        : m_backend(backend) {
    }

    void BeginScene(SceneToken sceneToken) {
        if (m_sceneToken != 0 && m_sceneToken != sceneToken) {
            CancelAllForScene(m_sceneToken);
        }
        m_sceneToken = sceneToken;
        m_timeline.Reset(sceneToken);
    }

    void PlayCountdown() {
        RequireScene();
        m_timeline.Schedule(PresentationEventType::Countdown3, 0.0f);
        m_timeline.Schedule(PresentationEventType::Countdown2, 1.0f);
        m_timeline.Schedule(PresentationEventType::Countdown1, 2.0f);
        m_timeline.Schedule(PresentationEventType::Go, 3.0f, 1.15f);
    }

    void PlaySuccess(float delaySeconds = 0.0f, float intensity = 1.0f) {
        RequireScene();
        m_timeline.Schedule(
            PresentationEventType::Success,
            delaySeconds,
            std::max(0.0f, intensity)
        );
    }

    void PlayNearMiss(float delaySeconds = 0.0f) {
        RequireScene();
        m_timeline.Schedule(PresentationEventType::NearMiss, delaySeconds, 0.65f);
    }

    void PlayFailure(float delaySeconds = 0.0f) {
        RequireScene();
        m_timeline.Schedule(PresentationEventType::Failure, delaySeconds, 0.85f);
    }

    void PlayResult(float delaySeconds = 0.55f) {
        RequireScene();
        m_timeline.Schedule(PresentationEventType::ResultReveal, delaySeconds);
        m_timeline.Schedule(PresentationEventType::EnableRetry, delaySeconds + 0.65f);
    }

    void Tick(float unscaledDeltaTime) {
        for (const PresentationEvent& event : m_timeline.Tick(unscaledDeltaTime)) {
            Dispatch(event);
        }
    }

    void CancelAllForScene(SceneToken sceneToken) {
        m_timeline.CancelAllForScene(sceneToken);
        m_backend.CancelAllForScene(sceneToken);
        if (m_sceneToken == sceneToken) {
            m_sceneToken = 0;
        }
    }

    SceneToken GetSceneToken() const noexcept { return m_sceneToken; }
    std::size_t PendingCueCount() const noexcept { return m_timeline.PendingCount(); }

private:
    void Dispatch(const PresentationEvent& event) {
        switch (event.type) {
        case PresentationEventType::Countdown3:
        case PresentationEventType::Countdown2:
        case PresentationEventType::Countdown1:
            m_backend.PlayOneShotSound({
                .cueId = "countdown",
                .volume = 0.9f,
                .pitch = CountdownPitch(event.type),
                .sceneToken = event.sceneToken
            });
            m_backend.PlayUiTween({
                .targetId = "countdown",
                .durationSeconds = 0.28f,
                .fromScale = 1.45f,
                .toScale = 1.0f,
                .sceneToken = event.sceneToken
            });
            break;

        case PresentationEventType::Go:
            m_backend.PlayOneShotSound({
                .cueId = "go",
                .volume = 1.0f,
                .pitch = 1.0f,
                .sceneToken = event.sceneToken
            });
            m_backend.PlayUiTween({
                .targetId = "go",
                .durationSeconds = 0.35f,
                .fromScale = 1.75f,
                .toScale = 1.0f,
                .sceneToken = event.sceneToken
            });
            m_backend.PlayCameraShake({
                .durationSeconds = 0.18f,
                .amplitude = 0.14f,
                .frequency = 18.0f,
                .sceneToken = event.sceneToken
            });
            break;

        case PresentationEventType::Success:
            m_backend.PlayOneShotEffect({
                .cueId = "success",
                .position = {},
                .intensity = event.intensity,
                .sceneToken = event.sceneToken
            });
            m_backend.PlayOneShotSound({
                .cueId = "success",
                .volume = 1.0f,
                .pitch = 1.0f + 0.08f * event.intensity,
                .sceneToken = event.sceneToken
            });
            m_backend.PlayCameraShake({
                .durationSeconds = 0.22f,
                .amplitude = 0.2f * event.intensity,
                .frequency = 22.0f,
                .sceneToken = event.sceneToken
            });
            m_backend.PlayScreenFlash({
                .durationSeconds = 0.12f,
                .intensity = 0.55f * event.intensity,
                .sceneToken = event.sceneToken
            });
            m_backend.PlayUiTween({
                .targetId = "outcome",
                .durationSeconds = 0.32f,
                .fromScale = 1.55f,
                .toScale = 1.0f,
                .sceneToken = event.sceneToken
            });
            break;

        case PresentationEventType::NearMiss:
            m_backend.PlayOneShotSound({
                .cueId = "near_miss",
                .volume = 0.9f,
                .pitch = 1.0f,
                .sceneToken = event.sceneToken
            });
            m_backend.PlayCameraShake({
                .durationSeconds = 0.14f,
                .amplitude = 0.08f,
                .frequency = 16.0f,
                .sceneToken = event.sceneToken
            });
            m_backend.PlayUiTween({
                .targetId = "outcome",
                .durationSeconds = 0.3f,
                .fromScale = 1.35f,
                .toScale = 1.0f,
                .sceneToken = event.sceneToken
            });
            break;

        case PresentationEventType::Failure:
            m_backend.PlayOneShotSound({
                .cueId = "failure",
                .volume = 1.0f,
                .pitch = 0.92f,
                .sceneToken = event.sceneToken
            });
            m_backend.PlayCameraShake({
                .durationSeconds = 0.28f,
                .amplitude = 0.18f,
                .frequency = 14.0f,
                .sceneToken = event.sceneToken
            });
            m_backend.PlayScreenFlash({
                .durationSeconds = 0.2f,
                .intensity = 0.28f,
                .sceneToken = event.sceneToken
            });
            m_backend.PlayUiTween({
                .targetId = "outcome",
                .durationSeconds = 0.38f,
                .fromScale = 0.72f,
                .toScale = 1.0f,
                .sceneToken = event.sceneToken
            });
            break;

        case PresentationEventType::ResultReveal:
            m_backend.PlayOneShotSound({
                .cueId = "result",
                .volume = 1.0f,
                .pitch = 1.0f,
                .sceneToken = event.sceneToken
            });
            m_backend.PlayUiTween({
                .targetId = "result",
                .durationSeconds = 0.45f,
                .fromScale = 0.82f,
                .toScale = 1.0f,
                .sceneToken = event.sceneToken
            });
            break;

        case PresentationEventType::EnableRetry:
            m_backend.PlayUiTween({
                .targetId = "retry",
                .durationSeconds = 0.25f,
                .fromScale = 0.85f,
                .toScale = 1.0f,
                .sceneToken = event.sceneToken
            });
            break;
        }
    }

    static float CountdownPitch(PresentationEventType type) noexcept {
        switch (type) {
        case PresentationEventType::Countdown3: return 0.92f;
        case PresentationEventType::Countdown2: return 1.0f;
        case PresentationEventType::Countdown1: return 1.08f;
        default: return 1.0f;
        }
    }

    void RequireScene() const {
        if (m_sceneToken == 0) {
            throw std::logic_error("MiniGamePresentationService has no active scene token");
        }
    }

    IMiniGamePresentationBackend& m_backend;
    PresentationTimeline m_timeline;
    SceneToken m_sceneToken = 0;
};

} // namespace MiniGameCollection::Presentation
