#pragma once

#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace MiniGameCollection::Runtime {

class PresentationSpikeRuntime final : public MiniGameRuntimeScriptBase {
public:
    PresentationSpikeRuntime()
        : MiniGameRuntimeScriptBase("PresentationSpikeRuntime") {
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, -50);
        SetExecutionOrder(SystemTaskDomain::Render, SystemPhase::Late, 100);
    }

private:
    enum class Phase : std::uint8_t {
        Countdown,
        InputWindow,
        Outcome,
        Result
    };

    enum class Outcome : std::uint8_t {
        Pending,
        Success,
        NearMiss,
        Failure
    };

    static constexpr const char* ScenePath =
        "Asset/Game/MiniGameCollection/Scene/PresentationTest/PresentationSpike.scene";
    static constexpr float CountdownSeconds = 3.0f;
    static constexpr float IdealInputSeconds = 0.65f;
    static constexpr float InputTimeoutSeconds = 1.5f;
    static constexpr float SuccessToleranceSeconds = 0.12f;
    static constexpr float NearMissToleranceSeconds = 0.34f;
    static constexpr float OutcomeDisplaySeconds = 0.75f;

    void OnStart() override {
        m_sceneToken = GetRuntimeSceneToken();
        QueueSpikeVisuals();
        MiniGameRuntimeMailbox::RegisterRulesShutdown(
            m_sceneToken,
            [this]() { ShutdownSpike(); }
        );
        BeginAttempt();
    }

    void OnUpdate(float dt) override {
        if (m_shutdown || m_transitionSubmitted) {
            return;
        }

        const float delta = std::max(0.0f, dt);
        m_phaseElapsedSeconds += delta;
        m_totalElapsedSeconds += delta;

        switch (m_phase) {
        case Phase::Countdown:
            if (m_phaseElapsedSeconds >= CountdownSeconds) {
                ChangePhase(Phase::InputWindow);
            }
            break;

        case Phase::InputWindow:
            UpdateMarker(delta);
            if (GetKeyDown(VK_SPACE)) {
                ResolveInput();
            } else if (m_phaseElapsedSeconds >= InputTimeoutSeconds) {
                ResolveOutcome(Outcome::Failure);
            }
            break;

        case Phase::Outcome:
            if (m_phaseElapsedSeconds >= OutcomeDisplaySeconds) {
                ChangePhase(Phase::Result);
                SubmitPresentation(RuntimePresentationCommandType::Result);
            }
            break;

        case Phase::Result:
            if (GetKeyDown('R')) {
                ++m_retryCount;
                SubmitPresentation(RuntimePresentationCommandType::Cancel);
                BeginAttempt();
            } else if (GetKeyDown(VK_ESCAPE)) {
                m_transitionSubmitted = SubmitTransition(
                    {},
                    TransitionRequest::Selection
                );
            } else if (GetKeyDown(VK_RETURN) || GetKeyDown('N')) {
                m_transitionSubmitted = SubmitTransition(
                    "Asset/Game/MiniGameCollection/Scene/ColorTerritory/ColorTerritory.scene",
                    TransitionRequest::NextGame
                );
            }
            break;
        }
    }

    void OnFixedUpdate(float dt) override {
        (void)dt;
    }

    void OnDraw() override {
        DrawScreenHeader(
            "PRESENTATION SPIKE",
            "光る目印が中央へ来た瞬間に押せ！",
            "操作：SPACE",
            RemainingSeconds()
        );
        DrawTimingTrack();
        DrawOutcomePanel();
    }

    void OnEditorUpdate(float dt) override {
        (void)dt;
    }

    void OnStop() override {
        ShutdownSpike();
        MiniGameRuntimeMailbox::UnregisterRulesShutdown(m_sceneToken);
        MiniGameRuntimeMailbox::ClearForScene(m_sceneToken);
    }

    void QueueSpikeVisuals() {
        QueueCube(
            "PresentationSpikePlatform",
            Vector3(0.0f, -0.18f, 0.0f),
            Vector3(11.0f, 0.3f, 7.0f),
            DirectX::XMFLOAT4(0.08f, 0.1f, 0.16f, 1.0f)
        );
        QueueCube(
            "PresentationSpikeMarker",
            Vector3(-4.0f, 0.65f, 0.0f),
            Vector3(0.5f, 1.0f, 0.5f),
            DirectX::XMFLOAT4(1.0f, 0.76f, 0.15f, 1.0f),
            [this](const CubeVisualRefs& refs) {
                m_markerTransform = refs.transform;
                m_markerMaterial = refs.material;
            }
        );
        QueueCube(
            "PresentationSpikeGoal",
            Vector3(0.0f, 0.16f, 0.0f),
            Vector3(1.3f, 0.15f, 1.3f),
            DirectX::XMFLOAT4(0.2f, 0.75f, 1.0f, 1.0f)
        );
    }

    void BeginAttempt() {
        m_phase = Phase::Countdown;
        m_outcome = Outcome::Pending;
        m_phaseElapsedSeconds = 0.0f;
        m_totalElapsedSeconds = 0.0f;
        m_inputErrorSeconds = 0.0f;
        m_markerPosition = -4.0f;
        SubmitPresentation(RuntimePresentationCommandType::BeginScene);
        SubmitPresentation(RuntimePresentationCommandType::Countdown);
        ApplyMarkerVisual();
    }

    void UpdateMarker(float deltaTime) {
        (void)deltaTime;
        const float normalized = std::clamp(
            m_phaseElapsedSeconds / InputTimeoutSeconds,
            0.0f,
            1.0f
        );
        m_markerPosition = std::lerp(-4.0f, 4.0f, normalized);
        ApplyMarkerVisual();
    }

    void ResolveInput() {
        m_inputErrorSeconds = std::abs(
            m_phaseElapsedSeconds - IdealInputSeconds
        );
        if (m_inputErrorSeconds <= SuccessToleranceSeconds) {
            ResolveOutcome(Outcome::Success);
        } else if (m_inputErrorSeconds <= NearMissToleranceSeconds) {
            ResolveOutcome(Outcome::NearMiss);
        } else {
            ResolveOutcome(Outcome::Failure);
        }
    }

    void ResolveOutcome(Outcome outcome) {
        if (m_phase != Phase::InputWindow || m_outcome != Outcome::Pending) {
            return;
        }

        m_outcome = outcome;
        ChangePhase(Phase::Outcome);
        switch (outcome) {
        case Outcome::Success:
            SubmitPresentation(
                RuntimePresentationCommandType::Success,
                {0.0f, 0.0f},
                1.3f
            );
            break;
        case Outcome::NearMiss:
            SubmitPresentation(
                RuntimePresentationCommandType::NearMiss,
                {m_markerPosition, 0.0f},
                0.8f
            );
            break;
        case Outcome::Failure:
            SubmitPresentation(
                RuntimePresentationCommandType::Failure,
                {m_markerPosition, 0.0f},
                1.0f
            );
            break;
        case Outcome::Pending:
            break;
        }
        ApplyMarkerVisual();
    }

    void ApplyMarkerVisual() {
        if (TransformComponent* transform = m_markerTransform.TryGet()) {
            transform->position = Vector3(m_markerPosition, 0.65f, 0.0f);
            const float scale = m_outcome == Outcome::Success
                ? 1.45f
                : m_outcome == Outcome::Failure
                    ? 0.72f
                    : 1.0f;
            transform->scale = Vector3(0.5f * scale, 1.0f * scale, 0.5f * scale);
        }
        if (MaterialComponent* material = m_markerMaterial.TryGet()) {
            switch (m_outcome) {
            case Outcome::Success:
                material->Material.BaseColor =
                    DirectX::XMFLOAT4(0.25f, 1.0f, 0.42f, 1.0f);
                material->Material.EmissiveIntensity = 1.3f;
                break;
            case Outcome::NearMiss:
                material->Material.BaseColor =
                    DirectX::XMFLOAT4(1.0f, 0.72f, 0.18f, 1.0f);
                material->Material.EmissiveIntensity = 0.65f;
                break;
            case Outcome::Failure:
                material->Material.BaseColor =
                    DirectX::XMFLOAT4(1.0f, 0.2f, 0.2f, 1.0f);
                material->Material.EmissiveIntensity = 0.25f;
                break;
            case Outcome::Pending:
                material->Material.BaseColor =
                    DirectX::XMFLOAT4(1.0f, 0.76f, 0.15f, 1.0f);
                material->Material.EmissiveIntensity = 0.4f;
                break;
            }
        }
    }

    void DrawTimingTrack() const {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 center = viewport->GetCenter();
        const ImVec2 trackMin(center.x - 260.0f, center.y + 120.0f);
        const ImVec2 trackMax(center.x + 260.0f, center.y + 154.0f);
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        draw->AddRectFilled(trackMin, trackMax, IM_COL32(18, 24, 38, 225), 8.0f);
        draw->AddRectFilled(
            ImVec2(center.x - 42.0f, trackMin.y),
            ImVec2(center.x + 42.0f, trackMax.y),
            IM_COL32(50, 180, 245, 175),
            6.0f
        );
        if (m_phase == Phase::InputWindow || m_phase == Phase::Outcome) {
            const float normalized = std::clamp(
                (m_markerPosition + 4.0f) / 8.0f,
                0.0f,
                1.0f
            );
            const float markerX = std::lerp(trackMin.x, trackMax.x, normalized);
            draw->AddCircleFilled(
                ImVec2(markerX, (trackMin.y + trackMax.y) * 0.5f),
                13.0f,
                IM_COL32(255, 212, 72, 255),
                24
            );
        }
    }

    void DrawOutcomePanel() const {
        if (m_phase != Phase::Outcome && m_phase != Phase::Result) {
            return;
        }

        const char* label = "WAIT";
        switch (m_outcome) {
        case Outcome::Success: label = "SUCCESS"; break;
        case Outcome::NearMiss: label = "CLOSE"; break;
        case Outcome::Failure: label = "MISS"; break;
        case Outcome::Pending: break;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            viewport->GetCenter(),
            ImGuiCond_Always,
            ImVec2(0.5f, 0.5f)
        );
        ImGui::SetNextWindowBgAlpha(0.9f);
        ImGui::Begin(
            "##PresentationSpikeResult",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings
        );
        ImGui::TextUnformatted(label);
        if (m_outcome != Outcome::Failure || m_inputErrorSeconds > 0.0f) {
            ImGui::Text("ERROR %.3f sec", m_inputErrorSeconds);
        }
        ImGui::Text("RETRY COUNT %u", m_retryCount);
        if (m_phase == Phase::Result) {
            ImGui::Separator();
            ImGui::TextUnformatted(
                "R: RETRY   ENTER/N: COLOR TERRITORY   ESC: SELECT"
            );
        }
        ImGui::End();
    }

    float RemainingSeconds() const noexcept {
        switch (m_phase) {
        case Phase::Countdown:
            return std::max(0.0f, CountdownSeconds - m_phaseElapsedSeconds);
        case Phase::InputWindow:
            return std::max(0.0f, InputTimeoutSeconds - m_phaseElapsedSeconds);
        case Phase::Outcome:
            return std::max(0.0f, OutcomeDisplaySeconds - m_phaseElapsedSeconds);
        case Phase::Result:
            return 0.0f;
        }
        return 0.0f;
    }

    void ChangePhase(Phase phase) noexcept {
        m_phase = phase;
        m_phaseElapsedSeconds = 0.0f;
    }

    void ShutdownSpike() {
        if (m_shutdown) {
            return;
        }
        m_shutdown = true;
        SubmitPresentation(RuntimePresentationCommandType::Cancel);
    }

    ComponentRef<TransformComponent> m_markerTransform;
    ComponentRef<MaterialComponent> m_markerMaterial;
    SceneToken m_sceneToken = 0;
    Phase m_phase = Phase::Countdown;
    Outcome m_outcome = Outcome::Pending;
    float m_phaseElapsedSeconds = 0.0f;
    float m_totalElapsedSeconds = 0.0f;
    float m_markerPosition = -4.0f;
    float m_inputErrorSeconds = 0.0f;
    std::uint32_t m_retryCount = 0;
    bool m_shutdown = false;
    bool m_transitionSubmitted = false;
};

} // namespace MiniGameCollection::Runtime
