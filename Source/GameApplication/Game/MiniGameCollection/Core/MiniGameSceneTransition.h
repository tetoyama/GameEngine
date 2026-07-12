#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCore.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace MiniGameCollection {

enum class SceneTransitionStep : std::uint8_t {
    Idle,
    LockInput,
    WaitForPresentation,
    CancelPresentation,
    ShutdownRules,
    UnloadGameScene,
    LoadTargetScene,
    Complete,
    Failed
};

struct SceneTransitionRequest {
    SceneToken sourceSceneToken = 0;
    std::string sourceSceneName;
    // Selectionへ戻る場合は空文字を許可し、MiniGame SceneのUnloadだけで完了する。
    std::string targetScenePath;
    TransitionRequest reason = TransitionRequest::None;
    float presentationWaitSeconds = 0.0f;
};

class IMiniGameSceneTransitionBackend {
public:
    virtual ~IMiniGameSceneTransitionBackend() = default;

    virtual void SetGameplayInputEnabled(bool enabled) = 0;
    virtual void CancelPresentation(SceneToken sceneToken) = 0;
    virtual void ShutdownRules(SceneToken sceneToken) = 0;
    virtual bool RequestUnloadScene(const std::string& sceneName) = 0;
    virtual bool IsSceneUnloaded(const std::string& sceneName) const = 0;
    virtual bool RequestLoadScene(const std::string& scenePath) = 0;
    virtual bool IsSceneLoaded(const std::string& scenePath) const = 0;
};

class MiniGameSceneTransition {
public:
    explicit MiniGameSceneTransition(IMiniGameSceneTransitionBackend& backend)
        : m_backend(backend) {
    }

    void Begin(SceneTransitionRequest request) {
        if (m_step != SceneTransitionStep::Idle &&
            m_step != SceneTransitionStep::Complete &&
            m_step != SceneTransitionStep::Failed) {
            throw std::logic_error("A mini-game scene transition is already active");
        }
        if (request.sourceSceneToken == 0 || request.sourceSceneName.empty()) {
            throw std::invalid_argument("Scene transition requires a source scene");
        }
        if (request.targetScenePath.empty() &&
            request.reason != TransitionRequest::Selection) {
            throw std::invalid_argument(
                "Only return-to-selection may omit the target scene path"
            );
        }

        m_request = std::move(request);
        m_waitRemainingSeconds = std::max(
            0.0f,
            m_request.presentationWaitSeconds
        );
        m_failureReason.clear();
        m_unloadRequested = false;
        m_loadRequested = false;
        m_step = SceneTransitionStep::LockInput;
    }

    void Tick(float unscaledDeltaTime) {
        const float delta = std::max(0.0f, unscaledDeltaTime);

        switch (m_step) {
        case SceneTransitionStep::Idle:
        case SceneTransitionStep::Complete:
        case SceneTransitionStep::Failed:
            return;

        case SceneTransitionStep::LockInput:
            m_backend.SetGameplayInputEnabled(false);
            m_step = SceneTransitionStep::WaitForPresentation;
            return;

        case SceneTransitionStep::WaitForPresentation:
            m_waitRemainingSeconds = std::max(
                0.0f,
                m_waitRemainingSeconds - delta
            );
            if (m_waitRemainingSeconds <= 0.0f) {
                m_step = SceneTransitionStep::CancelPresentation;
            }
            return;

        case SceneTransitionStep::CancelPresentation:
            m_backend.CancelPresentation(m_request.sourceSceneToken);
            m_step = SceneTransitionStep::ShutdownRules;
            return;

        case SceneTransitionStep::ShutdownRules:
            m_backend.ShutdownRules(m_request.sourceSceneToken);
            m_step = SceneTransitionStep::UnloadGameScene;
            return;

        case SceneTransitionStep::UnloadGameScene:
            if (!m_unloadRequested) {
                m_unloadRequested = true;
                if (!m_backend.RequestUnloadScene(m_request.sourceSceneName)) {
                    Fail("Failed to request mini-game scene unload");
                    return;
                }
            }
            if (m_backend.IsSceneUnloaded(m_request.sourceSceneName)) {
                m_step = m_request.targetScenePath.empty()
                    ? SceneTransitionStep::Complete
                    : SceneTransitionStep::LoadTargetScene;
            }
            return;

        case SceneTransitionStep::LoadTargetScene:
            if (!m_loadRequested) {
                m_loadRequested = true;
                if (!m_backend.RequestLoadScene(m_request.targetScenePath)) {
                    Fail("Failed to request target scene load");
                    return;
                }
            }
            if (m_backend.IsSceneLoaded(m_request.targetScenePath)) {
                m_step = SceneTransitionStep::Complete;
            }
            return;
        }
    }

    void Reset() noexcept {
        m_request = {};
        m_step = SceneTransitionStep::Idle;
        m_waitRemainingSeconds = 0.0f;
        m_unloadRequested = false;
        m_loadRequested = false;
        m_failureReason.clear();
    }

    SceneTransitionStep GetStep() const noexcept { return m_step; }
    bool IsActive() const noexcept {
        return m_step != SceneTransitionStep::Idle &&
            m_step != SceneTransitionStep::Complete &&
            m_step != SceneTransitionStep::Failed;
    }
    bool IsComplete() const noexcept { return m_step == SceneTransitionStep::Complete; }
    bool HasFailed() const noexcept { return m_step == SceneTransitionStep::Failed; }
    const std::string& GetFailureReason() const noexcept { return m_failureReason; }
    const SceneTransitionRequest& GetRequest() const noexcept { return m_request; }

private:
    void Fail(std::string reason) {
        m_failureReason = std::move(reason);
        m_step = SceneTransitionStep::Failed;
    }

    IMiniGameSceneTransitionBackend& m_backend;
    SceneTransitionRequest m_request{};
    SceneTransitionStep m_step = SceneTransitionStep::Idle;
    float m_waitRemainingSeconds = 0.0f;
    bool m_unloadRequested = false;
    bool m_loadRequested = false;
    std::string m_failureReason;
};

} // namespace MiniGameCollection
