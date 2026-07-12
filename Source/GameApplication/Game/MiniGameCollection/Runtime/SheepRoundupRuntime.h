#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCpuDecisionClock.h"
#include "Game/MiniGameCollection/Core/MiniGamePlayerModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeUi.h"
#include "Game/MiniGameCollection/SheepRoundup/SheepRoundupRules.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace MiniGameCollection::Runtime {

class SheepRoundupRuntime final : public MiniGameRuntimeScriptBase {
public:
    SheepRoundupRuntime()
        : MiniGameRuntimeScriptBase("SheepRoundupRuntime"),
          m_rules(PlayerCount, GameDurationSeconds, MovementArea) {
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, 0);
        SetExecutionOrder(SystemTaskDomain::Render, SystemPhase::Late, 100);
    }

private:
    static constexpr std::size_t PlayerCount = 4;
    static constexpr float GameDurationSeconds = 50.0f;
    static constexpr float SpawnPopSeconds = 0.55f;
    static constexpr SheepRoundup::Bounds2 MovementArea{
        {-10.0f, -7.0f},
        {10.0f, 7.0f}
    };
    static constexpr const char* ScenePath =
        "Asset/Game/MiniGameCollection/Scene/SheepRoundup/SheepRoundup.scene";
    static constexpr const char* NextScenePath =
        "Asset/Game/MiniGameCollection/Scene/Backshot/Backshot.scene";

    struct PlayerRuntime {
        MiniGamePlayerState state{};
        ComponentRef<TransformComponent> transform;
        std::optional<std::size_t> targetSheep;
        Vec2 interceptPosition{};
    };

    struct SheepVisual {
        ComponentRef<TransformComponent> bodyTransform;
        ComponentRef<MaterialComponent> bodyMaterial;
        ComponentRef<TransformComponent> haloTransform;
        ComponentRef<MaterialComponent> haloMaterial;
        float spawnPulseRemainingSeconds = 0.0f;
    };

    struct Banner {
        std::string text;
        D2D1::ColorF color = D2D1::ColorF(1.0f, 0.86f, 0.18f, 1.0f);
        float remainingSeconds = 0.0f;
    };

    void OnStart() override {
        m_sceneToken = GetRuntimeSceneToken();
        m_rules.Prepare();
        m_players = {};
        m_players[0].state = {
            .playerId = 0,
            .position = {-7.5f, -4.8f},
            .inputEnabled = false
        };
        m_players[1].state = {
            .playerId = 1,
            .position = {7.5f, -4.8f},
            .inputEnabled = false
        };
        m_players[2].state = {
            .playerId = 2,
            .position = {7.5f, 4.8f},
            .inputEnabled = false
        };
        m_players[3].state = {
            .playerId = 3,
            .position = {-7.5f, 4.8f},
            .inputEnabled = false
        };

        for (std::size_t index = 0; index < PlayerCount; ++index) {
            m_rules.SetPlayerPosition(
                static_cast<PlayerId>(index),
                m_players[index].state.position
            );
        }

        m_cpuClocks.clear();
        for (std::uint32_t index = 1; index < PlayerCount; ++index) {
            m_cpuClocks.emplace_back(
                index == 1
                    ? CpuDifficultyProfile::Easy()
                    : index == 2
                        ? CpuDifficultyProfile::Normal()
                        : CpuDifficultyProfile::Hard(),
                0x5EE0000u + index * 1297u
            );
            m_cpuClocks.back().Reset(0.2f * static_cast<float>(index));
        }

        QueueStageVisuals();
        QueuePlayerVisuals();
        QueueSheepVisuals();
        QueuePenVisuals();

        MiniGameRuntimeMailbox::RegisterRulesShutdown(
            m_sceneToken,
            [this]() { ShutdownRules(); }
        );
        SubmitPresentation(RuntimePresentationCommandType::BeginScene);
        SubmitPresentation(RuntimePresentationCommandType::Countdown);
        m_countdownRemainingSeconds = 3.0f;
        m_result.reset();
        m_banner = {};
        m_visualTimeSeconds = 0.0f;
        m_transitionSubmitted = false;
        m_warning10Played = false;
        m_warning5Played = false;
        m_lateRushAnnounced = false;
        m_started = false;
        m_rulesShutdown = false;
    }

    void OnUpdate(float dt) override {
        if (m_rulesShutdown || m_transitionSubmitted) {
            return;
        }

        const float delta = std::max(0.0f, dt);
        m_visualTimeSeconds += delta;
        m_banner.remainingSeconds = std::max(
            0.0f,
            m_banner.remainingSeconds - delta
        );

        if (!m_started) {
            m_countdownRemainingSeconds = std::max(
                0.0f,
                m_countdownRemainingSeconds - delta
            );
            if (m_countdownRemainingSeconds <= 0.0f) {
                m_rules.StartGame();
                m_started = true;
                for (PlayerRuntime& player : m_players) {
                    player.state.inputEnabled = true;
                }
            }
            UpdatePlayerVisuals();
            UpdateSheepVisuals(delta);
            return;
        }

        if (!m_rules.IsFinished()) {
            UpdatePlayers(delta);
            m_rules.Tick(delta);

            if (!m_lateRushAnnounced && m_rules.IsLateRush()) {
                m_lateRushAnnounced = true;
                SetBanner(
                    "FLOCK RUSH!  MORE SHEEP / MORE GOLDEN CHANCES",
                    D2D1::ColorF(1.0f, 0.58f, 0.12f, 1.0f),
                    1.35f
                );
                SubmitPresentation(
                    RuntimePresentationCommandType::Hit,
                    {},
                    1.25f
                );
            }

            ApplySpawnEvents();
            UpdateSheepVisuals(delta);
            ApplyScoreEvents();
            UpdateWarnings();

            if (m_rules.IsFinished()) {
                for (PlayerRuntime& player : m_players) {
                    player.state.inputEnabled = false;
                }
                m_result = m_rules.BuildResult();
                SubmitPresentation(RuntimePresentationCommandType::Result);
                if (m_result && !m_result->isTie &&
                    !m_result->players.empty() &&
                    m_result->players.front().playerId == 0) {
                    SubmitPresentation(
                        RuntimePresentationCommandType::Success,
                        {},
                        1.3f
                    );
                } else {
                    SubmitPresentation(RuntimePresentationCommandType::Failure);
                }
            }
        } else {
            UpdateSheepVisuals(delta);
            UpdateResultInput();
        }
    }

    void OnFixedUpdate(float dt) override {
        (void)dt;
    }

    void OnDraw() override {
        DrawScreenHeader(
            "SHEEP ROUNDUP",
            "羊は無限補充。金色の羊は3点！ 後半は群れが一気に増える",
            "操作：WASD / 矢印キーで移動",
            m_rules.GetRemainingSeconds()
        );
        DrawScoreRow(m_rules.GetScores());
        DrawFlockHud();
        if (m_result) {
            DrawResultPanel(*m_result);
        }
    }

    void OnEditorUpdate(float dt) override {
        (void)dt;
    }

    void OnStop() override {
        ShutdownRules();
        MiniGameRuntimeMailbox::UnregisterRulesShutdown(m_sceneToken);
        MiniGameRuntimeMailbox::ClearForScene(m_sceneToken);
    }

    void QueueStageVisuals() {
        QueueCube(
            "SheepField",
            Vector3(0.0f, -0.22f, 0.0f),
            Vector3(21.0f, 0.3f, 15.0f),
            DirectX::XMFLOAT4(0.16f, 0.3f, 0.18f, 1.0f)
        );
        const DirectX::XMFLOAT4 wall(0.18f, 0.14f, 0.12f, 1.0f);
        QueueCube(
            "SheepWallNorth",
            Vector3(0.0f, 0.6f, 7.25f),
            Vector3(21.0f, 1.2f, 0.35f),
            wall
        );
        QueueCube(
            "SheepWallSouth",
            Vector3(0.0f, 0.6f, -7.25f),
            Vector3(21.0f, 1.2f, 0.35f),
            wall
        );
        QueueCube(
            "SheepWallEast",
            Vector3(10.25f, 0.6f, 0.0f),
            Vector3(0.35f, 1.2f, 15.0f),
            wall
        );
        QueueCube(
            "SheepWallWest",
            Vector3(-10.25f, 0.6f, 0.0f),
            Vector3(0.35f, 1.2f, 15.0f),
            wall
        );
    }

    void QueuePlayerVisuals() {
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            QueueCube(
                "SheepPlayer_" + std::to_string(index + 1),
                ToWorld(m_players[index].state.position, 0.72f),
                Vector3(0.75f, 1.15f, 0.75f),
                PlayerColor(static_cast<PlayerId>(index)),
                [this, index](const CubeVisualRefs& refs) {
                    m_players[index].transform = refs.transform;
                }
            );
        }
    }

    void QueueSheepVisuals() {
        const auto& sheep = m_rules.GetSheep();
        m_sheepVisuals.assign(sheep.size(), {});
        for (std::size_t index = 0; index < sheep.size(); ++index) {
            QueueCube(
                "SheepBody_" + std::to_string(index),
                sheep[index].IsActive()
                    ? ToWorld(sheep[index].position, 0.48f)
                    : HiddenPosition(),
                sheep[index].IsActive()
                    ? Vector3(0.64f, 0.72f, 0.84f)
                    : Vector3(),
                DirectX::XMFLOAT4(0.94f, 0.94f, 0.88f, 1.0f),
                [this, index](const CubeVisualRefs& refs) {
                    if (index < m_sheepVisuals.size()) {
                        m_sheepVisuals[index].bodyTransform = refs.transform;
                        m_sheepVisuals[index].bodyMaterial = refs.material;
                    }
                }
            );
            QueueCube(
                "GoldenSheepHalo_" + std::to_string(index),
                HiddenPosition(),
                Vector3(),
                DirectX::XMFLOAT4(1.0f, 0.76f, 0.08f, 1.0f),
                [this, index](const CubeVisualRefs& refs) {
                    if (index < m_sheepVisuals.size()) {
                        m_sheepVisuals[index].haloTransform = refs.transform;
                        m_sheepVisuals[index].haloMaterial = refs.material;
                        ConfigureGoldenMaterial(refs.material, 4.0f);
                    }
                }
            );
        }
    }

    void QueuePenVisuals() {
        for (const SheepRoundup::SheepPenDefinition& pen : m_rules.GetPens()) {
            const DirectX::XMFLOAT4 color = PlayerColor(pen.owner, 0.55f);
            QueueCube(
                "SheepPen_" + std::to_string(pen.owner + 1),
                ToWorld(pen.center, 0.08f),
                Vector3(pen.radius * 1.7f, 0.12f, pen.radius * 1.7f),
                color
            );
            const Vec2 outward = NormalizeOrZero(pen.center);
            const Vec2 back = pen.center + outward * (pen.radius * 0.8f);
            QueueCube(
                "SheepPenBack_" + std::to_string(pen.owner + 1),
                ToWorld(back, 0.5f),
                Vector3(1.5f, 0.9f, 0.32f),
                PlayerColor(pen.owner)
            );
        }
    }

    void UpdatePlayers(float deltaTime) {
        std::array<MiniGamePlayerInput, PlayerCount> inputs{};
        inputs[0].move = ReadMovementInput();

        const auto& sheep = m_rules.GetSheep();
        std::vector<SheepRoundup::SheepTargetCandidate> candidates;
        candidates.reserve(sheep.size());
        std::vector<SheepRoundup::SheepTargetCandidate> goldenCandidates;
        for (const SheepRoundup::SheepState& value : sheep) {
            SheepRoundup::SheepTargetCandidate candidate{
                .sheepIndex = value.sheepId,
                .sheepPosition = value.position,
                .sheepVelocity = value.velocity,
                .alreadyScored = !value.IsActive()
            };
            candidates.push_back(candidate);
            if (value.IsActive() && value.golden) {
                goldenCandidates.push_back(candidate);
            }
        }

        const auto& pens = m_rules.GetPens();
        for (std::size_t cpuIndex = 0; cpuIndex < m_cpuClocks.size(); ++cpuIndex) {
            const std::size_t playerIndex = cpuIndex + 1;
            MiniGameCpuDecisionClock& clock = m_cpuClocks[cpuIndex];
            if (clock.Tick(deltaTime) && clock.CanChangeTarget()) {
                SheepRoundup::SheepCpuContext context;
                context.cpuPosition = m_players[playerIndex].state.position;
                context.ownPenCenter = FindPenCenter(
                    pens,
                    static_cast<PlayerId>(playerIndex)
                );
                context.remainingTimeRatio = m_rules.GetRemainingTimeRatio();
                for (std::size_t opponent = 0; opponent < PlayerCount; ++opponent) {
                    if (opponent != playerIndex) {
                        context.opponentPositions.push_back(
                            m_players[opponent].state.position
                        );
                    }
                }

                const CpuDifficultyProfile difficulty = cpuIndex == 0
                    ? CpuDifficultyProfile::Easy()
                    : cpuIndex == 1
                        ? CpuDifficultyProfile::Normal()
                        : CpuDifficultyProfile::Hard();
                const float standOff = cpuIndex == 0 ? 1.15f : 1.65f;
                auto decision = SheepRoundup::SheepCpuEvaluator::ChooseSheep(
                    candidates,
                    context,
                    difficulty,
                    standOff
                );

                const auto goldenDecision =
                    SheepRoundup::SheepCpuEvaluator::ChooseSheep(
                        goldenCandidates,
                        context,
                        difficulty,
                        standOff
                    );
                if (goldenDecision) {
                    const float distance = Distance(
                        m_players[playerIndex].state.position,
                        sheep[goldenDecision->sheepIndex].position
                    );
                    const float goldenAwareness = cpuIndex == 0
                        ? 5.5f
                        : cpuIndex == 1
                            ? 8.0f
                            : 12.0f;
                    if (distance <= goldenAwareness || !decision) {
                        decision = goldenDecision;
                    }
                }

                if (decision) {
                    m_players[playerIndex].targetSheep = decision->sheepIndex;
                    m_players[playerIndex].interceptPosition =
                        decision->interceptPosition;
                    if (clock.ShouldMakeMistake()) {
                        m_players[playerIndex].interceptPosition.x +=
                            playerIndex % 2 == 0 ? 1.2f : -1.2f;
                    }
                    clock.CommitTarget();
                }
            }

            if (m_players[playerIndex].targetSheep) {
                const std::size_t targetIndex =
                    *m_players[playerIndex].targetSheep;
                if (targetIndex >= sheep.size() ||
                    !sheep[targetIndex].IsActive()) {
                    m_players[playerIndex].targetSheep.reset();
                    clock.ClearTarget();
                } else {
                    const Vec2 toTarget =
                        m_players[playerIndex].interceptPosition -
                        m_players[playerIndex].state.position;
                    inputs[playerIndex].move = NormalizeOrZero(toTarget);
                    if (LengthSquared(toTarget) < 0.2f) {
                        inputs[playerIndex].move = NormalizeOrZero(
                            sheep[targetIndex].position -
                            m_players[playerIndex].state.position
                        );
                    }
                }
            }
        }

        MovementBounds bounds{
            MovementArea.minimum,
            MovementArea.maximum
        };
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            MiniGamePlayerModel::Tick(
                m_players[index].state,
                inputs[index],
                m_playerConfig,
                bounds,
                deltaTime
            );
        }
        for (std::size_t lhs = 0; lhs < PlayerCount; ++lhs) {
            for (std::size_t rhs = lhs + 1; rhs < PlayerCount; ++rhs) {
                MiniGamePlayerModel::ResolveSoftContact(
                    m_players[lhs].state,
                    m_players[rhs].state,
                    m_playerConfig,
                    0.65f
                );
            }
        }
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            m_rules.SetPlayerPosition(
                static_cast<PlayerId>(index),
                m_players[index].state.position
            );
        }
        UpdatePlayerVisuals();
    }

    void UpdatePlayerVisuals() {
        for (PlayerRuntime& player : m_players) {
            if (TransformComponent* transform = player.transform.TryGet()) {
                transform->position = ToWorld(player.state.position, 0.72f);
                const float yaw = std::atan2(
                    player.state.forward.x,
                    player.state.forward.y
                );
                transform->SetRotationEuler(Vector3(0.0f, yaw, 0.0f));
            }
        }
    }

    void ApplySpawnEvents() {
        for (const SheepRoundup::SheepSpawnEvent& event :
            m_rules.ConsumeSpawnEvents()) {
            if (event.sheepId < m_sheepVisuals.size()) {
                m_sheepVisuals[event.sheepId].spawnPulseRemainingSeconds =
                    SpawnPopSeconds;
            }

            SubmitPresentation(
                RuntimePresentationCommandType::Score,
                event.position,
                event.golden ? 1.65f : 0.35f
            );

            if (event.golden) {
                SetBanner(
                    "GOLDEN SHEEP!  3 POINTS",
                    D2D1::ColorF(1.0f, 0.82f, 0.12f, 1.0f),
                    1.1f
                );
            }
        }
    }

    void UpdateSheepVisuals(float deltaTime) {
        const auto& sheep = m_rules.GetSheep();
        const std::size_t count = std::min(
            sheep.size(),
            m_sheepVisuals.size()
        );
        for (std::size_t index = 0; index < count; ++index) {
            SheepVisual& visual = m_sheepVisuals[index];
            visual.spawnPulseRemainingSeconds = std::max(
                0.0f,
                visual.spawnPulseRemainingSeconds - deltaTime
            );

            TransformComponent* body = visual.bodyTransform.TryGet();
            TransformComponent* halo = visual.haloTransform.TryGet();
            MaterialComponent* bodyMaterial = visual.bodyMaterial.TryGet();
            MaterialComponent* haloMaterial = visual.haloMaterial.TryGet();
            const SheepRoundup::SheepState& state = sheep[index];

            if (!state.IsActive()) {
                HideTransform(body);
                HideTransform(halo);
                continue;
            }

            const float spawnProgress =
                visual.spawnPulseRemainingSeconds > 0.0f
                    ? 1.0f -
                        visual.spawnPulseRemainingSeconds / SpawnPopSeconds
                    : 1.0f;
            const float spawnPop = std::sin(
                std::clamp(spawnProgress, 0.0f, 1.0f) * 3.14159265f
            );
            const float goldenPulse = 0.5f + 0.5f * std::sin(
                m_visualTimeSeconds * 9.0f +
                static_cast<float>(index) * 0.73f
            );

            if (body) {
                body->position = ToWorld(
                    state.position,
                    0.48f + (1.0f - spawnProgress) * 1.1f
                );
                const float yaw = std::atan2(
                    state.direction.x,
                    state.direction.y
                );
                body->SetRotationEuler(Vector3(
                    0.0f,
                    yaw + (state.golden
                        ? m_visualTimeSeconds * 0.35f
                        : 0.0f),
                    0.0f
                ));
                const float spawnScale =
                    0.7f + spawnProgress * 0.3f + spawnPop * 0.18f;
                const float goldenScale = state.golden
                    ? 1.08f + goldenPulse * 0.08f
                    : 1.0f;
                body->scale = Vector3(
                    0.64f * spawnScale * goldenScale,
                    0.72f * spawnScale * goldenScale,
                    0.84f * spawnScale * goldenScale
                );
            }

            if (bodyMaterial) {
                if (state.golden) {
                    bodyMaterial->ShaderID = 1;
                    bodyMaterial->Material.BaseColor =
                        DirectX::XMFLOAT4(1.0f, 0.68f, 0.06f, 1.0f);
                    bodyMaterial->Material.Metallic = 0.62f;
                    bodyMaterial->Material.Roughness = 0.18f;
                    bodyMaterial->Material.EmissiveColor =
                        DirectX::XMFLOAT3(1.0f, 0.48f, 0.02f);
                    bodyMaterial->Material.EmissiveIntensity =
                        3.2f + goldenPulse * 2.8f;
                } else {
                    bodyMaterial->Material.BaseColor =
                        DirectX::XMFLOAT4(0.94f, 0.94f, 0.88f, 1.0f);
                    bodyMaterial->Material.Metallic = 0.0f;
                    bodyMaterial->Material.Roughness = 0.82f;
                    bodyMaterial->Material.EmissiveColor =
                        DirectX::XMFLOAT3(0.08f, 0.08f, 0.06f);
                    bodyMaterial->Material.EmissiveIntensity = 0.08f;
                }
            }

            if (state.golden && halo) {
                halo->position = ToWorld(state.position, 0.12f);
                const float haloScale = 1.15f + goldenPulse * 0.5f;
                halo->scale = Vector3(haloScale, 0.055f, haloScale);
                halo->SetRotationEuler(Vector3(
                    0.0f,
                    m_visualTimeSeconds * 2.8f,
                    0.0f
                ));
                if (haloMaterial) {
                    haloMaterial->Material.EmissiveIntensity =
                        4.0f + goldenPulse * 3.0f;
                }
            } else {
                HideTransform(halo);
            }
        }
    }

    void ApplyScoreEvents() {
        for (const SheepRoundup::SheepScoreEvent& event :
            m_rules.ConsumeScoreEvents()) {
            const float intensity = event.golden
                ? 2.6f
                : event.changedLeader
                    ? 2.0f
                    : 0.85f +
                        static_cast<float>(event.newScore) * 0.04f;
            SubmitPresentation(
                RuntimePresentationCommandType::Score,
                event.position,
                intensity
            );

            if (event.golden) {
                SetBanner(
                    "P" + std::to_string(event.playerId + 1) +
                        " GOLDEN SHEEP  +3!",
                    PlayerColorD2D(event.playerId),
                    1.25f
                );
            } else if (event.changedLeader) {
                SetBanner(
                    "P" + std::to_string(event.playerId + 1) +
                        " TAKES THE LEAD!",
                    PlayerColorD2D(event.playerId),
                    0.9f
                );
            }
        }
    }

    void DrawFlockHud() const {
        MiniGameRuntimeUi ui(GetEntityRef().GetScene());
        if (!ui.IsAvailable() || m_result) {
            return;
        }

        const float panelWidth = std::min(
            620.0f,
            std::max(420.0f, ui.Width() - 48.0f)
        );
        const float x = (ui.Width() - panelWidth) * 0.5f;
        const float y = 182.0f;
        ui.FillPanel(
            x,
            y,
            panelWidth,
            42.0f,
            D2D1::ColorF(0.018f, 0.03f, 0.045f, 0.88f)
        );

        std::ostringstream status;
        status << (m_rules.IsLateRush() ? "FLOCK RUSH" : "FLOCK")
               << "  ACTIVE " << m_rules.GetActiveSheepCount()
               << "   GOLD " << m_rules.GetActiveGoldenSheepCount()
               << "   GOLDEN = 3 POINTS";
        ui.DrawText(
            status.str(),
            x + 14.0f,
            y + 12.0f,
            14.0f,
            m_rules.IsLateRush()
                ? D2D1::ColorF(1.0f, 0.67f, 0.14f, 1.0f)
                : D2D1::ColorF(0.9f, 0.94f, 1.0f, 1.0f),
            false
        );

        if (m_banner.remainingSeconds > 0.0f && !m_banner.text.empty()) {
            const float bannerWidth = 390.0f;
            const float bannerX = ui.Width() - bannerWidth - 24.0f;
            const float bannerY = 234.0f;
            ui.FillPanel(
                bannerX,
                bannerY,
                bannerWidth,
                44.0f,
                D2D1::ColorF(
                    m_banner.color.r * 0.13f,
                    m_banner.color.g * 0.13f,
                    m_banner.color.b * 0.13f,
                    0.9f
                )
            );
            ui.DrawTextCentered(
                m_banner.text,
                bannerX + bannerWidth * 0.5f,
                bannerY + 11.0f,
                17.0f,
                m_banner.color
            );
        }
    }

    void SetBanner(
        std::string text,
        D2D1::ColorF color,
        float durationSeconds
    ) {
        m_banner.text = std::move(text);
        m_banner.color = color;
        m_banner.remainingSeconds = std::max(0.0f, durationSeconds);
    }

    void UpdateWarnings() {
        const float remaining = m_rules.GetRemainingSeconds();
        if (!m_warning10Played && remaining <= 10.0f) {
            m_warning10Played = true;
            SubmitPresentation(RuntimePresentationCommandType::NearMiss);
        }
        if (!m_warning5Played && remaining <= 5.0f) {
            m_warning5Played = true;
            SubmitPresentation(
                RuntimePresentationCommandType::Hit,
                {},
                0.7f
            );
        }
    }

    void UpdateResultInput() {
        if (GetKeyDown('R')) {
            m_transitionSubmitted = SubmitTransition(
                ScenePath,
                TransitionRequest::Retry
            );
        } else if (IsReturnToSelectionPressed()) {
            m_transitionSubmitted = SubmitTransition(
                {},
                TransitionRequest::Selection
            );
        } else if (GetKeyDown('N')) {
            m_transitionSubmitted = SubmitTransition(
                NextScenePath,
                TransitionRequest::NextGame
            );
        }
    }

    static Vec2 FindPenCenter(
        const std::vector<SheepRoundup::SheepPenDefinition>& pens,
        PlayerId playerId
    ) {
        for (const auto& pen : pens) {
            if (pen.owner == playerId) {
                return pen.center;
            }
        }
        return {};
    }

    static void ConfigureGoldenMaterial(
        ComponentRef<MaterialComponent> materialRef,
        float emissiveIntensity
    ) {
        if (MaterialComponent* material = materialRef.TryGet()) {
            material->ShaderID = 1;
            material->Material.BaseColor =
                DirectX::XMFLOAT4(1.0f, 0.72f, 0.08f, 1.0f);
            material->Material.Metallic = 0.55f;
            material->Material.Roughness = 0.16f;
            material->Material.EmissiveColor =
                DirectX::XMFLOAT3(1.0f, 0.5f, 0.02f);
            material->Material.EmissiveIntensity = emissiveIntensity;
        }
    }

    static D2D1::ColorF PlayerColorD2D(PlayerId playerId) {
        const DirectX::XMFLOAT4 color = PlayerColor(playerId);
        return D2D1::ColorF(color.x, color.y, color.z, color.w);
    }

    static void HideTransform(TransformComponent* transform) {
        if (!transform) {
            return;
        }
        transform->position = HiddenPosition();
        transform->scale = Vector3();
    }

    static Vector3 HiddenPosition() {
        return Vector3(0.0f, -1000.0f, 0.0f);
    }

    static Vector3 ToWorld(Vec2 position, float y) {
        return Vector3(position.x, y, position.y);
    }

    void ShutdownRules() {
        if (m_rulesShutdown) {
            return;
        }
        m_rulesShutdown = true;
        for (PlayerRuntime& player : m_players) {
            player.state.inputEnabled = false;
        }
        m_rules.Shutdown();
        SubmitPresentation(RuntimePresentationCommandType::Cancel);
    }

    SheepRoundup::SheepRoundupRules m_rules;
    std::array<PlayerRuntime, PlayerCount> m_players;
    std::vector<MiniGameCpuDecisionClock> m_cpuClocks;
    MiniGamePlayerConfig m_playerConfig{
        .acceleration = 17.0f,
        .deceleration = 21.0f,
        .maximumSpeed = 4.0f,
        .turnResponsiveness = 9.0f,
        .knockbackDamping = 9.0f,
        .collisionRadius = 0.45f
    };
    std::vector<SheepVisual> m_sheepVisuals;
    std::optional<MiniGameResult> m_result;
    Banner m_banner;
    SceneToken m_sceneToken = 0;
    float m_countdownRemainingSeconds = 3.0f;
    float m_visualTimeSeconds = 0.0f;
    bool m_started = false;
    bool m_rulesShutdown = false;
    bool m_transitionSubmitted = false;
    bool m_warning10Played = false;
    bool m_warning5Played = false;
    bool m_lateRushAnnounced = false;
};

} // namespace MiniGameCollection::Runtime
