#pragma once

#include "Game/MiniGameCollection/Backshot/BackshotRules.h"
#include "Game/MiniGameCollection/Core/MiniGameCpuDecisionClock.h"
#include "Game/MiniGameCollection/Core/MiniGamePlayerModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"

#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace MiniGameCollection::Runtime {

class BackshotRuntime final : public MiniGameRuntimeScriptBase {
public:
    BackshotRuntime()
        : MiniGameRuntimeScriptBase("BackshotRuntime"),
          m_rules(PlayerCount, GameDurationSeconds, m_combatConfig) {
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, 0);
        SetExecutionOrder(SystemTaskDomain::Render, SystemPhase::Late, 100);
    }

private:
    static constexpr std::size_t PlayerCount = 4;
    static constexpr float GameDurationSeconds = 35.0f;
    static constexpr const char* ScenePath =
        "Asset/Game/MiniGameCollection/Scene/Backshot/Backshot.scene";
    static constexpr const char* NextScenePath =
        "Asset/Game/MiniGameCollection/Scene/ColorTerritory/ColorTerritory.scene";

    struct Obstacle {
        Vec2 center{};
        float radius = 1.0f;
        Vector3 visualScale{1.0f, 1.0f, 1.0f};
    };

    struct PlayerRuntime {
        MiniGamePlayerState state{};
        ComponentRef<TransformComponent> transform;
        ComponentRef<MaterialComponent> material;
        PlayerId cpuTarget = InvalidPlayerId;
        Vec2 desiredPosition{};
        bool cpuShoot = false;
    };

    struct DebugShot {
        Vec2 from{};
        Vec2 to{};
        Backshot::ShotResolution resolution = Backshot::ShotResolution::Miss;
        float remainingSeconds = 0.0f;
    };

    void OnStart() override {
        m_sceneToken = GetRuntimeSceneToken();
        m_rules.Prepare();
        m_players[0].state = {
            .playerId = 0,
            .position = {-6.0f, -4.0f},
            .forward = {0.0f, 1.0f},
            .inputEnabled = false
        };
        m_players[1].state = {
            .playerId = 1,
            .position = {6.0f, -4.0f},
            .forward = {-1.0f, 0.0f},
            .inputEnabled = false
        };
        m_players[2].state = {
            .playerId = 2,
            .position = {6.0f, 4.0f},
            .forward = {0.0f, -1.0f},
            .inputEnabled = false
        };
        m_players[3].state = {
            .playerId = 3,
            .position = {-6.0f, 4.0f},
            .forward = {1.0f, 0.0f},
            .inputEnabled = false
        };

        for (std::size_t index = 0; index < PlayerCount; ++index) {
            m_rules.UpdateCombatant(
                static_cast<PlayerId>(index),
                m_players[index].state.position,
                m_players[index].state.forward
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
                0xBAC0000u + index * 1613u
            );
            m_cpuClocks.back().Reset(0.25f * static_cast<float>(index));
        }

        BuildObstacleDefinitions();
        QueueStageVisuals();
        QueuePlayerVisuals();

        MiniGameRuntimeMailbox::RegisterRulesShutdown(
            m_sceneToken,
            [this]() { ShutdownRules(); }
        );
        SubmitPresentation(RuntimePresentationCommandType::BeginScene);
        SubmitPresentation(RuntimePresentationCommandType::Countdown);
        m_countdownRemainingSeconds = 3.0f;
        m_result.reset();
        m_debugShots.clear();
        m_transitionSubmitted = false;
        m_warning10Played = false;
        m_finalDuelPlayed = false;
    }

    void OnUpdate(float dt) override {
        if (m_rulesShutdown || m_transitionSubmitted) {
            return;
        }

        const float delta = std::max(0.0f, dt);
        UpdateDebugShots(delta);
        if (GetKeyDown(VK_F3)) {
            m_showHitDebug = !m_showHitDebug;
        }

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
            UpdatePlayersAndShots(delta);
            m_rules.Tick(delta);
            ApplyCombatEvents();
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
                        1.4f
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
            "BACKSHOT",
            "相手の背中を撃て！",
            "操作：WASD / 矢印キーで移動  SPACEで射撃",
            m_rules.GetRemainingSeconds()
        );
        DrawAliveRow();
        if (m_showHitDebug) {
            DrawHitDebug();
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

    void BuildObstacleDefinitions() {
        m_obstacles = {
            {{0.0f, 0.0f}, 1.25f, Vector3(2.2f, 2.4f, 2.2f)},
            {{-3.4f, -1.8f}, 0.8f, Vector3(1.5f, 1.05f, 1.5f)},
            {{3.4f, 1.8f}, 0.8f, Vector3(1.5f, 1.05f, 1.5f)},
            {{-3.4f, 2.2f}, 0.65f, Vector3(1.2f, 0.8f, 1.2f)},
            {{3.4f, -2.2f}, 0.65f, Vector3(1.2f, 0.8f, 1.2f)}
        };
    }

    void QueueStageVisuals() {
        QueueCube(
            "BackshotFloor",
            Vector3(0.0f, -0.22f, 0.0f),
            Vector3(19.0f, 0.3f, 14.0f),
            DirectX::XMFLOAT4(0.12f, 0.13f, 0.17f, 1.0f)
        );
        const DirectX::XMFLOAT4 obstacleColor(0.35f, 0.37f, 0.44f, 1.0f);
        for (std::size_t index = 0; index < m_obstacles.size(); ++index) {
            const Obstacle& obstacle = m_obstacles[index];
            QueueCube(
                "BackshotObstacle_" + std::to_string(index),
                ToWorld(obstacle.center, obstacle.visualScale.y * 0.5f),
                obstacle.visualScale,
                obstacleColor
            );
        }

        const DirectX::XMFLOAT4 wall(0.08f, 0.09f, 0.12f, 1.0f);
        QueueCube(
            "BackshotWallNorth",
            Vector3(0.0f, 0.65f, 7.0f),
            Vector3(19.0f, 1.3f, 0.35f),
            wall
        );
        QueueCube(
            "BackshotWallSouth",
            Vector3(0.0f, 0.65f, -7.0f),
            Vector3(19.0f, 1.3f, 0.35f),
            wall
        );
        QueueCube(
            "BackshotWallEast",
            Vector3(9.5f, 0.65f, 0.0f),
            Vector3(0.35f, 1.3f, 14.0f),
            wall
        );
        QueueCube(
            "BackshotWallWest",
            Vector3(-9.5f, 0.65f, 0.0f),
            Vector3(0.35f, 1.3f, 14.0f),
            wall
        );
    }

    void QueuePlayerVisuals() {
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            QueueCube(
                "BackshotPlayer_" + std::to_string(index + 1),
                ToWorld(m_players[index].state.position, 0.72f),
                Vector3(0.76f, 1.18f, 0.9f),
                PlayerColor(static_cast<PlayerId>(index)),
                [this, index](const CubeVisualRefs& refs) {
                    m_players[index].transform = refs.transform;
                    m_players[index].material = refs.material;
                }
            );
        }
    }

    void UpdatePlayersAndShots(float deltaTime) {
        std::array<MiniGamePlayerInput, PlayerCount> inputs{};
        inputs[0].move = ReadMovementInput();
        inputs[0].actionPressed = GetKeyDown(VK_SPACE);

        const auto& combatants = m_rules.GetCombatants();
        for (std::size_t cpuIndex = 0; cpuIndex < m_cpuClocks.size(); ++cpuIndex) {
            const std::size_t playerIndex = cpuIndex + 1;
            if (playerIndex >= combatants.size() ||
                !combatants[playerIndex].alive) {
                continue;
            }

            MiniGameCpuDecisionClock& clock = m_cpuClocks[cpuIndex];
            if (clock.Tick(deltaTime) && clock.CanChangeTarget()) {
                Backshot::BackshotCpuContext context;
                context.self = combatants[playerIndex];
                context.distanceToWallBehindSelf = DistanceToWallBehind(
                    combatants[playerIndex]
                );
                context.remainingTimeRatio =
                    m_rules.GetRemainingSeconds() / GameDurationSeconds;
                context.livingPlayerCount = m_rules.GetLivingPlayerCount();

                for (const auto& candidate : combatants) {
                    if (candidate.playerId == context.self.playerId ||
                        !candidate.alive) {
                        continue;
                    }
                    context.candidates.push_back({
                        .combatant = candidate,
                        .hasLineOfSight = HasLineOfSight(
                            context.self.position,
                            candidate.position
                        ),
                        .isTargetingSelf = IsFacingPlayer(
                            candidate,
                            context.self
                        ),
                        .distanceToWallBehindTarget =
                            DistanceToWallBehind(candidate)
                    });
                }

                const CpuDifficultyProfile difficulty = cpuIndex == 0
                    ? CpuDifficultyProfile::Easy()
                    : cpuIndex == 1
                        ? CpuDifficultyProfile::Normal()
                        : CpuDifficultyProfile::Hard();
                auto decision = Backshot::BackshotCpuEvaluator::Evaluate(
                    context,
                    m_combatConfig,
                    difficulty
                );
                if (decision) {
                    m_players[playerIndex].cpuTarget = decision->target;
                    m_players[playerIndex].desiredPosition = decision->desiredPosition;
                    m_players[playerIndex].cpuShoot = decision->shouldShoot;
                    if (clock.ShouldMakeMistake()) {
                        m_players[playerIndex].cpuShoot = false;
                        m_players[playerIndex].desiredPosition.x +=
                            playerIndex % 2 == 0 ? 1.5f : -1.5f;
                    }
                    clock.CommitTarget();
                }
            }

            if (m_players[playerIndex].cpuTarget != InvalidPlayerId) {
                const Vec2 toPosition =
                    m_players[playerIndex].desiredPosition -
                    m_players[playerIndex].state.position;
                inputs[playerIndex].move = NormalizeOrZero(toPosition);
                inputs[playerIndex].actionPressed =
                    m_players[playerIndex].cpuShoot;
                m_players[playerIndex].cpuShoot = false;
            }
        }

        MovementBounds bounds{{-9.0f, -6.5f}, {9.0f, 6.5f}};
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            MiniGamePlayerModel::Tick(
                m_players[index].state,
                inputs[index],
                m_playerConfig,
                bounds,
                deltaTime
            );
            ResolveObstacleContact(m_players[index].state);
        }
        for (std::size_t lhs = 0; lhs < PlayerCount; ++lhs) {
            for (std::size_t rhs = lhs + 1; rhs < PlayerCount; ++rhs) {
                MiniGamePlayerModel::ResolveSoftContact(
                    m_players[lhs].state,
                    m_players[rhs].state,
                    m_playerConfig,
                    0.9f
                );
            }
        }

        for (std::size_t index = 0; index < PlayerCount; ++index) {
            m_rules.UpdateCombatant(
                static_cast<PlayerId>(index),
                m_players[index].state.position,
                m_players[index].state.forward
            );
        }

        if (inputs[0].actionPressed && m_players[0].state.inputEnabled) {
            if (auto target = FindForwardTarget(0)) {
                m_rules.QueueShot(
                    0,
                    *target,
                    HasLineOfSight(
                        m_players[0].state.position,
                        m_players[*target].state.position
                    )
                );
            } else {
                SubmitPresentation(
                    RuntimePresentationCommandType::NearMiss,
                    m_players[0].state.position,
                    0.55f
                );
            }
        }

        for (std::size_t index = 1; index < PlayerCount; ++index) {
            if (!inputs[index].actionPressed ||
                m_players[index].cpuTarget == InvalidPlayerId ||
                !m_players[index].state.inputEnabled) {
                continue;
            }
            const PlayerId target = m_players[index].cpuTarget;
            if (target < PlayerCount) {
                m_rules.QueueShot(
                    static_cast<PlayerId>(index),
                    target,
                    HasLineOfSight(
                        m_players[index].state.position,
                        m_players[target].state.position
                    )
                );
            }
        }
        UpdatePlayerVisuals();
    }

    std::optional<PlayerId> FindForwardTarget(PlayerId attackerId) const {
        if (attackerId >= PlayerCount) {
            return std::nullopt;
        }
        const PlayerRuntime& attacker = m_players[attackerId];
        float bestDistance = m_combatConfig.range + 0.001f;
        std::optional<PlayerId> best;
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            if (index == attackerId || m_players[index].state.eliminated) {
                continue;
            }
            const Vec2 toTarget =
                m_players[index].state.position - attacker.state.position;
            const float distance = Length(toTarget);
            if (distance <= 0.0001f || distance > bestDistance) {
                continue;
            }
            const float aimDot = Dot(
                NormalizeOrZero(attacker.state.forward),
                NormalizeOrZero(toTarget)
            );
            if (aimDot < m_combatConfig.forwardAimDotThreshold) {
                continue;
            }
            bestDistance = distance;
            best = static_cast<PlayerId>(index);
        }
        return best;
    }

    void ApplyCombatEvents() {
        for (const Backshot::BackshotEvent& event : m_rules.ConsumeEvents()) {
            if (event.shot.attacker < PlayerCount &&
                event.shot.victim < PlayerCount) {
                m_debugShots.push_back({
                    .from = m_players[event.shot.attacker].state.position,
                    .to = m_players[event.shot.victim].state.position,
                    .resolution = event.shot.resolution,
                    .remainingSeconds = 0.65f
                });
            }

            if (event.eliminatedVictim && event.shot.victim < PlayerCount) {
                PlayerRuntime& victim = m_players[event.shot.victim];
                victim.state.eliminated = true;
                victim.state.inputEnabled = false;
                SubmitPresentation(
                    RuntimePresentationCommandType::Hit,
                    victim.state.position,
                    1.5f
                );
                if (event.livingPlayerCount == 2 && !m_finalDuelPlayed) {
                    m_finalDuelPlayed = true;
                    SubmitPresentation(
                        RuntimePresentationCommandType::Success,
                        {},
                        0.9f
                    );
                }
            } else if (
                event.shot.resolution ==
                    Backshot::ShotResolution::FrontOrSideGuard
            ) {
                SubmitPresentation(
                    RuntimePresentationCommandType::NearMiss,
                    event.shot.victim < PlayerCount
                        ? m_players[event.shot.victim].state.position
                        : Vec2{},
                    0.7f
                );
            } else if (
                event.shot.resolution == Backshot::ShotResolution::Blocked
            ) {
                SubmitPresentation(
                    RuntimePresentationCommandType::Hit,
                    event.shot.attacker < PlayerCount
                        ? m_players[event.shot.attacker].state.position
                        : Vec2{},
                    0.45f
                );
            }
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
                transform->scale = player.state.eliminated
                    ? Vector3(0.08f, 0.08f, 0.08f)
                    : Vector3(0.76f, 1.18f, 0.9f);
            }
            if (MaterialComponent* material = player.material.TryGet()) {
                material->Material.EmissiveIntensity =
                    player.state.eliminated ? 0.0f : 0.22f;
            }
        }
    }

    void ResolveObstacleContact(MiniGamePlayerState& state) const {
        for (const Obstacle& obstacle : m_obstacles) {
            Vec2 away = state.position - obstacle.center;
            float distance = Length(away);
            const float minimum = obstacle.radius + m_playerConfig.collisionRadius;
            if (distance >= minimum) {
                continue;
            }
            if (distance <= 0.0001f) {
                away = {1.0f, 0.0f};
                distance = 1.0f;
            }
            const Vec2 normal = away / distance;
            state.position = obstacle.center + normal * minimum;
            MiniGamePlayerModel::ApplyKnockback(
                state,
                normal,
                0.55f,
                2.0f
            );
        }
    }

    bool HasLineOfSight(Vec2 from, Vec2 to) const {
        for (const Obstacle& obstacle : m_obstacles) {
            if (SegmentIntersectsCircle(from, to, obstacle.center, obstacle.radius)) {
                return false;
            }
        }
        return true;
    }

    static bool SegmentIntersectsCircle(
        Vec2 from,
        Vec2 to,
        Vec2 center,
        float radius
    ) {
        const Vec2 segment = to - from;
        const float lengthSquared = LengthSquared(segment);
        if (lengthSquared <= 0.0001f) {
            return DistanceSquared(from, center) <= radius * radius;
        }
        const float t = std::clamp(
            Dot(center - from, segment) / lengthSquared,
            0.0f,
            1.0f
        );
        const Vec2 closest = from + segment * t;
        return DistanceSquared(closest, center) <= radius * radius;
    }

    static bool IsFacingPlayer(
        const Backshot::CombatantSnapshot& attacker,
        const Backshot::CombatantSnapshot& target
    ) {
        const Vec2 direction = NormalizeOrZero(target.position - attacker.position);
        return Dot(NormalizeOrZero(attacker.forward), direction) > 0.72f;
    }

    static float DistanceToWallBehind(
        const Backshot::CombatantSnapshot& combatant
    ) {
        const Vec2 behind = NormalizeOrZero(combatant.forward) * -1.0f;
        float distance = 20.0f;
        if (std::abs(behind.x) > 0.0001f) {
            const float targetX = behind.x > 0.0f ? 9.0f : -9.0f;
            distance = std::min(
                distance,
                std::abs((targetX - combatant.position.x) / behind.x)
            );
        }
        if (std::abs(behind.y) > 0.0001f) {
            const float targetY = behind.y > 0.0f ? 6.5f : -6.5f;
            distance = std::min(
                distance,
                std::abs((targetY - combatant.position.y) / behind.y)
            );
        }
        return distance;
    }

    void UpdateWarnings() {
        const float remaining = m_rules.GetRemainingSeconds();
        if (!m_warning10Played && remaining <= 10.0f) {
            m_warning10Played = true;
            SubmitPresentation(RuntimePresentationCommandType::NearMiss);
        }
        if (!m_finalDuelPlayed && m_rules.GetLivingPlayerCount() <= 2) {
            m_finalDuelPlayed = true;
            SubmitPresentation(
                RuntimePresentationCommandType::Success,
                {},
                0.9f
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

    void UpdateDebugShots(float deltaTime) {
        for (DebugShot& shot : m_debugShots) {
            shot.remainingSeconds = std::max(
                0.0f,
                shot.remainingSeconds - deltaTime
            );
        }
        std::erase_if(
            m_debugShots,
            [](const DebugShot& shot) {
                return shot.remainingSeconds <= 0.0f;
            }
        );
    }

    void DrawAliveRow() const {
        std::vector<int> values;
        values.reserve(PlayerCount);
        const auto& combatants = m_rules.GetCombatants();
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            values.push_back(
                index < combatants.size() && combatants[index].alive ? 1 : 0
            );
        }
        DrawScoreRow(values);
    }

    void DrawHitDebug() const {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 origin(
            viewport->WorkPos.x + 24.0f,
            viewport->WorkPos.y + viewport->WorkSize.y - 244.0f
        );
        const ImVec2 size(300.0f, 210.0f);
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        draw->AddRectFilled(
            origin,
            ImVec2(origin.x + size.x, origin.y + size.y),
            IM_COL32(5, 8, 12, 205),
            8.0f
        );
        draw->AddText(
            ImVec2(origin.x + 10.0f, origin.y + 8.0f),
            IM_COL32(230, 235, 245, 255),
            "F3 HIT DEBUG / REAR CONE"
        );

        const auto toScreen = [origin, size](Vec2 world) {
            return ImVec2(
                origin.x + size.x * (0.5f + world.x / 20.0f),
                origin.y + 28.0f + (size.y - 38.0f) *
                    (0.5f - world.y / 14.0f)
            );
        };

        for (const Obstacle& obstacle : m_obstacles) {
            draw->AddCircleFilled(
                toScreen(obstacle.center),
                obstacle.radius * 12.0f,
                IM_COL32(90, 95, 110, 255),
                20
            );
        }

        const auto& combatants = m_rules.GetCombatants();
        for (const auto& combatant : combatants) {
            const ImVec2 position = toScreen(combatant.position);
            const ImU32 color = combatant.alive
                ? IM_COL32(110, 210, 255, 255)
                : IM_COL32(80, 80, 80, 255);
            draw->AddCircleFilled(position, 6.0f, color, 16);
            const Vec2 forward = NormalizeOrZero(combatant.forward);
            const Vec2 rear = forward * -1.0f;
            const float halfAngle = std::acos(std::clamp(
                -m_combatConfig.rearHitDotThreshold,
                -1.0f,
                1.0f
            ));
            const auto rotate = [](Vec2 value, float angle) {
                const float c = std::cos(angle);
                const float s = std::sin(angle);
                return Vec2{
                    value.x * c - value.y * s,
                    value.x * s + value.y * c
                };
            };
            draw->AddLine(
                position,
                toScreen(combatant.position + forward * 1.4f),
                IM_COL32(255, 255, 255, 255),
                2.0f
            );
            draw->AddLine(
                position,
                toScreen(combatant.position + rotate(rear, halfAngle) * 1.6f),
                IM_COL32(255, 90, 90, 220),
                2.0f
            );
            draw->AddLine(
                position,
                toScreen(combatant.position + rotate(rear, -halfAngle) * 1.6f),
                IM_COL32(255, 90, 90, 220),
                2.0f
            );
        }

        for (const DebugShot& shot : m_debugShots) {
            const ImU32 color =
                shot.resolution == Backshot::ShotResolution::RearElimination
                    ? IM_COL32(255, 60, 60, 255)
                    : shot.resolution == Backshot::ShotResolution::Blocked
                        ? IM_COL32(150, 150, 160, 255)
                        : IM_COL32(255, 220, 90, 255);
            draw->AddLine(toScreen(shot.from), toScreen(shot.to), color, 3.0f);
        }
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

    Backshot::BackshotConfig m_combatConfig{
        .range = 8.0f,
        .forwardAimDotThreshold = 0.92f,
        .rearHitDotThreshold = -0.45f,
        .cooldownSeconds = 0.82f
    };
    Backshot::BackshotRules m_rules;
    std::array<PlayerRuntime, PlayerCount> m_players;
    std::vector<MiniGameCpuDecisionClock> m_cpuClocks;
    MiniGamePlayerConfig m_playerConfig{
        .acceleration = 18.5f,
        .deceleration = 23.0f,
        .maximumSpeed = 4.25f,
        .turnResponsiveness = 10.0f,
        .knockbackDamping = 9.0f,
        .collisionRadius = 0.44f
    };
    std::vector<Obstacle> m_obstacles;
    std::vector<DebugShot> m_debugShots;
    std::optional<MiniGameResult> m_result;
    SceneToken m_sceneToken = 0;
    float m_countdownRemainingSeconds = 3.0f;
    bool m_started = false;
    bool m_rulesShutdown = false;
    bool m_transitionSubmitted = false;
    bool m_warning10Played = false;
    bool m_finalDuelPlayed = false;
    bool m_showHitDebug = false;
};

} // namespace MiniGameCollection::Runtime
