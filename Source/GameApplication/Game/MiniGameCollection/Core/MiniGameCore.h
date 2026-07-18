#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace MiniGameCollection {

enum class MiniGameId : std::uint8_t {
    PresentationSpike,
    ColorTerritory,
    SheepRoundup,
    Backshot
};

enum class MiniGameState : std::uint8_t {
    Loading,
    Introduction,
    Countdown,
    Playing,
    Finishing,
    Result,
    Transition
};

enum class TransitionRequest : std::uint8_t {
    None,
    Retry,
    Selection,
    NextGame
};

using PlayerId = std::uint8_t;
using SceneToken = std::uint64_t;

inline constexpr PlayerId InvalidPlayerId = std::numeric_limits<PlayerId>::max();

struct PlayerResult {
    PlayerId playerId = InvalidPlayerId;
    int score = 0;
    bool eliminated = false;
    float finishTimeSeconds = 0.0f;
    std::uint32_t rank = 0;
};

struct MiniGameResult {
    MiniGameId gameId = MiniGameId::PresentationSpike;
    std::vector<PlayerResult> players;
    bool isTie = false;
    int winningScore = 0;

    void RebuildRanking() {
        std::stable_sort(
            players.begin(),
            players.end(),
            [](const PlayerResult& lhs, const PlayerResult& rhs) {
                if (lhs.eliminated != rhs.eliminated) {
                    return !lhs.eliminated;
                }
                if (lhs.score != rhs.score) {
                    return lhs.score > rhs.score;
                }
                if (lhs.finishTimeSeconds != rhs.finishTimeSeconds) {
                    return lhs.finishTimeSeconds < rhs.finishTimeSeconds;
                }
                return lhs.playerId < rhs.playerId;
            }
        );

        if (players.empty()) {
            isTie = false;
            winningScore = 0;
            return;
        }

        std::uint32_t currentRank = 1;
        players.front().rank = currentRank;
        for (std::size_t index = 1; index < players.size(); ++index) {
            const PlayerResult& previous = players[index - 1];
            PlayerResult& current = players[index];
            const bool samePlacement =
                previous.eliminated == current.eliminated &&
                previous.score == current.score &&
                previous.finishTimeSeconds == current.finishTimeSeconds;
            if (!samePlacement) {
                currentRank = static_cast<std::uint32_t>(index + 1);
            }
            current.rank = currentRank;
        }

        winningScore = players.front().score;
        isTie = players.size() > 1 && players[0].rank == players[1].rank;
    }
};

class IMiniGameRules {
public:
    virtual ~IMiniGameRules() = default;

    virtual void Prepare() = 0;
    virtual void StartGame() = 0;
    virtual void Tick(float deltaTime) = 0;
    virtual bool IsFinished() const = 0;
    virtual MiniGameResult BuildResult() const = 0;
    virtual void Shutdown() = 0;
};

struct CpuDifficultyProfile {
    float decisionIntervalSeconds = 0.35f;
    float informationRadius = 8.0f;
    float targetHoldSeconds = 0.9f;
    float mistakeProbability = 0.08f;
    float predictionSeconds = 0.2f;
    float lateGameAggression = 0.6f;

    static CpuDifficultyProfile Easy() {
        return {
            .decisionIntervalSeconds = 0.55f,
            .informationRadius = 5.0f,
            .targetHoldSeconds = 1.15f,
            .mistakeProbability = 0.18f,
            .predictionSeconds = 0.05f,
            .lateGameAggression = 0.35f
        };
    }

    static CpuDifficultyProfile Normal() {
        return {};
    }

    static CpuDifficultyProfile Hard() {
        return {
            .decisionIntervalSeconds = 0.24f,
            .informationRadius = 11.0f,
            .targetHoldSeconds = 0.75f,
            .mistakeProbability = 0.04f,
            .predictionSeconds = 0.35f,
            .lateGameAggression = 0.85f
        };
    }
};

class MiniGameSession {
public:
    void BeginLoading(MiniGameId gameId, SceneToken sceneToken) {
        if (sceneToken == 0) {
            throw std::invalid_argument("MiniGameSession requires a non-zero scene token");
        }

        m_gameId = gameId;
        m_sceneToken = sceneToken;
        m_state = MiniGameState::Loading;
        m_transitionRequest = TransitionRequest::None;
        m_rules = nullptr;
        m_result.reset();
        m_stateElapsedSeconds = 0.0f;
        m_gameElapsedSeconds = 0.0f;
        m_countdownRemainingSeconds = 0.0f;
        m_finishDelayRemainingSeconds = 0.0f;
        m_inputEnabled = false;
    }

    void AttachRules(IMiniGameRules& rules) {
        RequireState(MiniGameState::Loading);
        m_rules = &rules;
        m_rules->Prepare();
    }

    void EnterIntroduction() {
        RequireRules();
        RequireState(MiniGameState::Loading);
        ChangeState(MiniGameState::Introduction);
    }

    void BeginCountdown(float durationSeconds) {
        RequireState(MiniGameState::Introduction);
        m_countdownRemainingSeconds = std::max(0.0f, durationSeconds);
        ChangeState(MiniGameState::Countdown);
    }

    void Tick(float scaledDeltaTime, float unscaledDeltaTime) {
        const float scaled = std::max(0.0f, scaledDeltaTime);
        const float unscaled = std::max(0.0f, unscaledDeltaTime);
        m_stateElapsedSeconds += unscaled;

        switch (m_state) {
        case MiniGameState::Countdown:
            m_countdownRemainingSeconds =
                std::max(0.0f, m_countdownRemainingSeconds - unscaled);
            if (m_countdownRemainingSeconds <= 0.0f) {
                RequireRules();
                m_rules->StartGame();
                m_inputEnabled = true;
                ChangeState(MiniGameState::Playing);
            }
            break;

        case MiniGameState::Playing:
            RequireRules();
            m_gameElapsedSeconds += scaled;
            m_rules->Tick(scaled);
            if (m_rules->IsFinished()) {
                BeginFinishing(0.65f);
            }
            break;

        case MiniGameState::Finishing:
            m_finishDelayRemainingSeconds =
                std::max(0.0f, m_finishDelayRemainingSeconds - unscaled);
            if (m_finishDelayRemainingSeconds <= 0.0f) {
                RequireRules();
                m_result = m_rules->BuildResult();
                m_result->RebuildRanking();
                ChangeState(MiniGameState::Result);
            }
            break;

        default:
            break;
        }
    }

    void ForceFinish(float presentationDelaySeconds = 0.65f) {
        if (m_state != MiniGameState::Playing) {
            return;
        }
        BeginFinishing(presentationDelaySeconds);
    }

    void RequestRetry() {
        RequireState(MiniGameState::Result);
        m_transitionRequest = TransitionRequest::Retry;
        ChangeState(MiniGameState::Transition);
    }

    void RequestSelection() {
        RequireState(MiniGameState::Result);
        m_transitionRequest = TransitionRequest::Selection;
        ChangeState(MiniGameState::Transition);
    }

    void RequestNextGame() {
        RequireState(MiniGameState::Result);
        m_transitionRequest = TransitionRequest::NextGame;
        ChangeState(MiniGameState::Transition);
    }

    void ShutdownRules() {
        if (m_rules != nullptr) {
            m_rules->Shutdown();
            m_rules = nullptr;
        }
        m_inputEnabled = false;
    }

    MiniGameId GetGameId() const noexcept { return m_gameId; }
    MiniGameState GetState() const noexcept { return m_state; }
    TransitionRequest GetTransitionRequest() const noexcept { return m_transitionRequest; }
    SceneToken GetSceneToken() const noexcept { return m_sceneToken; }
    float GetStateElapsedSeconds() const noexcept { return m_stateElapsedSeconds; }
    float GetGameElapsedSeconds() const noexcept { return m_gameElapsedSeconds; }
    float GetCountdownRemainingSeconds() const noexcept { return m_countdownRemainingSeconds; }
    bool IsInputEnabled() const noexcept { return m_inputEnabled; }
    bool HasResult() const noexcept { return m_result.has_value(); }

    const MiniGameResult* TryGetResult() const noexcept {
        return m_result ? &*m_result : nullptr;
    }

private:
    void BeginFinishing(float delaySeconds) {
        m_inputEnabled = false;
        m_finishDelayRemainingSeconds = std::max(0.0f, delaySeconds);
        ChangeState(MiniGameState::Finishing);
    }

    void ChangeState(MiniGameState next) {
        m_state = next;
        m_stateElapsedSeconds = 0.0f;
    }

    void RequireRules() const {
        if (m_rules == nullptr) {
            throw std::logic_error("MiniGameSession has no attached rules");
        }
    }

    void RequireState(MiniGameState expected) const {
        if (m_state != expected) {
            throw std::logic_error("MiniGameSession state transition is invalid");
        }
    }

    MiniGameId m_gameId = MiniGameId::PresentationSpike;
    MiniGameState m_state = MiniGameState::Loading;
    TransitionRequest m_transitionRequest = TransitionRequest::None;
    SceneToken m_sceneToken = 0;
    IMiniGameRules* m_rules = nullptr;
    std::optional<MiniGameResult> m_result;
    float m_stateElapsedSeconds = 0.0f;
    float m_gameElapsedSeconds = 0.0f;
    float m_countdownRemainingSeconds = 0.0f;
    float m_finishDelayRemainingSeconds = 0.0f;
    bool m_inputEnabled = false;
};

enum class PresentationEventType : std::uint8_t {
    Countdown3,
    Countdown2,
    Countdown1,
    Go,
    Success,
    NearMiss,
    Failure,
    ResultReveal,
    EnableRetry
};

struct PresentationEvent {
    PresentationEventType type = PresentationEventType::Countdown3;
    SceneToken sceneToken = 0;
    float triggerTimeSeconds = 0.0f;
    float intensity = 1.0f;
    std::uint64_t sequence = 0;
};

class PresentationTimeline {
public:
    void Reset(SceneToken sceneToken) {
        m_sceneToken = sceneToken;
        m_elapsedSeconds = 0.0f;
        m_nextSequence = 1;
        m_pending.clear();
    }

    void Schedule(
        PresentationEventType type,
        float delaySeconds,
        float intensity = 1.0f
    ) {
        if (m_sceneToken == 0) {
            throw std::logic_error("PresentationTimeline requires Reset with a scene token");
        }

        m_pending.push_back({
            .type = type,
            .sceneToken = m_sceneToken,
            .triggerTimeSeconds = m_elapsedSeconds + std::max(0.0f, delaySeconds),
            .intensity = std::max(0.0f, intensity),
            .sequence = m_nextSequence++
        });

        std::stable_sort(
            m_pending.begin(),
            m_pending.end(),
            [](const PresentationEvent& lhs, const PresentationEvent& rhs) {
                if (lhs.triggerTimeSeconds != rhs.triggerTimeSeconds) {
                    return lhs.triggerTimeSeconds < rhs.triggerTimeSeconds;
                }
                return lhs.sequence < rhs.sequence;
            }
        );
    }

    std::vector<PresentationEvent> Tick(float unscaledDeltaTime) {
        m_elapsedSeconds += std::max(0.0f, unscaledDeltaTime);
        std::vector<PresentationEvent> fired;

        auto firstPending = std::find_if(
            m_pending.begin(),
            m_pending.end(),
            [this](const PresentationEvent& event) {
                return event.triggerTimeSeconds > m_elapsedSeconds;
            }
        );

        fired.assign(m_pending.begin(), firstPending);
        m_pending.erase(m_pending.begin(), firstPending);
        return fired;
    }

    void CancelAllForScene(SceneToken sceneToken) {
        std::erase_if(
            m_pending,
            [sceneToken](const PresentationEvent& event) {
                return event.sceneToken == sceneToken;
            }
        );
        if (m_sceneToken == sceneToken) {
            m_sceneToken = 0;
        }
    }

    std::size_t PendingCount() const noexcept { return m_pending.size(); }
    float GetElapsedSeconds() const noexcept { return m_elapsedSeconds; }

private:
    SceneToken m_sceneToken = 0;
    float m_elapsedSeconds = 0.0f;
    std::uint64_t m_nextSequence = 1;
    std::vector<PresentationEvent> m_pending;
};

} // namespace MiniGameCollection
