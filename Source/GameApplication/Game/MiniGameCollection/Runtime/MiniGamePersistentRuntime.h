#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCollectionManagerModel.h"
#include "Game/MiniGameCollection/Core/MiniGameSceneTransitionEngineBackend.h"
#include "Game/MiniGameCollection/Presentation/MiniGameFallbackEffectPool.h"
#include "Game/MiniGameCollection/Presentation/MiniGamePresentationEngineBackend.h"
#include "Game/MiniGameCollection/Presentation/MiniGameProceduralAudio.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeMailbox.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"

#include "Scene/Component/audioComponent.h"
#include "Scene/Component/cameraComponent.h"
#include "Scene/Component/materialComponent.h"
#include "Scene/Component/transformComponent.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MiniGameCollection::Runtime {

class MiniGamePersistentRuntime final : public MiniGameRuntimeScriptBase {
public:
    MiniGamePersistentRuntime()
        : MiniGameRuntimeScriptBase("MiniGamePersistentRuntime") {
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Early, -200);
        SetExecutionOrder(SystemTaskDomain::Render, SystemPhase::Late, 200);
    }

    YAML::Node encode() override {
        YAML::Node node = MiniGameRuntimeScriptBase::encode();
        node["SelectedIndex"] = static_cast<int>(m_collection.GetSelectedIndex());
        return node;
    }

    bool decode(SceneContext* context, const YAML::Node& node) override {
        MiniGameRuntimeScriptBase::decode(context, node);
        if (node["SelectedIndex"]) {
            const int index = node["SelectedIndex"].as<int>();
            if (index >= 0) {
                m_collection.SelectIndex(static_cast<std::size_t>(index));
            }
        }
        return true;
    }

private:
    static constexpr const char* PresentationSpikePath =
        "Asset/Game/MiniGameCollection/Scene/PresentationTest/PresentationSpike.scene";

    struct HudBurst {
        SceneToken sceneToken = 0;
        Vec2 worldPosition{};
        float remainingSeconds = 0.0f;
        float durationSeconds = 0.0f;
        float intensity = 1.0f;
    };

    struct ScreenFlashOverlay {
        SceneToken sceneToken = 0;
        float remainingSeconds = 0.0f;
        float durationSeconds = 0.0f;
        float intensity = 0.0f;
    };

    void OnStart() override {
        SceneContext* context = GetEntityRef().GetScene();
        if (!context || !context->manager || !context->manager->sceneManager) {
            return;
        }

        m_backend = std::make_unique<Presentation::MiniGamePresentationEngineBackend>(
            context
        );
        m_presentation = std::make_unique<Presentation::MiniGamePresentationService>(
            *m_backend
        );
        m_transitionBackend =
            std::make_unique<MiniGameSceneTransitionEngineBackend>(
                *context->manager->sceneManager,
                *m_presentation,
                [this](bool enabled) {
                    m_selectionInputEnabled = enabled;
                },
                [](SceneToken sceneToken) {
                    MiniGameRuntimeMailbox::InvokeRulesShutdown(sceneToken);
                }
            );
        m_transition = std::make_unique<MiniGameSceneTransition>(
            *m_transitionBackend
        );

        RegisterExistingCameraAndFlash(context);
        QueueUiTargets();
        QueueAudioPool();
        QueueFallbackEffectPool();
        m_selectionInputEnabled = true;
    }

    void OnUpdate(float dt) override {
        if (!m_backend || !m_presentation || !m_transition) {
            return;
        }

        const float unscaledDeltaTime = std::max(0.0f, dt);
        m_backend->Tick(unscaledDeltaTime);
        m_fallbackEffects.Tick(unscaledDeltaTime);
        m_presentation->Tick(unscaledDeltaTime);
        UpdateHudBursts(unscaledDeltaTime);
        UpdateScreenFlash(unscaledDeltaTime);
        UpdateCountdown(unscaledDeltaTime);
        DispatchPresentationCommands();

        if (m_transition->IsActive()) {
            m_transition->Tick(unscaledDeltaTime);
        } else if (m_transition->IsComplete() || m_transition->HasFailed()) {
            m_transition->Reset();
            m_selectionInputEnabled = true;
        } else if (auto command = MiniGameRuntimeMailbox::ConsumeTransition()) {
            m_transition->Begin({
                .sourceSceneToken = command->sceneToken,
                .sourceSceneName = command->sourceSceneName,
                .targetScenePath = command->targetScenePath,
                .reason = command->reason,
                .presentationWaitSeconds = command->presentationWaitSeconds
            });
        }

        if (!IsAnyMiniGameSceneLoaded() && !m_transition->IsActive()) {
            UpdateSelectionInput();
        }
    }

    void OnFixedUpdate(float dt) override {
        (void)dt;
    }

    void OnDraw() override {
        if (!IsAnyMiniGameSceneLoaded() &&
            (!m_transition || !m_transition->IsActive())) {
            DrawSelection();
        }
        DrawCountdownOverlay();
        DrawHudBursts();
        DrawScreenFlashOverlay();
    }

    void OnEditorUpdate(float dt) override {
        (void)dt;
    }

    void OnStop() override {
        if (m_presentation && m_presentation->GetSceneToken() != 0) {
            const SceneToken token = m_presentation->GetSceneToken();
            m_presentation->CancelAllForScene(token);
            m_fallbackEffects.CancelAllForScene(token);
        }
        m_transition.reset();
        m_transitionBackend.reset();
        m_presentation.reset();
        m_backend.reset();
        m_hudBursts.clear();
        m_uiTargets.clear();
        m_screenFlash = {};
    }

    void RegisterExistingCameraAndFlash(SceneContext* context) {
        const auto cameras =
            context->component->FindEntitiesWithComponent<CameraComponent>();
        if (!cameras.empty()) {
            m_backend->RegisterCamera(
                ComponentRef<TransformComponent>(cameras.front(), context)
            );
        }

        const auto materials =
            context->component->FindEntitiesWithComponent<MaterialComponent>();
        for (Entity entity : materials) {
            const auto* name =
                context->component->GetComponent<NameComponent>(entity);
            if (name && name->name == "MiniGameScreenFlash") {
                m_backend->RegisterScreenFlash(
                    ComponentRef<MaterialComponent>(entity, context)
                );
                break;
            }
        }
    }

    void QueueUiTargets() {
        static constexpr const char* targetIds[] = {
            "countdown", "go", "outcome", "result", "retry"
        };
        for (const char* targetId : targetIds) {
            CommandEntity pending = QueueCreateEntity();
            QueueAddComponent<NameComponent>(pending);
            QueueAddComponent<TransformComponent>(pending);
            QueueEntitySetup(
                pending,
                [this, target = std::string(targetId)](
                    Entity entity,
                    SceneContext& context
                ) {
                    if (auto* name =
                        context.component->GetComponent<NameComponent>(entity)) {
                        name->name = "MiniGameUiTween_" + target;
                    }
                    ComponentRef<TransformComponent> transform(entity, &context);
                    m_backend->RegisterUiTarget(target, transform);
                    m_uiTargets[target] = std::move(transform);
                }
            );
        }
    }

    void QueueAudioPool() {
        struct CueDefinition {
            const char* cueId;
            Presentation::ProceduralToneDescription tone;
            int voiceCount;
        };

        const CueDefinition cues[] = {
            {"countdown", Presentation::MiniGameProceduralAudio::Countdown(), 4},
            {"go", Presentation::MiniGameProceduralAudio::Go(), 3},
            {"success", Presentation::MiniGameProceduralAudio::Score(), 4},
            {"near_miss", Presentation::MiniGameProceduralAudio::Countdown(), 3},
            {"failure", Presentation::MiniGameProceduralAudio::Failure(), 3},
            {"result", Presentation::MiniGameProceduralAudio::Result(), 3},
            {"score", Presentation::MiniGameProceduralAudio::Score(), 6},
            {"hit", Presentation::MiniGameProceduralAudio::Hit(), 5},
            {"warning", Presentation::MiniGameProceduralAudio::Countdown(), 3},
            {"eliminate", Presentation::MiniGameProceduralAudio::Hit(), 4}
        };

        for (const CueDefinition& cue : cues) {
            const auto audioData = Presentation::MiniGameProceduralAudio::CreateTone(
                std::string("procedural://") + cue.cueId,
                cue.tone
            );
            for (int index = 0; index < cue.voiceCount; ++index) {
                CommandEntity pending = QueueCreateEntity();
                QueueAddComponent<NameComponent>(pending);
                QueueAddComponent<AudioComponent>(pending);
                QueueEntitySetup(
                    pending,
                    [
                        this,
                        cueId = std::string(cue.cueId),
                        audioData,
                        index
                    ](Entity entity, SceneContext& context) {
                        if (auto* name =
                            context.component->GetComponent<NameComponent>(entity)) {
                            name->name = cueId + "_Voice_" +
                                std::to_string(index);
                        }
                        if (auto* audio =
                            context.component->GetComponent<AudioComponent>(entity)) {
                            audio->m_AudioData = audioData;
                            audio->FilePath.clear();
                            audio->Loop = false;
                            audio->PlayOnStart = false;
                            audio->isInitialized = true;
                        }
                        m_backend->RegisterAudioVoice(
                            cueId,
                            ComponentRef<AudioComponent>(entity, &context)
                        );
                    }
                );
            }
        }
    }

    void QueueFallbackEffectPool() {
        constexpr int EffectVoiceCount = 16;
        for (int index = 0; index < EffectVoiceCount; ++index) {
            QueueCube(
                "MiniGameFallbackEffect_" + std::to_string(index),
                Vector3(0.0f, -1000.0f, 0.0f),
                Vector3(0.0f, 0.0f, 0.0f),
                DirectX::XMFLOAT4(1.0f, 0.75f, 0.2f, 0.0f),
                [this](const CubeVisualRefs& refs) {
                    m_fallbackEffects.RegisterVoice(
                        refs.transform,
                        refs.material
                    );
                }
            );
        }
    }

    void DispatchPresentationCommands() {
        for (const RuntimePresentationCommand& command :
            MiniGameRuntimeMailbox::ConsumePresentation()) {
            switch (command.type) {
            case RuntimePresentationCommandType::BeginScene:
                m_presentation->BeginScene(command.sceneToken);
                break;

            case RuntimePresentationCommandType::Countdown:
                if (m_presentation->GetSceneToken() != command.sceneToken) {
                    m_presentation->BeginScene(command.sceneToken);
                }
                m_countdownRemainingSeconds = 3.45f;
                m_presentation->PlayCountdown();
                break;

            case RuntimePresentationCommandType::Score:
                m_backend->PlayOneShotSound({
                    .cueId = "score",
                    .volume = 0.9f,
                    .pitch = 1.0f + std::min(
                        0.35f,
                        command.intensity * 0.05f
                    ),
                    .sceneToken = command.sceneToken
                });
                m_backend->PlayCameraShake({
                    .durationSeconds = 0.11f,
                    .amplitude = 0.05f + command.intensity * 0.025f,
                    .frequency = 18.0f,
                    .sceneToken = command.sceneToken
                });
                PlayFallbackEffect(command);
                AddHudBurst(command);
                break;

            case RuntimePresentationCommandType::Hit:
                m_backend->PlayOneShotSound({
                    .cueId = "hit",
                    .volume = 1.0f,
                    .pitch = 0.95f,
                    .sceneToken = command.sceneToken
                });
                m_backend->PlayCameraShake({
                    .durationSeconds = 0.18f,
                    .amplitude = 0.13f * command.intensity,
                    .frequency = 20.0f,
                    .sceneToken = command.sceneToken
                });
                m_backend->PlayScreenFlash({
                    .durationSeconds = 0.1f,
                    .intensity = 0.28f * command.intensity,
                    .sceneToken = command.sceneToken
                });
                PlayFallbackEffect(command);
                StartScreenFlash(command.sceneToken, 0.1f, 0.28f * command.intensity);
                AddHudBurst(command);
                break;

            case RuntimePresentationCommandType::Success:
                PlayFallbackEffect(command);
                StartScreenFlash(command.sceneToken, 0.13f, 0.5f * command.intensity);
                m_presentation->PlaySuccess(0.0f, command.intensity);
                AddHudBurst(command);
                break;

            case RuntimePresentationCommandType::NearMiss:
                m_presentation->PlayNearMiss();
                break;

            case RuntimePresentationCommandType::Failure:
                StartScreenFlash(command.sceneToken, 0.2f, 0.2f);
                m_presentation->PlayFailure();
                break;

            case RuntimePresentationCommandType::Result:
                m_presentation->PlayResult(0.0f);
                break;

            case RuntimePresentationCommandType::Cancel:
                m_presentation->CancelAllForScene(command.sceneToken);
                m_fallbackEffects.CancelAllForScene(command.sceneToken);
                std::erase_if(
                    m_hudBursts,
                    [token = command.sceneToken](const HudBurst& burst) {
                        return burst.sceneToken == token;
                    }
                );
                if (m_screenFlash.sceneToken == command.sceneToken) {
                    m_screenFlash = {};
                }
                break;
            }
        }
    }

    void PlayFallbackEffect(const RuntimePresentationCommand& command) {
        m_fallbackEffects.Play(
            command.sceneToken,
            command.position,
            command.intensity,
            m_nextEffectSerial++
        );
    }

    void StartScreenFlash(
        SceneToken sceneToken,
        float durationSeconds,
        float intensity
    ) {
        m_screenFlash = {
            .sceneToken = sceneToken,
            .remainingSeconds = std::max(0.0f, durationSeconds),
            .durationSeconds = std::max(0.001f, durationSeconds),
            .intensity = std::clamp(intensity, 0.0f, 1.0f)
        };
    }

    void UpdateSelectionInput() {
        if (!m_selectionInputEnabled) {
            return;
        }

        std::size_t selected = m_collection.GetSelectedIndex();
        if (GetKeyDown('W') || GetKeyDown(VK_UP)) {
            selected = selected == 0
                ? m_collection.GetGameCount() - 1
                : selected - 1;
            m_collection.SelectIndex(selected);
        }
        if (GetKeyDown('S') || GetKeyDown(VK_DOWN)) {
            selected = (selected + 1) % m_collection.GetGameCount();
            m_collection.SelectIndex(selected);
        }

        if (GetKeyDown(VK_RETURN) || GetKeyDown(VK_SPACE)) {
            SceneContext* context = GetEntityRef().GetScene();
            if (context && context->manager && context->manager->sceneManager) {
                const MiniGameDescriptor& game = m_collection.GetSelectedGame();
                if (context->manager->sceneManager->LoadFromFilePath(game.scenePath)) {
                    m_selectionInputEnabled = false;
                }
            }
        }
    }

    bool IsAnyMiniGameSceneLoaded() const {
        SceneContext* context = GetEntityRef().GetScene();
        if (!context || !context->manager || !context->manager->sceneManager) {
            return false;
        }

        for (const auto& [name, scene] :
            context->manager->sceneManager->GetActiveScenes()) {
            (void)name;
            if (!scene || scene->GetSceneContext() == context) {
                continue;
            }
            if (scene->ScenePath == PresentationSpikePath) {
                return true;
            }
            for (std::size_t index = 0;
                 index < m_collection.GetGameCount();
                 ++index) {
                if (scene->ScenePath == m_collection.GetGame(index).scenePath) {
                    return true;
                }
            }
        }
        return false;
    }

    void DrawSelection() const {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            viewport->GetCenter(),
            ImGuiCond_Always,
            ImVec2(0.5f, 0.5f)
        );
        ImGui::SetNextWindowSize(ImVec2(580.0f, 390.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.9f);
        ImGui::Begin(
            "MINI GAME COLLECTION",
            nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings
        );
        ImGui::TextUnformatted("SHORT COMPETITIVE GAMES / 1 PLAYER + 3 CPU");
        ImGui::Separator();
        for (std::size_t index = 0; index < m_collection.GetGameCount(); ++index) {
            const MiniGameDescriptor& game = m_collection.GetGame(index);
            const bool selected = index == m_collection.GetSelectedIndex();
            if (selected) {
                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    ImVec4(1.0f, 0.85f, 0.25f, 1.0f)
                );
            }
            ImGui::Text(
                "%s %s",
                selected ? ">" : " ",
                game.displayName.c_str()
            );
            if (selected) {
                ImGui::PopStyleColor();
                ImGui::Indent(28.0f);
                ImGui::TextWrapped("%s", game.ruleText.c_str());
                ImGui::TextUnformatted(game.controlText.c_str());
                ImGui::Unindent(28.0f);
            }
            ImGui::Spacing();
        }
        ImGui::Separator();
        ImGui::TextUnformatted(
            "W/S or UP/DOWN: SELECT   ENTER/SPACE: PLAY"
        );
        ImGui::End();
    }

    void UpdateCountdown(float deltaTime) {
        if (m_countdownRemainingSeconds > 0.0f) {
            m_countdownRemainingSeconds = std::max(
                0.0f,
                m_countdownRemainingSeconds - deltaTime
            );
        }
    }

    void DrawCountdownOverlay() const {
        if (m_countdownRemainingSeconds <= 0.0f) {
            return;
        }

        const char* text = "GO!";
        if (m_countdownRemainingSeconds > 2.45f) text = "3";
        else if (m_countdownRemainingSeconds > 1.45f) text = "2";
        else if (m_countdownRemainingSeconds > 0.45f) text = "1";

        float scale = 1.0f;
        const char* targetId = text[0] == 'G' ? "go" : "countdown";
        auto found = m_uiTargets.find(targetId);
        if (found != m_uiTargets.end()) {
            if (const TransformComponent* transform = found->second.TryGet()) {
                scale = std::max(0.5f, transform->scale.x);
            }
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        const ImVec2 center = viewport->GetCenter();
        const float radius = 62.0f * scale;
        draw->AddCircleFilled(
            center,
            radius,
            IM_COL32(15, 20, 30, 210),
            48
        );
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        draw->AddText(
            ImVec2(
                center.x - textSize.x * 0.5f,
                center.y - textSize.y * 0.5f
            ),
            IM_COL32(255, 245, 180, 255),
            text
        );
    }

    void AddHudBurst(const RuntimePresentationCommand& command) {
        m_hudBursts.push_back({
            .sceneToken = command.sceneToken,
            .worldPosition = command.position,
            .remainingSeconds = 0.42f,
            .durationSeconds = 0.42f,
            .intensity = std::max(0.25f, command.intensity)
        });
    }

    void UpdateHudBursts(float deltaTime) {
        for (HudBurst& burst : m_hudBursts) {
            burst.remainingSeconds = std::max(
                0.0f,
                burst.remainingSeconds - deltaTime
            );
        }
        std::erase_if(
            m_hudBursts,
            [](const HudBurst& burst) {
                return burst.remainingSeconds <= 0.0f;
            }
        );
    }

    void DrawHudBursts() const {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        for (const HudBurst& burst : m_hudBursts) {
            const float normalized = burst.durationSeconds > 0.0f
                ? 1.0f - burst.remainingSeconds / burst.durationSeconds
                : 1.0f;
            const ImVec2 position(
                viewport->WorkPos.x + viewport->WorkSize.x *
                    (0.5f + burst.worldPosition.x / 24.0f),
                viewport->WorkPos.y + viewport->WorkSize.y *
                    (0.5f - burst.worldPosition.y / 18.0f)
            );
            const float radius =
                (18.0f + normalized * 52.0f) * burst.intensity;
            const int alpha = static_cast<int>(
                220.0f * (1.0f - normalized)
            );
            draw->AddCircle(
                position,
                radius,
                IM_COL32(255, 235, 110, alpha),
                32,
                4.0f
            );
        }
    }

    void UpdateScreenFlash(float deltaTime) {
        m_screenFlash.remainingSeconds = std::max(
            0.0f,
            m_screenFlash.remainingSeconds - deltaTime
        );
        if (m_screenFlash.remainingSeconds <= 0.0f) {
            m_screenFlash = {};
        }
    }

    void DrawScreenFlashOverlay() const {
        if (m_screenFlash.remainingSeconds <= 0.0f) {
            return;
        }
        const float normalized = std::clamp(
            m_screenFlash.remainingSeconds / m_screenFlash.durationSeconds,
            0.0f,
            1.0f
        );
        const int alpha = static_cast<int>(
            255.0f * m_screenFlash.intensity * normalized
        );
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::GetForegroundDrawList()->AddRectFilled(
            viewport->WorkPos,
            ImVec2(
                viewport->WorkPos.x + viewport->WorkSize.x,
                viewport->WorkPos.y + viewport->WorkSize.y
            ),
            IM_COL32(255, 255, 255, alpha)
        );
    }

    MiniGameCollectionManagerModel m_collection;
    std::unique_ptr<Presentation::MiniGamePresentationEngineBackend> m_backend;
    std::unique_ptr<Presentation::MiniGamePresentationService> m_presentation;
    std::unique_ptr<MiniGameSceneTransitionEngineBackend> m_transitionBackend;
    std::unique_ptr<MiniGameSceneTransition> m_transition;
    Presentation::MiniGameFallbackEffectPool m_fallbackEffects;
    std::unordered_map<std::string, ComponentRef<TransformComponent>> m_uiTargets;
    std::vector<HudBurst> m_hudBursts;
    ScreenFlashOverlay m_screenFlash{};
    std::uint64_t m_nextEffectSerial = 1;
    float m_countdownRemainingSeconds = 0.0f;
    bool m_selectionInputEnabled = true;
};

} // namespace MiniGameCollection::Runtime
