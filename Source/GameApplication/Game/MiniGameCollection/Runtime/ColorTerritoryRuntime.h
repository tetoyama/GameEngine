#pragma once

#include "Game/MiniGameCollection/ColorTerritory/ColorTerritoryRules.h"
#include "Game/MiniGameCollection/Core/MiniGameCpuDecisionClock.h"
#include "Game/MiniGameCollection/Core/MiniGamePlayerModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"

#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace MiniGameCollection::Runtime {

class ColorTerritoryRuntime final : public MiniGameRuntimeScriptBase {
public:
    ColorTerritoryRuntime()
        : MiniGameRuntimeScriptBase("ColorTerritoryRuntime"),
          m_rules(BoardWidth, BoardHeight, PlayerCount, GameDurationSeconds) {
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, 0);
        SetExecutionOrder(SystemTaskDomain::Render, SystemPhase::Late, 100);
    }

private:
    static constexpr int BoardWidth = 11;
    static constexpr int BoardHeight = 7;
    static constexpr std::size_t PlayerCount = 4;
    static constexpr float GameDurationSeconds = 40.0f;
    static constexpr float TileSpacing = 1.1f;
    static constexpr const char* ScenePath =
        "Asset/Game/MiniGameCollection/Scene/ColorTerritory/ColorTerritory.scene";
    static constexpr const char* NextScenePath =
        "Asset/Game/MiniGameCollection/Scene/SheepRoundup/SheepRoundup.scene";

    struct PlayerRuntime {
        MiniGamePlayerState state{};
        ComponentRef<TransformComponent> transform;
        std::optional<ColorTerritory::TileCoord> cpuTarget;
    };

    void OnStart() override {
        m_sceneToken = GetRuntimeSceneToken();
        m_rules.Prepare();
        m_players[0].state = {
            .playerId = 0,
            .position = {-4.5f, -2.5f},
            .inputEnabled = false
        };
        m_players[1].state = {
            .playerId = 1,
            .position = {4.5f, -2.5f},
            .inputEnabled = false
        };
        m_players[2].state = {
            .playerId = 2,
            .position = {4.5f, 2.5f},
            .inputEnabled = false
        };
        m_players[3].state = {
            .playerId = 3,
            .position = {-4.5f, 2.5f},
            .inputEnabled = false
        };

        m_cpuClocks.clear();
        for (std::uint32_t index = 1; index < PlayerCount; ++index) {
            m_cpuClocks.emplace_back(
                index == 1
                    ? CpuDifficultyProfile::Easy()
                    : index == 2
                        ? CpuDifficultyProfile::Normal()
                        : CpuDifficultyProfile::Hard(),
                0xC010000u + index * 977u
            );
            m_cpuClocks.back().Reset(0.15f * static_cast<float>(index));
        }

        QueueBoardVisuals();
        QueuePlayerVisuals();
        QueueBoundaryVisuals();

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
            return;
        }

        if (!m_rules.IsFinished()) {
            UpdatePlayers(delta);
            m_rules.Tick(delta);
            ApplyPaintEvents();
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
                        1.25f
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
            "COLOR TERRITORY",
            "床を自分の色に塗れ！",
            "操作：WASD / 矢印キーで移動",
            m_rules.GetRemainingSeconds()
        );
        if (const auto* board = m_rules.TryGetBoard()) {
            DrawScoreRow(board->GetScores());
        }
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

    void QueueBoardVisuals() {
        m_tileMaterials.assign(
            static_cast<std::size_t>(BoardWidth * BoardHeight),
            {}
        );
        for (int y = 0; y < BoardHeight; ++y) {
            for (int x = 0; x < BoardWidth; ++x) {
                const std::size_t index = static_cast<std::size_t>(
                    y * BoardWidth + x
                );
                const Vector3 position = TileWorldPosition({x, y});
                QueueCube(
                    "TerritoryTile_" + std::to_string(index),
                    position,
                    Vector3(1.0f, 0.12f, 1.0f),
                    DirectX::XMFLOAT4(0.19f, 0.21f, 0.24f, 1.0f),
                    [this, index](const CubeVisualRefs& refs) {
                        if (index < m_tileMaterials.size()) {
                            m_tileMaterials[index] = refs.material;
                        }
                    }
                );
            }
        }
    }

    void QueuePlayerVisuals() {
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            const PlayerId playerId = static_cast<PlayerId>(index);
            QueueCube(
                "TerritoryPlayer_" + std::to_string(index + 1),
                ToWorld(m_players[index].state.position, 0.68f),
                Vector3(0.72f, 1.1f, 0.72f),
                PlayerColor(playerId),
                [this, index](const CubeVisualRefs& refs) {
                    m_players[index].transform = refs.transform;
                }
            );
        }
    }

    void QueueBoundaryVisuals() {
        const float width = BoardWidth * TileSpacing;
        const float height = BoardHeight * TileSpacing;
        const DirectX::XMFLOAT4 wallColor(0.08f, 0.1f, 0.14f, 1.0f);
        QueueCube(
            "TerritoryWallNorth",
            Vector3(0.0f, 0.65f, height * 0.5f + 0.35f),
            Vector3(width + 0.8f, 1.2f, 0.35f),
            wallColor
        );
        QueueCube(
            "TerritoryWallSouth",
            Vector3(0.0f, 0.65f, -height * 0.5f - 0.35f),
            Vector3(width + 0.8f, 1.2f, 0.35f),
            wallColor
        );
        QueueCube(
            "TerritoryWallEast",
            Vector3(width * 0.5f + 0.35f, 0.65f, 0.0f),
            Vector3(0.35f, 1.2f, height),
            wallColor
        );
        QueueCube(
            "TerritoryWallWest",
            Vector3(-width * 0.5f - 0.35f, 0.65f, 0.0f),
            Vector3(0.35f, 1.2f, height),
            wallColor
        );
    }

    void UpdatePlayers(float deltaTime) {
        std::array<MiniGamePlayerInput, PlayerCount> inputs{};
        inputs[0].move = ReadMovementInput();

        const ColorTerritory::TerritoryBoard* board = m_rules.TryGetBoard();
        if (board) {
            const std::vector<std::uint8_t> crowd = BuildCrowdMap();
            for (std::size_t cpuIndex = 0; cpuIndex < m_cpuClocks.size(); ++cpuIndex) {
                const std::size_t playerIndex = cpuIndex + 1;
                MiniGameCpuDecisionClock& clock = m_cpuClocks[cpuIndex];
                if (clock.Tick(deltaTime) && clock.CanChangeTarget()) {
                    ColorTerritory::CpuTargetContext context;
                    context.self = static_cast<PlayerId>(playerIndex);
                    context.currentTile = WorldToTile(
                        m_players[playerIndex].state.position
                    );
                    context.remainingTimeRatio = m_rules.GetRemainingTimeRatio();
                    context.crowdByTile = crowd;
                    auto decision = ColorTerritory::TerritoryCpuEvaluator::ChooseTarget(
                        *board,
                        context,
                        cpuIndex == 0
                            ? CpuDifficultyProfile::Easy()
                            : cpuIndex == 1
                                ? CpuDifficultyProfile::Normal()
                                : CpuDifficultyProfile::Hard()
                    );
                    if (decision) {
                        m_players[playerIndex].cpuTarget = decision->target;
                        if (clock.ShouldMakeMistake()) {
                            m_players[playerIndex].cpuTarget =
                                ColorTerritory::TileCoord{
                                    std::clamp(
                                        decision->target.x + (playerIndex % 2 == 0 ? 1 : -1),
                                        0,
                                        BoardWidth - 1
                                    ),
                                    decision->target.y
                                };
                        }
                        clock.CommitTarget();
                    }
                }

                if (m_players[playerIndex].cpuTarget) {
                    const Vec2 target = TileWorldPosition2D(
                        *m_players[playerIndex].cpuTarget
                    );
                    const Vec2 toTarget =
                        target - m_players[playerIndex].state.position;
                    inputs[playerIndex].move = NormalizeOrZero(toTarget);
                    if (LengthSquared(toTarget) < 0.18f) {
                        clock.ClearTarget();
                    }
                }
            }
        }

        MovementBounds bounds{
            {-BoardWidth * TileSpacing * 0.5f, -BoardHeight * TileSpacing * 0.5f},
            { BoardWidth * TileSpacing * 0.5f,  BoardHeight * TileSpacing * 0.5f}
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
                    0.7f
                );
            }
        }

        for (std::size_t index = 0; index < PlayerCount; ++index) {
            m_rules.SubmitPlayerTile(
                static_cast<PlayerId>(index),
                WorldToTile(m_players[index].state.position)
            );
        }
        UpdatePlayerVisuals();
    }

    void UpdatePlayerVisuals() {
        for (PlayerRuntime& player : m_players) {
            if (TransformComponent* transform = player.transform.TryGet()) {
                transform->position = ToWorld(player.state.position, 0.68f);
                const float yaw = std::atan2(
                    player.state.forward.x,
                    player.state.forward.y
                );
                transform->SetRotationEuler(Vector3(0.0f, yaw, 0.0f));
            }
        }
    }

    void ApplyPaintEvents() {
        for (const ColorTerritory::TerritoryPaintEvent& event :
            m_rules.ConsumePaintEvents()) {
            const std::size_t index = static_cast<std::size_t>(
                event.tile.y * BoardWidth + event.tile.x
            );
            if (index < m_tileMaterials.size()) {
                if (MaterialComponent* material = m_tileMaterials[index].TryGet()) {
                    material->Material.BaseColor = PlayerColor(event.playerId);
                    material->Material.EmissiveColor = DirectX::XMFLOAT3(
                        0.1f,
                        0.1f,
                        0.1f
                    );
                    material->Material.EmissiveIntensity =
                        event.changedLeader ? 0.8f : 0.25f;
                }
            }
            SubmitPresentation(
                RuntimePresentationCommandType::Score,
                TileWorldPosition2D(event.tile),
                event.changedLeader ? 1.8f : 0.65f
            );
        }
    }

    void UpdateWarnings() {
        const float remaining = m_rules.GetRemainingSeconds();
        if (!m_warning10Played && remaining <= 10.0f) {
            m_warning10Played = true;
            SubmitPresentation(
                RuntimePresentationCommandType::NearMiss,
                {},
                0.7f
            );
        }
        if (!m_warning5Played && remaining <= 5.0f) {
            m_warning5Played = true;
            SubmitPresentation(
                RuntimePresentationCommandType::Hit,
                {},
                0.65f
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

    std::vector<std::uint8_t> BuildCrowdMap() const {
        std::vector<std::uint8_t> crowd(
            static_cast<std::size_t>(BoardWidth * BoardHeight),
            0
        );
        for (const PlayerRuntime& player : m_players) {
            const auto tile = WorldToTile(player.state.position);
            const std::size_t index = static_cast<std::size_t>(
                tile.y * BoardWidth + tile.x
            );
            if (index < crowd.size() && crowd[index] < 255) {
                ++crowd[index];
            }
        }
        return crowd;
    }

    static Vector3 TileWorldPosition(ColorTerritory::TileCoord tile) {
        const Vec2 position = TileWorldPosition2D(tile);
        return Vector3(position.x, 0.0f, position.y);
    }

    static Vec2 TileWorldPosition2D(ColorTerritory::TileCoord tile) {
        return {
            (static_cast<float>(tile.x) - (BoardWidth - 1) * 0.5f) * TileSpacing,
            (static_cast<float>(tile.y) - (BoardHeight - 1) * 0.5f) * TileSpacing
        };
    }

    static ColorTerritory::TileCoord WorldToTile(Vec2 position) {
        const int x = std::clamp(
            static_cast<int>(std::lround(position.x / TileSpacing +
                (BoardWidth - 1) * 0.5f)),
            0,
            BoardWidth - 1
        );
        const int y = std::clamp(
            static_cast<int>(std::lround(position.y / TileSpacing +
                (BoardHeight - 1) * 0.5f)),
            0,
            BoardHeight - 1
        );
        return {x, y};
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

    ColorTerritory::ColorTerritoryRules m_rules;
    std::array<PlayerRuntime, PlayerCount> m_players;
    std::vector<MiniGameCpuDecisionClock> m_cpuClocks;
    MiniGamePlayerConfig m_playerConfig{
        .acceleration = 18.0f,
        .deceleration = 22.0f,
        .maximumSpeed = 4.2f,
        .turnResponsiveness = 10.0f,
        .knockbackDamping = 9.0f,
        .collisionRadius = 0.42f
    };
    std::vector<ComponentRef<MaterialComponent>> m_tileMaterials;
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
