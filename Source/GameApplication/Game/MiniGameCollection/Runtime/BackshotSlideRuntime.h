#pragma once

#include "Game/MiniGameCollection/Backshot/BackshotRules.h"
#include "Game/MiniGameCollection/Backshot/BackshotSlideMovement.h"
#include "Game/MiniGameCollection/Core/MiniGameCpuDecisionClock.h"
#include "Game/MiniGameCollection/Core/MiniGamePlayerModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace MiniGameCollection::Runtime {

class BackshotSlideRuntime final : public MiniGameRuntimeScriptBase {
public:
    BackshotSlideRuntime()
        : MiniGameRuntimeScriptBase("BackshotSlideRuntime"),
          m_rules(PlayerCount, GameDurationSeconds, m_combatConfig),
          m_board(GridWidth, GridHeight, CellSize, BuildBlockedCells()) {
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, 0);
        SetExecutionOrder(SystemTaskDomain::Render, SystemPhase::Late, 100);
    }

private:
    static constexpr std::size_t PlayerCount = 4;
    static constexpr int GridWidth = 11;
    static constexpr int GridHeight = 7;
    static constexpr float CellSize = 1.55f;
    static constexpr float GameDurationSeconds = 35.0f;
    static constexpr float LandingLockSeconds = 0.10f;
    static constexpr float MinimumSlideSeconds = 0.18f;
    static constexpr float MaximumSlideSeconds = 0.52f;
    static constexpr float SecondsPerSlideCell = 0.072f;
    static constexpr float TracerLifetimeSeconds = 0.24f;
    static constexpr const char* ScenePath =
        "Asset/Game/MiniGameCollection/Scene/Backshot/Backshot.scene";
    static constexpr const char* NextScenePath =
        "Asset/Game/MiniGameCollection/Scene/ColorTerritory/ColorTerritory.scene";

    struct PlayerRuntime {
        MiniGamePlayerState state{};
        Backshot::SlideCell cell{};
        Backshot::SlideCell slideStart{};
        Backshot::SlideCell slideTarget{};
        Backshot::SlideDirection slideDirection = Backshot::SlideDirection::Up;
        float slideElapsedSeconds = 0.0f;
        float slideDurationSeconds = 0.0f;
        float landingLockRemainingSeconds = 0.0f;
        bool sliding = false;

        ComponentRef<TransformComponent> bodyTransform;
        ComponentRef<MaterialComponent> bodyMaterial;
        ComponentRef<TransformComponent> frontTransform;
        ComponentRef<MaterialComponent> frontMaterial;
        ComponentRef<TransformComponent> rearTransform;
        ComponentRef<MaterialComponent> rearMaterial;
        ComponentRef<TransformComponent> pathTransform;
        ComponentRef<MaterialComponent> pathMaterial;
    };

    struct ShotTracer {
        EntityRef entity;
        ComponentRef<TransformComponent> transform;
        ComponentRef<MaterialComponent> material;
        Vec2 from{};
        Vec2 to{};
        float elapsedSeconds = 0.0f;
    };

    struct CpuMoveChoice {
        Backshot::SlideDirection direction = Backshot::SlideDirection::Up;
        float utility = -std::numeric_limits<float>::infinity();
        bool valid = false;
    };

    void OnStart() override {
        m_sceneToken = GetRuntimeSceneToken();
        m_rules.Prepare();
        m_players = {};
        InitializePlayer(0, {1, 1}, Backshot::SlideDirection::Up);
        InitializePlayer(1, {9, 1}, Backshot::SlideDirection::Left);
        InitializePlayer(2, {9, 5}, Backshot::SlideDirection::Down);
        InitializePlayer(3, {1, 5}, Backshot::SlideDirection::Right);

        m_cpuClocks.clear();
        for (std::uint32_t index = 1; index < PlayerCount; ++index) {
            m_cpuClocks.emplace_back(
                index == 1
                    ? CpuDifficultyProfile::Easy()
                    : index == 2
                        ? CpuDifficultyProfile::Normal()
                        : CpuDifficultyProfile::Hard(),
                0x51DE000u + index * 1613u
            );
            m_cpuClocks.back().Reset(0.18f * static_cast<float>(index));
        }

        QueueStageVisuals();
        QueuePlayerVisuals();
        UpdateRuleSnapshots();

        MiniGameRuntimeMailbox::RegisterRulesShutdown(
            m_sceneToken,
            [this]() { ShutdownRules(); }
        );
        SubmitPresentation(RuntimePresentationCommandType::BeginScene);
        SubmitPresentation(RuntimePresentationCommandType::Countdown);

        m_countdownRemainingSeconds = 3.0f;
        m_result.reset();
        m_shotTracers.clear();
        m_nextShotTracerId = 0;
        m_started = false;
        m_rulesShutdown = false;
        m_transitionSubmitted = false;
        m_warning10Played = false;
        m_finalDuelPlayed = false;
    }

    void OnUpdate(float dt) override {
        const float delta = std::max(0.0f, dt);
        UpdateShotTracers(delta);
        if (m_rulesShutdown || m_transitionSubmitted) {
            return;
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
            TickLandingLocks(delta);
            HandleHumanCommand();
            HandleCpuCommands(delta);
            AdvanceSlides(delta);
            UpdateRuleSnapshots();
            HandleHumanShot();
            m_rules.Tick(delta);
            ApplyCombatEvents();
            UpdateWarnings();
            UpdatePlayerVisuals();

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

    void OnFixedUpdate(float dt) override { (void)dt; }

    void OnDraw() override {
        DrawScreenHeader(
            "BACKSHOT",
            "障害物の手前まで滑り、停止位置と背中を読み合え！",
            "矢印 / WASD：一直線にスライド   SPACE：停止中に射撃",
            m_rules.GetRemainingSeconds()
        );
        DrawAliveRow();
        DrawSlideStatus();
        if (m_result) {
            DrawResultPanel(*m_result);
        }
    }

    void OnEditorUpdate(float dt) override { (void)dt; }

    void OnStop() override {
        ClearShotTracers();
        ShutdownRules();
        MiniGameRuntimeMailbox::UnregisterRulesShutdown(m_sceneToken);
        MiniGameRuntimeMailbox::ClearForScene(m_sceneToken);
    }

    static std::vector<Backshot::SlideCell> BuildBlockedCells() {
        return {
            {5, 1},
            {3, 2},
            {7, 2},
            {3, 3},
            {5, 3},
            {7, 3},
            {3, 4},
            {7, 4},
            {5, 5}
        };
    }

    void InitializePlayer(
        std::size_t index,
        Backshot::SlideCell cell,
        Backshot::SlideDirection forward
    ) {
        PlayerRuntime& player = m_players[index];
        player.state = {
            .playerId = static_cast<PlayerId>(index),
            .position = m_board.CellToWorld(cell),
            .velocity = {},
            .forward = Backshot::BackshotSlideBoard::Forward(forward),
            .knockbackVelocity = {},
            .inputEnabled = false,
            .eliminated = false
        };
        player.cell = cell;
        player.slideStart = cell;
        player.slideTarget = cell;
        player.slideDirection = forward;
        player.sliding = false;
    }

    void QueueStageVisuals() {
        const float arenaWidth = static_cast<float>(GridWidth) * CellSize;
        const float arenaHeight = static_cast<float>(GridHeight) * CellSize;
        QueueCube(
            "BackshotSlideFloor",
            Vector3(0.0f, -0.24f, 0.0f),
            Vector3(arenaWidth + 0.4f, 0.3f, arenaHeight + 0.4f),
            DirectX::XMFLOAT4(0.095f, 0.105f, 0.14f, 1.0f)
        );

        const DirectX::XMFLOAT4 gridColor(0.16f, 0.20f, 0.27f, 1.0f);
        for (int x = 0; x <= GridWidth; ++x) {
            const float worldX =
                (static_cast<float>(x) - static_cast<float>(GridWidth) * 0.5f) *
                CellSize;
            QueueCube(
                "BackshotGridVertical_" + std::to_string(x),
                Vector3(worldX, -0.055f, 0.0f),
                Vector3(0.035f, 0.025f, arenaHeight),
                gridColor
            );
        }
        for (int y = 0; y <= GridHeight; ++y) {
            const float worldY =
                (static_cast<float>(y) - static_cast<float>(GridHeight) * 0.5f) *
                CellSize;
            QueueCube(
                "BackshotGridHorizontal_" + std::to_string(y),
                Vector3(0.0f, -0.05f, worldY),
                Vector3(arenaWidth, 0.025f, 0.035f),
                gridColor
            );
        }

        const DirectX::XMFLOAT4 obstacleColor(0.30f, 0.34f, 0.43f, 1.0f);
        const auto blocked = BuildBlockedCells();
        for (std::size_t index = 0; index < blocked.size(); ++index) {
            QueueCube(
                "BackshotSlideObstacle_" + std::to_string(index),
                ToWorld(m_board.CellToWorld(blocked[index]), 0.65f),
                Vector3(CellSize * 0.86f, 1.3f, CellSize * 0.86f),
                obstacleColor
            );
        }

        const DirectX::XMFLOAT4 wallColor(0.055f, 0.065f, 0.09f, 1.0f);
        const float halfWidth = arenaWidth * 0.5f + 0.25f;
        const float halfHeight = arenaHeight * 0.5f + 0.25f;
        QueueCube(
            "BackshotSlideWallNorth",
            Vector3(0.0f, 0.65f, halfHeight),
            Vector3(arenaWidth + 0.7f, 1.3f, 0.32f),
            wallColor
        );
        QueueCube(
            "BackshotSlideWallSouth",
            Vector3(0.0f, 0.65f, -halfHeight),
            Vector3(arenaWidth + 0.7f, 1.3f, 0.32f),
            wallColor
        );
        QueueCube(
            "BackshotSlideWallEast",
            Vector3(halfWidth, 0.65f, 0.0f),
            Vector3(0.32f, 1.3f, arenaHeight),
            wallColor
        );
        QueueCube(
            "BackshotSlideWallWest",
            Vector3(-halfWidth, 0.65f, 0.0f),
            Vector3(0.32f, 1.3f, arenaHeight),
            wallColor
        );
    }

    void QueuePlayerVisuals() {
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            const DirectX::XMFLOAT4 color = PlayerColor(
                static_cast<PlayerId>(index)
            );
            QueueCube(
                "BackshotSlidePlayer_" + std::to_string(index + 1),
                ToWorld(m_players[index].state.position, 0.68f),
                Vector3(0.86f, 1.05f, 1.18f),
                color,
                [this, index](const CubeVisualRefs& refs) {
                    m_players[index].bodyTransform = refs.transform;
                    m_players[index].bodyMaterial = refs.material;
                }
            );
            QueueCube(
                "BackshotSlideFront_" + std::to_string(index + 1),
                HiddenPosition(),
                Vector3(),
                DirectX::XMFLOAT4(0.92f, 0.97f, 1.0f, 1.0f),
                [this, index](const CubeVisualRefs& refs) {
                    m_players[index].frontTransform = refs.transform;
                    m_players[index].frontMaterial = refs.material;
                    ConfigureGlow(
                        refs.material,
                        DirectX::XMFLOAT4(0.92f, 0.97f, 1.0f, 1.0f),
                        2.6f
                    );
                }
            );
            QueueCube(
                "BackshotSlideRear_" + std::to_string(index + 1),
                HiddenPosition(),
                Vector3(),
                DirectX::XMFLOAT4(1.0f, 0.13f, 0.08f, 1.0f),
                [this, index](const CubeVisualRefs& refs) {
                    m_players[index].rearTransform = refs.transform;
                    m_players[index].rearMaterial = refs.material;
                    ConfigureGlow(
                        refs.material,
                        DirectX::XMFLOAT4(1.0f, 0.13f, 0.08f, 1.0f),
                        2.0f
                    );
                }
            );
            QueueCube(
                "BackshotSlidePath_" + std::to_string(index + 1),
                HiddenPosition(),
                Vector3(),
                PlayerColor(static_cast<PlayerId>(index)),
                [this, index, color](const CubeVisualRefs& refs) {
                    m_players[index].pathTransform = refs.transform;
                    m_players[index].pathMaterial = refs.material;
                    ConfigureGlow(refs.material, color, 2.4f);
                }
            );
        }
    }

    void TickLandingLocks(float deltaTime) {
        for (PlayerRuntime& player : m_players) {
            player.landingLockRemainingSeconds = std::max(
                0.0f,
                player.landingLockRemainingSeconds - deltaTime
            );
        }
    }

    void HandleHumanCommand() {
        PlayerRuntime& player = m_players[0];
        if (!CanStartCommand(player)) {
            return;
        }
        if (const auto direction = ReadSlideDirection()) {
            TryStartSlide(0, *direction);
        }
    }

    std::optional<Backshot::SlideDirection> ReadSlideDirection() const {
        if (GetKeyDown(VK_UP) || GetKeyDown('W')) {
            return Backshot::SlideDirection::Up;
        }
        if (GetKeyDown(VK_DOWN) || GetKeyDown('S')) {
            return Backshot::SlideDirection::Down;
        }
        if (GetKeyDown(VK_LEFT) || GetKeyDown('A')) {
            return Backshot::SlideDirection::Left;
        }
        if (GetKeyDown(VK_RIGHT) || GetKeyDown('D')) {
            return Backshot::SlideDirection::Right;
        }
        return std::nullopt;
    }

    void HandleCpuCommands(float deltaTime) {
        const auto& combatants = m_rules.GetCombatants();
        for (std::size_t cpuIndex = 0; cpuIndex < m_cpuClocks.size(); ++cpuIndex) {
            const std::size_t playerIndex = cpuIndex + 1;
            if (playerIndex >= combatants.size() ||
                !combatants[playerIndex].alive ||
                !CanStartCommand(m_players[playerIndex])) {
                continue;
            }

            MiniGameCpuDecisionClock& clock = m_cpuClocks[cpuIndex];
            if (!clock.Tick(deltaTime) || !clock.CanChangeTarget()) {
                continue;
            }

            if (TryCpuShoot(playerIndex)) {
                clock.CommitTarget();
                continue;
            }

            CpuMoveChoice choice = ChooseCpuMove(playerIndex);
            if (choice.valid && clock.ShouldMakeMistake()) {
                const auto alternatives = ValidSlideDirections(playerIndex);
                if (!alternatives.empty()) {
                    choice.direction = alternatives[
                        playerIndex % alternatives.size()
                    ];
                }
            }
            if (choice.valid) {
                TryStartSlide(playerIndex, choice.direction);
            }
            clock.CommitTarget();
        }
    }

    bool CanStartCommand(const PlayerRuntime& player) const noexcept {
        return player.state.inputEnabled &&
            !player.state.eliminated &&
            !player.sliding &&
            player.landingLockRemainingSeconds <= 0.0f;
    }

    bool TryStartSlide(
        std::size_t playerIndex,
        Backshot::SlideDirection direction
    ) {
        if (playerIndex >= PlayerCount ||
            !CanStartCommand(m_players[playerIndex])) {
            return false;
        }

        PlayerRuntime& player = m_players[playerIndex];
        const std::vector<Backshot::SlideCell> reserved =
            BuildReservedCells(playerIndex);
        const Backshot::SlideMove move = m_board.ComputeMove(
            player.cell,
            direction,
            reserved
        );
        if (!move.IsValid()) {
            if (playerIndex == 0) {
                SubmitPresentation(
                    RuntimePresentationCommandType::NearMiss,
                    player.state.position,
                    0.24f
                );
            }
            return false;
        }

        player.slideStart = move.start;
        player.slideTarget = move.stop;
        player.slideDirection = direction;
        player.slideElapsedSeconds = 0.0f;
        player.slideDurationSeconds = std::clamp(
            MinimumSlideSeconds +
                static_cast<float>(move.distanceCells) * SecondsPerSlideCell,
            MinimumSlideSeconds,
            MaximumSlideSeconds
        );
        player.sliding = true;
        player.state.forward = Backshot::BackshotSlideBoard::Forward(direction);
        player.state.velocity = {};
        player.state.knockbackVelocity = {};
        return true;
    }

    void AdvanceSlides(float deltaTime) {
        for (PlayerRuntime& player : m_players) {
            if (!player.sliding || player.state.eliminated) {
                continue;
            }

            player.slideElapsedSeconds += deltaTime;
            const float t = std::clamp(
                player.slideElapsedSeconds /
                    std::max(0.001f, player.slideDurationSeconds),
                0.0f,
                1.0f
            );
            const float eased = 1.0f - std::pow(1.0f - t, 3.0f);
            player.state.position = Lerp(
                m_board.CellToWorld(player.slideStart),
                m_board.CellToWorld(player.slideTarget),
                eased
            );

            if (t >= 1.0f) {
                player.cell = player.slideTarget;
                player.state.position = m_board.CellToWorld(player.cell);
                player.sliding = false;
                player.landingLockRemainingSeconds = LandingLockSeconds;
            }
        }
    }

    std::vector<Backshot::SlideCell> BuildReservedCells(
        std::size_t ignoredPlayer
    ) const {
        std::vector<Backshot::SlideCell> reserved;
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            if (index == ignoredPlayer || m_players[index].state.eliminated) {
                continue;
            }
            const PlayerRuntime& player = m_players[index];
            if (!player.sliding) {
                reserved.push_back(player.cell);
                continue;
            }

            const Backshot::SlideCell step =
                Backshot::BackshotSlideBoard::Step(player.slideDirection);
            Backshot::SlideCell cursor = player.slideStart;
            reserved.push_back(cursor);
            while (cursor != player.slideTarget) {
                cursor = {cursor.x + step.x, cursor.y + step.y};
                reserved.push_back(cursor);
            }
        }
        return reserved;
    }

    std::vector<Backshot::SlideDirection> ValidSlideDirections(
        std::size_t playerIndex
    ) const {
        static constexpr std::array<Backshot::SlideDirection, 4> directions{
            Backshot::SlideDirection::Up,
            Backshot::SlideDirection::Right,
            Backshot::SlideDirection::Down,
            Backshot::SlideDirection::Left
        };
        std::vector<Backshot::SlideDirection> result;
        const auto reserved = BuildReservedCells(playerIndex);
        for (Backshot::SlideDirection direction : directions) {
            if (m_board.ComputeMove(
                    m_players[playerIndex].cell,
                    direction,
                    reserved
                ).IsValid()) {
                result.push_back(direction);
            }
        }
        return result;
    }

    CpuMoveChoice ChooseCpuMove(std::size_t playerIndex) const {
        static constexpr std::array<Backshot::SlideDirection, 4> directions{
            Backshot::SlideDirection::Up,
            Backshot::SlideDirection::Right,
            Backshot::SlideDirection::Down,
            Backshot::SlideDirection::Left
        };
        CpuMoveChoice best;
        const auto reserved = BuildReservedCells(playerIndex);
        const auto& combatants = m_rules.GetCombatants();

        for (Backshot::SlideDirection direction : directions) {
            const Backshot::SlideMove move = m_board.ComputeMove(
                m_players[playerIndex].cell,
                direction,
                reserved
            );
            if (!move.IsValid()) {
                continue;
            }

            const Vec2 destination = m_board.CellToWorld(move.stop);
            const Vec2 forward = Backshot::BackshotSlideBoard::Forward(direction);
            float utility = static_cast<float>(move.distanceCells) * 0.12f;

            for (std::size_t targetIndex = 0;
                 targetIndex < combatants.size();
                 ++targetIndex) {
                if (targetIndex == playerIndex || !combatants[targetIndex].alive) {
                    continue;
                }

                const Vec2 targetPosition = combatants[targetIndex].position;
                const Vec2 toTarget = targetPosition - destination;
                const float distance = Length(toTarget);
                if (distance <= 0.0001f) {
                    continue;
                }
                const Vec2 directionToTarget = toTarget / distance;
                const float aimDot = Dot(forward, directionToTarget);
                const float rearDot = Dot(
                    NormalizeOrZero(combatants[targetIndex].forward),
                    NormalizeOrZero(destination - targetPosition)
                );
                const bool rearOpportunity =
                    rearDot <= m_combatConfig.rearHitDotThreshold;
                const bool aligned =
                    aimDot >= m_combatConfig.forwardAimDotThreshold;
                const bool inRange = distance <= m_combatConfig.range;
                const bool visible = HasLineOfSight(destination, targetPosition);

                utility += rearOpportunity ? 7.5f : 0.25f;
                utility += aligned ? 3.5f : 0.0f;
                utility += inRange ? 1.0f : -0.25f * (distance - m_combatConfig.range);
                utility += visible ? 0.8f : -1.6f;
                if (rearOpportunity && aligned && inRange && visible) {
                    utility += 7.0f;
                }

                const Vec2 enemyToDestination = destination - targetPosition;
                const float enemyAim = Dot(
                    NormalizeOrZero(combatants[targetIndex].forward),
                    NormalizeOrZero(enemyToDestination)
                );
                const float ownRear = Dot(
                    forward,
                    NormalizeOrZero(targetPosition - destination)
                );
                if (enemyAim >= m_combatConfig.forwardAimDotThreshold &&
                    ownRear <= m_combatConfig.rearHitDotThreshold &&
                    visible) {
                    utility -= 5.0f;
                }
            }

            if (!best.valid || utility > best.utility) {
                best = {
                    .direction = direction,
                    .utility = utility,
                    .valid = true
                };
            }
        }
        return best;
    }

    bool TryCpuShoot(std::size_t playerIndex) {
        if (!CanShoot(m_players[playerIndex])) {
            return false;
        }
        const auto target = FindForwardTarget(
            static_cast<PlayerId>(playerIndex)
        );
        if (!target) {
            return false;
        }

        const auto& combatants = m_rules.GetCombatants();
        const Vec2 attackerPosition = combatants[playerIndex].position;
        const Vec2 targetPosition = combatants[*target].position;
        const float rearDot = Dot(
            NormalizeOrZero(combatants[*target].forward),
            NormalizeOrZero(attackerPosition - targetPosition)
        );
        const bool rearOpportunity =
            rearDot <= m_combatConfig.rearHitDotThreshold;
        const bool finalDuelPressure =
            m_rules.GetLivingPlayerCount() <= 2 &&
            m_rules.GetRemainingSeconds() <= 6.0f;
        if (!rearOpportunity && !finalDuelPressure) {
            return false;
        }

        return m_rules.QueueShot(
            static_cast<PlayerId>(playerIndex),
            *target,
            HasLineOfSight(attackerPosition, targetPosition)
        );
    }

    void HandleHumanShot() {
        PlayerRuntime& player = m_players[0];
        if (!GetKeyDown(VK_SPACE) || !CanShoot(player)) {
            return;
        }

        if (const auto target = FindForwardTarget(0)) {
            m_rules.QueueShot(
                0,
                *target,
                HasLineOfSight(
                    player.state.position,
                    m_players[*target].state.position
                )
            );
            return;
        }

        const Vec2 from = player.state.position;
        const Vec2 to = ClipShotToObstacle(
            from,
            from + NormalizeOrZero(player.state.forward) * m_combatConfig.range
        );
        SpawnShotTracer(
            0,
            from,
            to,
            Backshot::ShotResolution::Miss
        );
        SubmitPresentation(
            RuntimePresentationCommandType::NearMiss,
            from,
            0.42f
        );
    }

    bool CanShoot(const PlayerRuntime& player) const noexcept {
        return player.state.inputEnabled &&
            !player.state.eliminated &&
            !player.sliding &&
            player.landingLockRemainingSeconds <= 0.0f;
    }

    std::optional<PlayerId> FindForwardTarget(PlayerId attackerId) const {
        if (attackerId >= PlayerCount) {
            return std::nullopt;
        }
        const PlayerRuntime& attacker = m_players[attackerId];
        const Vec2 attackerForward = NormalizeOrZero(attacker.state.forward);
        float nearestDistance = m_combatConfig.range + 0.001f;
        std::optional<PlayerId> best;

        for (std::size_t index = 0; index < PlayerCount; ++index) {
            if (index == attackerId || m_players[index].state.eliminated) {
                continue;
            }
            const Vec2 toTarget =
                m_players[index].state.position - attacker.state.position;
            const float distance = Length(toTarget);
            if (distance <= 0.0001f || distance > nearestDistance) {
                continue;
            }
            const float aimDot = Dot(
                attackerForward,
                NormalizeOrZero(toTarget)
            );
            if (aimDot < m_combatConfig.forwardAimDotThreshold) {
                continue;
            }
            nearestDistance = distance;
            best = static_cast<PlayerId>(index);
        }
        return best;
    }

    void UpdateRuleSnapshots() {
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            m_rules.UpdateCombatant(
                static_cast<PlayerId>(index),
                m_players[index].state.position,
                m_players[index].state.forward
            );
        }
    }

    void ApplyCombatEvents() {
        for (const Backshot::BackshotEvent& event : m_rules.ConsumeEvents()) {
            if (event.shot.attacker < PlayerCount &&
                event.shot.victim < PlayerCount) {
                const Vec2 from =
                    m_players[event.shot.attacker].state.position;
                Vec2 to = m_players[event.shot.victim].state.position;
                if (event.shot.resolution == Backshot::ShotResolution::Blocked) {
                    to = ClipShotToObstacle(from, to);
                }
                if (event.shot.resolution != Backshot::ShotResolution::Cooldown) {
                    SpawnShotTracer(
                        event.shot.attacker,
                        from,
                        to,
                        event.shot.resolution
                    );
                }
            }

            if (event.eliminatedVictim && event.shot.victim < PlayerCount) {
                PlayerRuntime& victim = m_players[event.shot.victim];
                victim.state.eliminated = true;
                victim.state.inputEnabled = false;
                victim.sliding = false;
                SubmitPresentation(
                    RuntimePresentationCommandType::Hit,
                    victim.state.position,
                    1.6f
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
    }

    bool HasLineOfSight(Vec2 from, Vec2 to) const {
        for (const Backshot::SlideCell cell : BuildBlockedCells()) {
            if (SegmentIntersectsCircle(
                    from,
                    to,
                    m_board.CellToWorld(cell),
                    CellSize * 0.47f
                )) {
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

    Vec2 ClipShotToObstacle(Vec2 from, Vec2 to) const {
        const Vec2 segment = to - from;
        const float segmentLengthSquared = LengthSquared(segment);
        if (segmentLengthSquared <= 0.0001f) {
            return to;
        }

        float nearestT = 1.0f;
        for (const Backshot::SlideCell cell : BuildBlockedCells()) {
            const Vec2 center = m_board.CellToWorld(cell);
            const float radius = CellSize * 0.47f;
            const Vec2 relative = from - center;
            const float a = segmentLengthSquared;
            const float b = 2.0f * Dot(relative, segment);
            const float c = Dot(relative, relative) - radius * radius;
            const float discriminant = b * b - 4.0f * a * c;
            if (discriminant < 0.0f) {
                continue;
            }
            const float root = std::sqrt(discriminant);
            const float inverse = 1.0f / (2.0f * a);
            const float first = (-b - root) * inverse;
            const float second = (-b + root) * inverse;
            if (first >= 0.0f && first <= nearestT) {
                nearestT = first;
            } else if (second >= 0.0f && second <= nearestT) {
                nearestT = second;
            }
        }
        return from + segment * nearestT;
    }

    void SpawnShotTracer(
        PlayerId attacker,
        Vec2 from,
        Vec2 to,
        Backshot::ShotResolution resolution
    ) {
        Vec2 direction = NormalizeOrZero(to - from);
        if (LengthSquared(direction) <= 0.0001f && attacker < PlayerCount) {
            direction = NormalizeOrZero(m_players[attacker].state.forward);
        }
        if (LengthSquared(direction) <= 0.0001f) {
            direction = {0.0f, 1.0f};
        }

        const Vec2 muzzle = from + direction * 0.55f;
        if (DistanceSquared(muzzle, to) <= 0.04f) {
            to = muzzle + direction * 0.25f;
        }
        const DirectX::XMFLOAT4 color = ShotTracerColor(attacker, resolution);
        const std::uint64_t tracerId = m_nextShotTracerId++;
        QueueCube(
            "BackshotSlideShotTracer_" + std::to_string(tracerId),
            ToWorld(muzzle, 0.92f),
            Vector3(0.13f, 0.13f, 0.05f),
            color,
            [this, muzzle, to, color](const CubeVisualRefs& refs) {
                ConfigureGlow(refs.material, color, 4.8f);
                m_shotTracers.push_back({
                    .entity = refs.entity,
                    .transform = refs.transform,
                    .material = refs.material,
                    .from = muzzle,
                    .to = to,
                    .elapsedSeconds = 0.0f
                });
            }
        );
    }

    void UpdateShotTracers(float deltaTime) {
        for (ShotTracer& tracer : m_shotTracers) {
            tracer.elapsedSeconds += deltaTime;
            const float life = std::clamp(
                tracer.elapsedSeconds / TracerLifetimeSeconds,
                0.0f,
                1.0f
            );
            const Vec2 segment = tracer.to - tracer.from;
            const float length = Length(segment);
            const Vec2 direction = NormalizeOrZero(segment);
            const float grow = std::clamp(life * 4.5f, 0.0f, 1.0f);
            const float visibleLength = std::max(0.05f, length * grow);
            const Vec2 center =
                tracer.from + direction * (visibleLength * 0.5f);

            if (TransformComponent* transform = tracer.transform.TryGet()) {
                transform->position = ToWorld(center, 0.92f);
                transform->SetRotationEuler(Vector3(
                    0.0f,
                    std::atan2(direction.x, direction.y),
                    0.0f
                ));
                const float thickness = 0.06f + (1.0f - life) * 0.09f;
                transform->scale = Vector3(
                    thickness,
                    thickness,
                    visibleLength
                );
            }
            if (MaterialComponent* material = tracer.material.TryGet()) {
                material->Material.EmissiveIntensity =
                    0.25f + (1.0f - life) * 4.55f;
            }
        }

        std::erase_if(
            m_shotTracers,
            [this](ShotTracer& tracer) {
                if (tracer.elapsedSeconds < TracerLifetimeSeconds) {
                    return false;
                }
                if (tracer.entity.IsValid()) {
                    QueueDestroyEntity(tracer.entity.GetEntityID());
                }
                return true;
            }
        );
    }

    void ClearShotTracers() {
        for (ShotTracer& tracer : m_shotTracers) {
            if (tracer.entity.IsValid()) {
                QueueDestroyEntity(tracer.entity.GetEntityID());
            }
        }
        m_shotTracers.clear();
    }

    void UpdatePlayerVisuals() {
        const float pulseTime = m_rules.GetElapsedSeconds();
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            PlayerRuntime& player = m_players[index];
            const float yaw = std::atan2(
                player.state.forward.x,
                player.state.forward.y
            );
            const float slidePulse = player.sliding
                ? 0.5f + 0.5f * std::sin(pulseTime * 20.0f)
                : 0.0f;

            if (TransformComponent* body = player.bodyTransform.TryGet()) {
                body->position = ToWorld(player.state.position, 0.68f);
                body->SetRotationEuler(Vector3(0.0f, yaw, 0.0f));
                body->scale = player.state.eliminated
                    ? Vector3(0.06f, 0.06f, 0.06f)
                    : player.sliding
                        ? Vector3(
                            0.82f - slidePulse * 0.05f,
                            0.96f,
                            1.26f + slidePulse * 0.10f
                        )
                        : Vector3(0.86f, 1.05f, 1.18f);
            }
            if (MaterialComponent* material = player.bodyMaterial.TryGet()) {
                material->Material.EmissiveIntensity = player.state.eliminated
                    ? 0.0f
                    : player.sliding
                        ? 0.75f + slidePulse * 0.55f
                        : 0.34f;
            }

            const Vec2 forward = NormalizeOrZero(player.state.forward);
            const Vec2 frontPosition = player.state.position + forward * 0.62f;
            const Vec2 rearPosition = player.state.position - forward * 0.62f;
            UpdateMarkerVisual(
                player.frontTransform,
                frontPosition,
                yaw,
                player.state.eliminated,
                Vector3(0.34f, 0.22f, 0.16f)
            );
            UpdateMarkerVisual(
                player.rearTransform,
                rearPosition,
                yaw,
                player.state.eliminated,
                Vector3(0.50f, 0.32f, 0.13f)
            );

            if (TransformComponent* path = player.pathTransform.TryGet()) {
                if (!player.sliding || player.state.eliminated) {
                    HideTransform(path);
                } else {
                    const Vec2 start = m_board.CellToWorld(player.slideStart);
                    const Vec2 stop = m_board.CellToWorld(player.slideTarget);
                    const Vec2 segment = stop - start;
                    const float length = Length(segment);
                    const Vec2 center = (start + stop) * 0.5f;
                    const Vec2 direction = NormalizeOrZero(segment);
                    path->position = ToWorld(center, 0.035f);
                    path->SetRotationEuler(Vector3(
                        0.0f,
                        std::atan2(direction.x, direction.y),
                        0.0f
                    ));
                    path->scale = Vector3(0.14f, 0.035f, length);
                    if (MaterialComponent* material = player.pathMaterial.TryGet()) {
                        material->Material.EmissiveIntensity =
                            1.8f + slidePulse * 1.8f;
                    }
                }
            }
        }
    }

    static void UpdateMarkerVisual(
        ComponentRef<TransformComponent> reference,
        Vec2 position,
        float yaw,
        bool hidden,
        Vector3 scale
    ) {
        if (TransformComponent* transform = reference.TryGet()) {
            if (hidden) {
                HideTransform(transform);
                return;
            }
            transform->position = ToWorld(position, 0.88f);
            transform->SetRotationEuler(Vector3(0.0f, yaw, 0.0f));
            transform->scale = scale;
        }
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

    void DrawSlideStatus() const {
        MiniGameRuntimeUi ui(GetEntityRef().GetScene());
        if (!ui.IsAvailable() || m_result) {
            return;
        }
        const float width = 430.0f;
        const float x = ui.Width() - width - 22.0f;
        const float y = 182.0f;
        ui.FillPanel(
            x,
            y,
            width,
            44.0f,
            D2D1::ColorF(0.018f, 0.03f, 0.052f, 0.88f)
        );

        const PlayerRuntime& player = m_players[0];
        std::string text;
        D2D1::ColorF color(0.72f, 0.84f, 1.0f, 1.0f);
        if (player.state.eliminated) {
            text = "YOU: OUT";
            color = D2D1::ColorF(1.0f, 0.25f, 0.18f, 1.0f);
        } else if (player.sliding) {
            text = "SLIDING - 移動と射撃は停止までロック";
            color = D2D1::ColorF(0.3f, 0.78f, 1.0f, 1.0f);
        } else if (player.landingLockRemainingSeconds > 0.0f) {
            text = "LANDING...";
            color = D2D1::ColorF(1.0f, 0.78f, 0.2f, 1.0f);
        } else {
            text = "READY - 方向を選ぶか SPACEで射撃";
        }
        ui.DrawText(text, x + 14.0f, y + 12.0f, 15.0f, color, false);
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

    static DirectX::XMFLOAT4 ShotTracerColor(
        PlayerId attacker,
        Backshot::ShotResolution resolution
    ) {
        switch (resolution) {
        case Backshot::ShotResolution::RearElimination:
            return {1.0f, 0.10f, 0.035f, 1.0f};
        case Backshot::ShotResolution::FrontOrSideGuard:
            return {1.0f, 0.82f, 0.16f, 1.0f};
        case Backshot::ShotResolution::Blocked:
            return {0.62f, 0.72f, 0.82f, 1.0f};
        case Backshot::ShotResolution::OutOfRange:
        case Backshot::ShotResolution::OutsideForwardArc:
        case Backshot::ShotResolution::Miss:
            return PlayerColor(attacker);
        case Backshot::ShotResolution::Cooldown:
        default:
            return {0.55f, 0.58f, 0.64f, 1.0f};
        }
    }

    static void ConfigureGlow(
        ComponentRef<MaterialComponent> reference,
        DirectX::XMFLOAT4 color,
        float intensity
    ) {
        if (MaterialComponent* material = reference.TryGet()) {
            material->ShaderID = 1;
            material->Material.BaseColor = color;
            material->Material.Metallic = 0.08f;
            material->Material.Roughness = 0.14f;
            material->Material.EmissiveColor = DirectX::XMFLOAT3(
                color.x,
                color.y,
                color.z
            );
            material->Material.EmissiveIntensity = intensity;
            material->Material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
            material->Material.MaterialFlags &=
                ~MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
        }
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
            player.sliding = false;
        }
        m_rules.Shutdown();
        SubmitPresentation(RuntimePresentationCommandType::Cancel);
    }

    Backshot::BackshotConfig m_combatConfig{
        .range = 9.5f,
        .forwardAimDotThreshold = 0.975f,
        .rearHitDotThreshold = -0.45f,
        .cooldownSeconds = 0.82f
    };
    Backshot::BackshotRules m_rules;
    Backshot::BackshotSlideBoard m_board;
    std::array<PlayerRuntime, PlayerCount> m_players;
    std::vector<MiniGameCpuDecisionClock> m_cpuClocks;
    std::vector<ShotTracer> m_shotTracers;
    std::optional<MiniGameResult> m_result;
    SceneToken m_sceneToken = 0;
    std::uint64_t m_nextShotTracerId = 0;
    float m_countdownRemainingSeconds = 3.0f;
    bool m_started = false;
    bool m_rulesShutdown = false;
    bool m_transitionSubmitted = false;
    bool m_warning10Played = false;
    bool m_finalDuelPlayed = false;
};

} // namespace MiniGameCollection::Runtime
