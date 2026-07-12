#pragma once

#include "Game/MiniGameCollection/Core/MiniGameCpuDecisionClock.h"
#include "Game/MiniGameCollection/Core/MiniGamePlayerModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"
#include "Game/MiniGameCollection/SheepRoundup/SheepRoundupRules.h"

#include <array>
#include <cmath>
#include <optional>
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

    void OnStart() override {
        m_sceneToken = GetRuntimeSceneToken();
        m_rules.Prepare();
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
        m_transitionSubmitted = false;
        m_warning10Played = false;
        m_warning5Played = false;
    }

    void OnUpdate(float dt) override {
        if (m_rulesShutdown || m_transitionSubmitted) {
            return;
        }

        const float delta = std::max(0.0f, dt);
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
            UpdateSheepVisuals();
            return;
        }

        if (!m_rules.IsFinished()) {
            UpdatePlayers(delta);
            m_rules.Tick(delta);
            UpdateSheepVisuals();
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
            UpdateResultInput();
        }
    }

    void OnFixedUpdate(float dt) override {
        (void)dt;
    }

    void OnDraw() override {
        DrawScreenHeader(
            "SHEEP ROUNDUP",
            "羊を自分の囲いへ入れろ！",
            "操作：WASD / 矢印キーで移動",
            m_rules.GetRemainingSeconds()
        );
        DrawScoreRow(m_rules.GetScores());
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
        m_sheepTransforms.assign(sheep.size(), {});
        m_sheepMaterials.assign(sheep.size(), {});
        for (std::size_t index = 0; index < sheep.size(); ++index) {
            QueueCube(
                "Sheep_" + std::to_string(index),
                ToWorld(sheep[index].position, 0.48f),
                Vector3(0.64f, 0.72f, 0.84f),
                DirectX::XMFLOAT4(0.94f, 0.94f, 0.88f, 1.0f),
                [this, index](const CubeVisualRefs& refs) {
                    if (index < m_sheepTransforms.size()) {
                        m_sheepTransforms[index] = refs.transform;
                        m_sheepMaterials[index] = refs.material;
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
            // 囲いの後端を視覚化し、CPUとプレイヤーが押す方向を読みやすくする。
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
        for (const SheepRoundup::SheepState& value : sheep) {
            candidates.push_back({
                .sheepIndex = value.sheepId,
                .sheepPosition = value.position,
                .sheepVelocity = value.velocity,
                .alreadyScored = value.IsScored()
            });
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
                context.remainingTimeRatio =
                    m_rules.GetRemainingSeconds() / GameDurationSeconds;
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
                auto decision = SheepRoundup::SheepCpuEvaluator::ChooseSheep(
                    candidates,
                    context,
                    difficulty,
                    cpuIndex == 0 ? 1.15f : 1.65f
                );
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
                const std::size_t targetIndex = *m_players[playerIndex].targetSheep;
                if (targetIndex >= sheep.size() || sheep[targetIndex].IsScored()) {
                    m_players[playerIndex].targetSheep.reset();
                    clock.ClearTarget();
                } else {
                    const Vec2 toTarget =
                        m_players[playerIndex].interceptPosition -
                        m_players[playerIndex].state.position;
                    inputs[playerIndex].move = NormalizeOrZero(toTarget);
                    if (LengthSquared(toTarget) < 0.2f) {
                        // 回り込み地点へ着いた後は羊へ接近して押し始める。
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

    void UpdateSheepVisuals() {
        const auto& sheep = m_rules.GetSheep();
        const std::size_t count = std::min(sheep.size(), m_sheepTransforms.size());
        for (std::size_t index = 0; index < count; ++index) {
            if (TransformComponent* transform = m_sheepTransforms[index].TryGet()) {
                transform->position = ToWorld(sheep[index].position, 0.48f);
                const float yaw = std::atan2(
                    sheep[index].direction.x,
                    sheep[index].direction.y
                );
                transform->SetRotationEuler(Vector3(0.0f, yaw, 0.0f));
                if (sheep[index].IsScored()) {
                    transform->scale = Vector3(0.48f, 0.48f, 0.48f);
                }
            }
        }
    }

    void ApplyScoreEvents() {
        for (const SheepRoundup::SheepScoreEvent& event :
            m_rules.ConsumeScoreEvents()) {
            if (event.sheepId < m_sheepMaterials.size()) {
                if (MaterialComponent* material =
                    m_sheepMaterials[event.sheepId].TryGet()) {
                    material->Material.BaseColor = PlayerColor(event.playerId);
                    material->Material.EmissiveColor = DirectX::XMFLOAT3(
                        0.2f,
                        0.2f,
                        0.2f
                    );
                    material->Material.EmissiveIntensity =
                        event.changedLeader ? 1.0f : 0.4f;
                }
            }
            Vec2 position{};
            if (event.sheepId < m_rules.GetSheep().size()) {
                position = m_rules.GetSheep()[event.sheepId].position;
            }
            SubmitPresentation(
                RuntimePresentationCommandType::Score,
                position,
                event.changedLeader ? 2.0f :
                    0.8f + static_cast<float>(event.newScore) * 0.08f
            );
        }
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
        } else if (GetKeyDown(VK_ESCAPE)) {
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
    std::vector<ComponentRef<TransformComponent>> m_sheepTransforms;
    std::vector<ComponentRef<MaterialComponent>> m_sheepMaterials;
    std::optional<MiniGameResult> m_result;
    SceneToken m_sceneToken = 0;
    float m_countdownRemainingSeconds = 3.0f;
    bool m_started = false;
    bool m_rulesShutdown = false;
    bool m_transitionSubmitted = false;
    bool m_warning10Played = false;
    bool m_warning5Played = false;
};

} // namespace MiniGameCollection::Runtime
