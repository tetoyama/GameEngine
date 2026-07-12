#pragma once

#include "Game/MiniGameCollection/Presentation/MiniGamePresentationService.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace MiniGameCollection::Presentation {

enum class PresentationSpikePhase : std::uint8_t {
    Countdown,
    InputWindow,
    Outcome,
    Result,
    RetryReady
};

enum class PresentationSpikeOutcome : std::uint8_t {
    Pending,
    Success,
    NearMiss,
    Failure
};

struct PresentationSpikeConfig {
    float countdownSeconds = 3.0f;
    float idealInputAfterGoSeconds = 0.65f;
    float inputTimeoutAfterGoSeconds = 1.5f;
    float successToleranceSeconds = 0.12f;
    float nearMissToleranceSeconds = 0.34f;
    float outcomeDisplaySeconds = 0.6f;
    float resultToRetrySeconds = 1.2f;
};

class PresentationSpikeModel {
public:
    PresentationSpikeModel(
        MiniGamePresentationService& presentation,
        PresentationSpikeConfig config = {}
    )
        : m_presentation(presentation),
          m_config(config) {
    }

    void Begin(SceneToken sceneToken) {
        if (sceneToken == 0) {
            throw std::invalid_argument("PresentationSpikeModel requires a scene token");
        }

        m_sceneToken = sceneToken;
        m_phase = PresentationSpikePhase::Countdown;
        m_outcome = PresentationSpikeOutcome::Pending;
        m_phaseElapsedSeconds = 0.0f;
        m_totalElapsedSeconds = 0.0f;
        m_inputAccepted = false;
        m_presentation.BeginScene(sceneToken);
        m_presentation.PlayCountdown();
    }

    void Tick(float unscaledDeltaTime) {
        const float delta = std::max(0.0f, unscaledDeltaTime);
        m_phaseElapsedSeconds += delta;
        m_totalElapsedSeconds += delta;
        m_presentation.Tick(delta);

        switch (m_phase) {
        case PresentationSpikePhase::Countdown:
            if (m_phaseElapsedSeconds >= m_config.countdownSeconds) {
                ChangePhase(PresentationSpikePhase::InputWindow);
            }
            break;

        case PresentationSpikePhase::InputWindow:
            if (m_phaseElapsedSeconds >= m_config.inputTimeoutAfterGoSeconds) {
                ResolveOutcome(PresentationSpikeOutcome::Failure);
            }
            break;

        case PresentationSpikePhase::Outcome:
            if (m_phaseElapsedSeconds >= m_config.outcomeDisplaySeconds) {
                ChangePhase(PresentationSpikePhase::Result);
            }
            break;

        case PresentationSpikePhase::Result:
            if (m_phaseElapsedSeconds >= m_config.resultToRetrySeconds) {
                ChangePhase(PresentationSpikePhase::RetryReady);
            }
            break;

        case PresentationSpikePhase::RetryReady:
            break;
        }
    }

    bool SubmitInput() {
        if (m_phase != PresentationSpikePhase::InputWindow || m_inputAccepted) {
            return false;
        }

        m_inputAccepted = true;
        const float error = std::abs(
            m_phaseElapsedSeconds - m_config.idealInputAfterGoSeconds
        );

        if (error <= m_config.successToleranceSeconds) {
            ResolveOutcome(PresentationSpikeOutcome::Success);
        } else if (error <= m_config.nearMissToleranceSeconds) {
            ResolveOutcome(PresentationSpikeOutcome::NearMiss);
        } else {
            ResolveOutcome(PresentationSpikeOutcome::Failure);
        }
        return true;
    }

    bool Retry() {
        if (m_phase != PresentationSpikePhase::RetryReady || m_sceneToken == 0) {
            return false;
        }

        const SceneToken token = m_sceneToken;
        m_presentation.CancelAllForScene(token);
        Begin(token);
        ++m_retryCount;
        return true;
    }

    void Shutdown() {
        if (m_sceneToken != 0) {
            m_presentation.CancelAllForScene(m_sceneToken);
        }
        m_sceneToken = 0;
        m_outcome = PresentationSpikeOutcome::Pending;
        m_phase = PresentationSpikePhase::Countdown;
        m_phaseElapsedSeconds = 0.0f;
        m_totalElapsedSeconds = 0.0f;
        m_inputAccepted = false;
    }

    PresentationSpikePhase GetPhase() const noexcept { return m_phase; }
    PresentationSpikeOutcome GetOutcome() const noexcept { return m_outcome; }
    SceneToken GetSceneToken() const noexcept { return m_sceneToken; }
    float GetPhaseElapsedSeconds() const noexcept { return m_phaseElapsedSeconds; }
    float GetTotalElapsedSeconds() const noexcept { return m_totalElapsedSeconds; }
    std::uint32_t GetRetryCount() const noexcept { return m_retryCount; }

private:
    void ResolveOutcome(PresentationSpikeOutcome outcome) {
        if (m_phase != PresentationSpikePhase::InputWindow ||
            m_outcome != PresentationSpikeOutcome::Pending) {
            return;
        }

        m_outcome = outcome;
        switch (outcome) {
        case PresentationSpikeOutcome::Success:
            m_presentation.PlaySuccess(0.0f, 1.25f);
            break;
        case PresentationSpikeOutcome::NearMiss:
            m_presentation.PlayNearMiss();
            break;
        case PresentationSpikeOutcome::Failure:
            m_presentation.PlayFailure();
            break;
        case PresentationSpikeOutcome::Pending:
            return;
        }
        m_presentation.PlayResult();
        ChangePhase(PresentationSpikePhase::Outcome);
    }

    void ChangePhase(PresentationSpikePhase phase) noexcept {
        m_phase = phase;
        m_phaseElapsedSeconds = 0.0f;
    }

    MiniGamePresentationService& m_presentation;
    PresentationSpikeConfig m_config{};
    SceneToken m_sceneToken = 0;
    PresentationSpikePhase m_phase = PresentationSpikePhase::Countdown;
    PresentationSpikeOutcome m_outcome = PresentationSpikeOutcome::Pending;
    float m_phaseElapsedSeconds = 0.0f;
    float m_totalElapsedSeconds = 0.0f;
    bool m_inputAccepted = false;
    std::uint32_t m_retryCount = 0;
};

} // namespace MiniGameCollection::Presentation
