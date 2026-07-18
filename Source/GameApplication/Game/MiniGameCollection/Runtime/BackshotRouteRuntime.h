#pragma once

#include "Game/MiniGameCollection/Backshot/BackshotDynamicEvents.h"
#include "Game/MiniGameCollection/Backshot/BackshotRules.h"
#include "Game/MiniGameCollection/Backshot/BackshotRouteTopology.h"
#include "Game/MiniGameCollection/Core/MiniGamePlayerModel.h"
#include "Game/MiniGameCollection/Core/WorldEventTelegraphModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"
#include "Game/MiniGameCollection/Runtime/WorldEventTelegraphPresenter.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace MiniGameCollection::Runtime {

class BackshotRouteRuntime final : public MiniGameRuntimeScriptBase {
public:
    BackshotRouteRuntime()
        : MiniGameRuntimeScriptBase("BackshotRouteRuntime"),
          m_rules(PlayerCount, GameDurationSeconds, m_combatConfig),
          m_cellSelector(0xBAAC5001u) {
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, 0);
        SetExecutionOrder(SystemTaskDomain::Render, SystemPhase::Late, 100);
    }

private:
    static constexpr std::size_t PlayerCount = 4;
    static constexpr std::size_t BoostPoolCapacity = 2;
    static constexpr std::size_t TracerPoolCapacity = 8;
    static constexpr float GameDurationSeconds = 35.0f;
    static constexpr float CountdownSeconds = 3.0f;
    static constexpr float LandingLockSeconds = 0.10f;
    static constexpr float SecondsPerSlideCell = 0.072f;
    static constexpr float MinimumSlideSeconds = 0.18f;
    static constexpr float MaximumSlideSeconds = 0.52f;
    static constexpr float BoostWarningSeconds = 2.8f;
    static constexpr float FirstBoostElapsedSeconds = 8.0f;
    static constexpr float BoostRepeatSeconds = 7.0f;
    static constexpr float FirstBlockerElapsedSeconds = 12.0f;
    static constexpr float BlockerRepeatSeconds = 10.0f;
    static constexpr const char* ScenePath =
        "Asset/Game/MiniGameCollection/Scene/Backshot/Backshot.scene";
    static constexpr const char* NextScenePath =
        "Asset/Game/MiniGameCollection/Scene/ColorTerritory/ColorTerritory.scene";

    struct PlayerRuntime {
        MiniGamePlayerState state{};
        Backshot::SlideCell cell{};
        Backshot::RouteSlideMove slide{};
        Backshot::BackshotBoostState boost;
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

    struct RouteVisual {
        ComponentRef<TransformComponent> transform;
        ComponentRef<MaterialComponent> material;
        Backshot::SlideCell cell{};
        Backshot::RouteCellType type = Backshot::RouteCellType::Block;
    };

    struct BoostSlot {
        ComponentRef<TransformComponent> transform;
        ComponentRef<MaterialComponent> material;
        Backshot::SlideCell cell{};
        std::uint64_t telegraphId = 0;
        bool warning = false;
        bool active = false;
    };

    struct ShotTracer {
        ComponentRef<TransformComponent> transform;
        ComponentRef<MaterialComponent> material;
        Vec2 from{};
        Vec2 to{};
        float remainingSeconds = 0.0f;
    };

    void OnStart() override {
        m_sceneToken = GetRuntimeSceneToken();
        m_roundIndex = s_roundCounter.fetch_add(1, std::memory_order_relaxed);
        m_board.emplace(Backshot::BackshotRouteLayouts::ForRound(m_roundIndex));
        m_cellSelector = Backshot::BackshotDeterministicCellSelector(
            m_board->Layout().deterministicSeed ^ 0x51DE1001u
        );

        m_rules.Prepare();
        m_players = {};
        const auto& starts = m_board->Layout().playerStarts;
        const std::array<Backshot::SlideDirection, PlayerCount> forwards{
            Backshot::SlideDirection::Up,
            Backshot::SlideDirection::Left,
            Backshot::SlideDirection::Down,
            Backshot::SlideDirection::Right
        };
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            InitializePlayer(index, starts[index], forwards[index]);
        }

        QueueStageVisuals();
        QueueRouteVisuals();
        QueuePlayerVisuals();
        QueueBoostVisuals();
        QueueBlockerVisual();
        QueueTracerVisuals();

        MiniGameRuntimeMailbox::RegisterRulesShutdown(
            m_sceneToken,
            [this]() { ShutdownRules(); }
        );
        SubmitPresentation(RuntimePresentationCommandType::BeginScene);
        SubmitPresentation(RuntimePresentationCommandType::Countdown);

        m_countdownRemainingSeconds = CountdownSeconds;
        m_nextBoostElapsedSeconds = FirstBoostElapsedSeconds;
        m_nextBlockerElapsedSeconds = FirstBlockerElapsedSeconds;
        m_blockerTelegraphId = 0;
        m_result.reset();
        m_started = false;
        m_transitionSubmitted = false;
        m_rulesShutdown = false;
        m_visualTimeSeconds = 0.0f;
        m_nextTelegraphId = 1;
        m_telegraphs.Clear();
        m_blocker.Reset();
        m_board->SetTemporaryBlockedCells({});
        MiniGameRuntimeMailbox::SetMajorTelegraphActive(m_sceneToken, false);
    }

    void OnUpdate(float dt) override {
        if (m_rulesShutdown || m_transitionSubmitted || !m_board) {
            return;
        }
        const float delta = (std::max)(0.0f, dt);
        m_visualTimeSeconds += delta;
        UpdateTracers(delta);

        if (!m_started) {
            m_countdownRemainingSeconds = (std::max)(
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

        if (m_rules.IsFinished()) {
            UpdateResultInput();
            UpdatePlayerVisuals();
            return;
        }

        for (PlayerRuntime& player : m_players) {
            player.boost.Tick(delta);
            player.landingLockRemainingSeconds = (std::max)(
                0.0f,
                player.landingLockRemainingSeconds - delta
            );
        }

        UpdateSlides(delta);
        UpdateRuleSnapshots();
        UpdateHumanInput();
        UpdateCpuPlayers(delta);
        UpdateBoostSchedule();
        UpdateBlockerSchedule(delta);
        UpdateTelegraphs(delta);
        ApplyBoostPickups();
        UpdateRuleSnapshots();

        m_rules.Tick(delta);
        ApplyCombatEvents();

        if (m_rules.IsFinished()) {
            for (PlayerRuntime& player : m_players) {
                player.state.inputEnabled = false;
            }
            m_result = m_rules.BuildResult();
            SubmitPresentation(RuntimePresentationCommandType::Result);
            if (m_result && !m_result->players.empty() &&
                m_result->players.front().playerId == 0 &&
                !m_result->isTie) {
                SubmitPresentation(
                    RuntimePresentationCommandType::Success,
                    {},
                    1.5f
                );
            } else {
                SubmitPresentation(RuntimePresentationCommandType::Failure);
            }
        }
        UpdatePlayerVisuals();
        UpdateBoostVisuals();
        UpdateBlockerVisual();
    }

    void OnFixedUpdate(float dt) override { (void)dt; }

    void OnDraw() override {
        if (!m_board) {
            return;
        }
        DrawScreenHeader(
            "BACKSHOT",
            "接続routeを滑走。Cornerは自動旋回、分岐で停止。背面だけ撃破",
            "矢印 / WASD：route選択   SPACE：射撃",
            m_rules.GetRemainingSeconds()
        );
        DrawAliveRow();
        DrawDynamicStatus();
        WorldEventTelegraphPresenter::Draw(
            GetEntityRef().GetScene(),
            m_telegraphs,
            24.0f,
            18.0f
        );
        if (m_result) {
            DrawResultPanel(*m_result);
        }
    }

    void OnEditorUpdate(float dt) override { (void)dt; }

    void OnStop() override {
        ShutdownRules();
        MiniGameRuntimeMailbox::UnregisterRulesShutdown(m_sceneToken);
        MiniGameRuntimeMailbox::SetMajorTelegraphActive(m_sceneToken, false);
        m_telegraphs.ClearForScene(m_sceneToken);
        MiniGameRuntimeMailbox::ClearForScene(m_sceneToken);
        m_board.reset();
    }

    void InitializePlayer(
        std::size_t index,
        Backshot::SlideCell cell,
        Backshot::SlideDirection facing
    ) {
        PlayerRuntime& player = m_players[index];
        player.cell = cell;
        player.state = {
            .playerId = static_cast<PlayerId>(index),
            .position = m_board->CellToWorld(cell),
            .velocity = {},
            .inputEnabled = false
        };
        player.state.facing = DirectionVector(facing);
        player.boost.Reset();
        m_rules.UpdateCombatant(
            player.state.playerId,
            player.state.position,
            player.state.facing
        );
    }

    void QueueStageVisuals() {
        QueueCube(
            "BackshotRouteFloor",
            Vector3(0.0f, -0.3f, 0.0f),
            Vector3(19.0f, 0.25f, 13.0f),
            DirectX::XMFLOAT4(0.035f, 0.05f, 0.075f, 1.0f)
        );
        const DirectX::XMFLOAT4 wall(0.11f, 0.14f, 0.19f, 1.0f);
        QueueCube("BackshotRouteWallN", Vector3(0.0f, 0.55f, 5.75f), Vector3(19.0f, 1.1f, 0.35f), wall);
        QueueCube("BackshotRouteWallS", Vector3(0.0f, 0.55f, -5.75f), Vector3(19.0f, 1.1f, 0.35f), wall);
        QueueCube("BackshotRouteWallE", Vector3(8.85f, 0.55f, 0.0f), Vector3(0.35f, 1.1f, 12.0f), wall);
        QueueCube("BackshotRouteWallW", Vector3(-8.85f, 0.55f, 0.0f), Vector3(0.35f, 1.1f, 12.0f), wall);
    }

    void QueueRouteVisuals() {
        m_routeVisuals.clear();
        const std::vector<Backshot::SlideCell> cells = m_board->RouteCells();
        m_routeVisuals.resize(cells.size());
        for (std::size_t index = 0; index < cells.size(); ++index) {
            const Backshot::SlideCell cell = cells[index];
            const Backshot::RouteCellDefinition* definition =
                m_board->Layout().TryGet(cell);
            const Backshot::RouteCellType type = definition
                ? definition->type
                : Backshot::RouteCellType::Empty;
            const DirectX::XMFLOAT4 color = RouteColor(type);
            const Vec2 world = m_board->CellToWorld(cell);
            QueueCube(
                "BackshotRouteTile_" + std::to_string(index),
                Vector3(world.x, -0.08f, world.y),
                Vector3(
                    m_board->CellSize() * 0.88f,
                    0.12f,
                    m_board->CellSize() * 0.88f
                ),
                color,
                [this, index, cell, type](const CubeVisualRefs& refs) {
                    if (index >= m_routeVisuals.size()) {
                        return;
                    }
                    m_routeVisuals[index] = {
                        .transform = refs.transform,
                        .material = refs.material,
                        .cell = cell,
                        .type = type
                    };
                }
            );
        }
    }

    void QueuePlayerVisuals() {
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            const DirectX::XMFLOAT4 color = PlayerColor(
                static_cast<PlayerId>(index)
            );
            QueueCube(
                "BackshotRoutePlayer_" + std::to_string(index),
                Vector3(0.0f, -1000.0f, 0.0f),
                Vector3(),
                color,
                [this, index](const CubeVisualRefs& refs) {
                    m_players[index].bodyTransform = refs.transform;
                    m_players[index].bodyMaterial = refs.material;
                }
            );
            QueueCube(
                "BackshotRouteFront_" + std::to_string(index),
                Vector3(0.0f, -1000.0f, 0.0f),
                Vector3(),
                DirectX::XMFLOAT4(0.95f, 0.98f, 1.0f, 1.0f),
                [this, index](const CubeVisualRefs& refs) {
                    m_players[index].frontTransform = refs.transform;
                    m_players[index].frontMaterial = refs.material;
                }
            );
            QueueCube(
                "BackshotRouteRear_" + std::to_string(index),
                Vector3(0.0f, -1000.0f, 0.0f),
                Vector3(),
                DirectX::XMFLOAT4(1.0f, 0.08f, 0.04f, 1.0f),
                [this, index](const CubeVisualRefs& refs) {
                    m_players[index].rearTransform = refs.transform;
                    m_players[index].rearMaterial = refs.material;
                }
            );
            QueueCube(
                "BackshotRouteCommit_" + std::to_string(index),
                Vector3(0.0f, -1000.0f, 0.0f),
                Vector3(),
                DirectX::XMFLOAT4(color.x, color.y, color.z, 0.42f),
                [this, index](const CubeVisualRefs& refs) {
                    m_players[index].pathTransform = refs.transform;
                    m_players[index].pathMaterial = refs.material;
                }
            );
        }
    }

    void QueueBoostVisuals() {
        for (std::size_t index = 0; index < BoostPoolCapacity; ++index) {
            QueueCube(
                "BackshotBoost_" + std::to_string(index),
                Vector3(0.0f, -1000.0f, 0.0f),
                Vector3(),
                DirectX::XMFLOAT4(0.08f, 0.76f, 1.0f, 1.0f),
                [this, index](const CubeVisualRefs& refs) {
                    m_boostSlots[index].transform = refs.transform;
                    m_boostSlots[index].material = refs.material;
                    HideTransform(refs.transform.TryGet());
                }
            );
        }
    }

    void QueueBlockerVisual() {
        QueueCube(
            "BackshotTemporaryRouteBlocker",
            Vector3(0.0f, -1000.0f, 0.0f),
            Vector3(),
            DirectX::XMFLOAT4(1.0f, 0.35f, 0.05f, 1.0f),
            [this](const CubeVisualRefs& refs) {
                m_blockerTransform = refs.transform;
                m_blockerMaterial = refs.material;
                HideTransform(refs.transform.TryGet());
            }
        );
    }

    void QueueTracerVisuals() {
        for (std::size_t index = 0; index < TracerPoolCapacity; ++index) {
            QueueCube(
                "BackshotRouteShotTracer_" + std::to_string(index),
                Vector3(0.0f, -1000.0f, 0.0f),
                Vector3(),
                DirectX::XMFLOAT4(0.7f, 0.92f, 1.0f, 1.0f),
                [this, index](const CubeVisualRefs& refs) {
                    m_tracers[index].transform = refs.transform;
                    m_tracers[index].material = refs.material;
                    HideTransform(refs.transform.TryGet());
                }
            );
        }
    }

    void UpdateSlides(float deltaTime) {
        for (PlayerRuntime& player : m_players) {
            if (!player.sliding) {
                continue;
            }
            player.slideElapsedSeconds = (std::min)(
                player.slideDurationSeconds,
                player.slideElapsedSeconds + deltaTime
            );
            const float normalized = player.slideDurationSeconds > 0.0f
                ? player.slideElapsedSeconds / player.slideDurationSeconds
                : 1.0f;
            const float pathDistance = normalized *
                static_cast<float>(player.slide.DistanceCells());
            const int segment = (std::min)(
                static_cast<int>(player.slide.path.size()) - 2,
                (std::max)(0, static_cast<int>(std::floor(pathDistance)))
            );
            const float local = (std::clamp)(
                pathDistance - static_cast<float>(segment),
                0.0f,
                1.0f
            );
            const Vec2 from = m_board->CellToWorld(
                player.slide.path[static_cast<std::size_t>(segment)]
            );
            const Vec2 to = m_board->CellToWorld(
                player.slide.path[static_cast<std::size_t>(segment + 1)]
            );
            player.state.position = Lerp(from, to, local);

            if (normalized >= 1.0f) {
                player.cell = player.slide.stop;
                player.state.position = m_board->CellToWorld(player.cell);
                player.state.facing = DirectionVector(
                    player.slide.finalDirection
                );
                player.state.velocity = {};
                player.sliding = false;
                player.landingLockRemainingSeconds = LandingLockSeconds;
            }
        }
    }

    void UpdateHumanInput() {
        PlayerRuntime& player = m_players[0];
        if (!IsPlayerControllable(player)) {
            return;
        }

        const std::optional<Backshot::SlideDirection> direction =
            ReadDirectionPressed();
        if (direction) {
            TryBeginSlide(0, *direction);
        }
        if (!player.sliding &&
            player.landingLockRemainingSeconds <= 0.0f &&
            CustomScriptComponent::GetKeyDown(VK_SPACE)) {
            TryShoot(0);
        }
    }

    void UpdateCpuPlayers(float deltaTime) {
        const auto& combatants = m_rules.GetCombatants();
        for (std::size_t index = 1; index < PlayerCount; ++index) {
            m_cpuDecisionRemaining[index] = (std::max)(
                0.0f,
                m_cpuDecisionRemaining[index] - deltaTime
            );
            PlayerRuntime& player = m_players[index];
            if (!IsPlayerControllable(player) ||
                m_cpuDecisionRemaining[index] > 0.0f) {
                continue;
            }
            m_cpuDecisionRemaining[index] = 0.22f +
                static_cast<float>(index) * 0.045f;

            Backshot::BackshotCpuContext context;
            context.self = combatants[index];
            context.remainingTimeRatio = m_rules.GetRemainingSeconds() /
                GameDurationSeconds;
            context.livingPlayerCount = m_rules.GetLivingPlayerCount();
            for (const Backshot::CombatantSnapshot& candidate : combatants) {
                if (candidate.playerId == index || !candidate.alive) {
                    continue;
                }
                context.candidates.push_back({
                    .combatant = candidate,
                    .hasLineOfSight = HasLineOfSight(
                        player.cell,
                        m_players[candidate.playerId].cell
                    ),
                    .isTargetingSelf = IsFacingPlayer(
                        candidate.playerId,
                        static_cast<PlayerId>(index)
                    ),
                    .distanceToWallBehindTarget = 4.0f
                });
            }
            const CpuDifficultyProfile difficulty = index == 1
                ? CpuDifficultyProfile::Easy()
                : index == 2
                    ? CpuDifficultyProfile::Normal()
                    : CpuDifficultyProfile::Hard();
            const auto decision = Backshot::BackshotCpuEvaluator::Evaluate(
                context,
                m_combatConfig,
                difficulty
            );
            if (decision && decision->shouldShoot &&
                player.landingLockRemainingSeconds <= 0.0f) {
                TryShoot(index, decision->target);
                continue;
            }

            const Vec2 desired = decision
                ? decision->desiredPosition
                : Vec2{};
            std::optional<Backshot::SlideDirection> bestDirection;
            float bestDistance = std::numeric_limits<float>::infinity();
            for (Backshot::SlideDirection direction :
                Backshot::BackshotRouteBoard::AllDirections()) {
                const Backshot::RouteSlideMove move = m_board->ComputeMove(
                    player.cell,
                    direction,
                    BuildReservedCells(index)
                );
                if (!move.IsValid()) {
                    continue;
                }
                const float distance = DistanceSquared(
                    m_board->CellToWorld(move.stop),
                    desired
                );
                if (!bestDirection || distance < bestDistance) {
                    bestDirection = direction;
                    bestDistance = distance;
                }
            }
            if (bestDirection) {
                TryBeginSlide(index, *bestDirection);
            } else if (player.landingLockRemainingSeconds <= 0.0f) {
                TryShoot(index);
            }
        }
    }

    bool TryBeginSlide(
        std::size_t playerIndex,
        Backshot::SlideDirection direction
    ) {
        PlayerRuntime& player = m_players[playerIndex];
        if (!IsPlayerControllable(player) || player.sliding) {
            return false;
        }
        const Backshot::RouteSlideMove move = m_board->ComputeMove(
            player.cell,
            direction,
            BuildReservedCells(playerIndex)
        );
        if (!move.IsValid()) {
            SubmitPresentation(
                RuntimePresentationCommandType::NearMiss,
                player.state.position,
                0.35f
            );
            return false;
        }

        player.slide = move;
        player.slideElapsedSeconds = 0.0f;
        player.slideDurationSeconds = player.boost.ResolveSlideDuration(
            move.DistanceCells(),
            SecondsPerSlideCell,
            MinimumSlideSeconds,
            MaximumSlideSeconds,
            m_boostConfig
        );
        player.sliding = true;
        player.state.velocity = DirectionVector(direction) *
            player.boost.ResolveSpeedMultiplier(m_boostConfig);
        player.state.facing = DirectionVector(direction);
        return true;
    }

    void TryShoot(
        std::size_t attackerIndex,
        PlayerId explicitTarget = InvalidPlayerId
    ) {
        PlayerRuntime& attacker = m_players[attackerIndex];
        if (!IsPlayerControllable(attacker) || attacker.sliding ||
            attacker.landingLockRemainingSeconds > 0.0f) {
            return;
        }

        PlayerId target = explicitTarget;
        if (target == InvalidPlayerId) {
            target = FindBestShotTarget(attackerIndex);
        }
        if (target == InvalidPlayerId || target >= PlayerCount) {
            StartTracer(
                attacker.state.position,
                attacker.state.position + attacker.state.facing * 5.5f,
                attacker.state.playerId
            );
            SubmitPresentation(
                RuntimePresentationCommandType::NearMiss,
                attacker.state.position,
                0.55f
            );
            return;
        }

        const bool lineOfSight = HasLineOfSight(
            attacker.cell,
            m_players[target].cell
        );
        if (m_rules.QueueShot(
                static_cast<PlayerId>(attackerIndex),
                target,
                lineOfSight)) {
            StartTracer(
                attacker.state.position,
                m_players[target].state.position,
                attacker.state.playerId
            );
        }
    }

    PlayerId FindBestShotTarget(std::size_t attackerIndex) const {
        const auto& combatants = m_rules.GetCombatants();
        if (attackerIndex >= combatants.size()) {
            return InvalidPlayerId;
        }
        const Backshot::CombatantSnapshot& attacker = combatants[attackerIndex];
        PlayerId best = InvalidPlayerId;
        float bestAim = m_combatConfig.forwardAimDotThreshold;
        float bestDistance = std::numeric_limits<float>::infinity();
        for (const Backshot::CombatantSnapshot& target : combatants) {
            if (!target.alive || target.playerId == attacker.playerId) {
                continue;
            }
            const Vec2 delta = target.position - attacker.position;
            const float distance = Length(delta);
            if (distance <= 0.0001f || distance > m_combatConfig.range) {
                continue;
            }
            const float aim = Dot(
                NormalizeOrZero(attacker.forward),
                delta / distance
            );
            if (aim > bestAim + 0.0001f ||
                (std::abs(aim - bestAim) <= 0.0001f &&
                 distance < bestDistance)) {
                best = target.playerId;
                bestAim = aim;
                bestDistance = distance;
            }
        }
        return best;
    }

    void UpdateBoostSchedule() {
        if (m_rules.GetElapsedSeconds() + 0.0001f <
            m_nextBoostElapsedSeconds) {
            return;
        }
        BoostSlot* slot = nullptr;
        for (BoostSlot& candidate : m_boostSlots) {
            if (!candidate.active && !candidate.warning) {
                slot = &candidate;
                break;
            }
        }
        if (!slot) {
            return;
        }

        const std::vector<Backshot::SlideCell> excluded =
            BuildDynamicEventExcludedCells();
        const auto selected = m_cellSelector.Choose(*m_board, excluded);
        if (!selected) {
            m_nextBoostElapsedSeconds += 1.0f;
            return;
        }

        slot->cell = *selected;
        slot->warning = true;
        slot->telegraphId = NextTelegraphId(1000u);
        m_telegraphs.Submit({
            .id = slot->telegraphId,
            .sceneToken = m_sceneToken,
            .priority = TelegraphPriority::Minor,
            .worldPosition = m_board->CellToWorld(slot->cell),
            .shape = TelegraphShape::Point,
            .radius = m_board->CellSize() * 0.55f,
            .warningSeconds = BoostWarningSeconds,
            .armedSeconds = 0.2f,
            .resolvingSeconds = 0.08f,
            .aftermathSeconds = 0.65f,
            .label = "BOOST INCOMING"
        });
        m_nextBoostElapsedSeconds += BoostRepeatSeconds;
    }

    void UpdateBlockerSchedule(float deltaTime) {
        m_blocker.Tick(deltaTime);
        if (m_blocker.IsActive()) {
            if (m_blocker.BlocksNewRoutes() && m_blocker.Cell()) {
                m_board->SetTemporaryBlockedCells({*m_blocker.Cell()});
            }
        } else {
            m_board->SetTemporaryBlockedCells({});
        }

        for (const Backshot::TemporaryBlockEvent& event :
            m_blocker.ConsumeEvents()) {
            switch (event.type) {
            case Backshot::TemporaryBlockEvent::Type::Closed:
                SubmitPresentation(
                    RuntimePresentationCommandType::Hit,
                    m_board->CellToWorld(event.cell),
                    1.0f
                );
                break;
            case Backshot::TemporaryBlockEvent::Type::Reopened:
                SubmitPresentation(
                    RuntimePresentationCommandType::Success,
                    m_board->CellToWorld(event.cell),
                    0.65f
                );
                break;
            default:
                break;
            }
        }

        if (m_blocker.IsActive() ||
            m_rules.GetElapsedSeconds() + 0.0001f <
                m_nextBlockerElapsedSeconds) {
            return;
        }

        const std::vector<Backshot::SlideCell> occupied =
            BuildOccupiedCells();
        const std::vector<Backshot::SlideCell> reserved =
            BuildAllReservedPathCells();
        std::vector<Backshot::SlideCell> excluded = occupied;
        excluded.insert(excluded.end(), reserved.begin(), reserved.end());
        for (const BoostSlot& slot : m_boostSlots) {
            if (slot.active || slot.warning) {
                excluded.push_back(slot.cell);
            }
        }

        for (int attempt = 0; attempt < 24; ++attempt) {
            const auto selected = m_cellSelector.Choose(*m_board, excluded);
            if (!selected) {
                break;
            }
            if (m_blocker.Schedule(
                    *selected,
                    *m_board,
                    occupied,
                    reserved,
                    {
                        .warningSeconds = 4.0f,
                        .closedSeconds = 6.0f,
                        .reopeningSeconds = 1.0f
                    })) {
                m_blockerTelegraphId = NextTelegraphId(2000u);
                m_telegraphs.Submit({
                    .id = m_blockerTelegraphId,
                    .sceneToken = m_sceneToken,
                    .priority = TelegraphPriority::Major,
                    .worldPosition = m_board->CellToWorld(*selected),
                    .shape = TelegraphShape::Area,
                    .radius = m_board->CellSize() * 0.7f,
                    .warningSeconds = 4.0f,
                    .armedSeconds = 0.0f,
                    .resolvingSeconds = 0.18f,
                    .aftermathSeconds = 1.0f,
                    .label = "ROUTE CLOSURE"
                });
                m_board->SetTemporaryBlockedCells({*selected});
                break;
            }
            excluded.push_back(*selected);
        }
        m_nextBlockerElapsedSeconds += BlockerRepeatSeconds;
    }

    void UpdateTelegraphs(float deltaTime) {
        const std::vector<TelegraphEvent> events = m_telegraphs.Tick(deltaTime);
        for (const TelegraphEvent& event : events) {
            if (event.type != TelegraphEventType::Resolve) {
                continue;
            }
            for (BoostSlot& slot : m_boostSlots) {
                if (slot.telegraphId == event.id && slot.warning) {
                    slot.warning = false;
                    slot.active = true;
                    SubmitPresentation(
                        RuntimePresentationCommandType::NearMiss,
                        m_board->CellToWorld(slot.cell),
                        0.75f
                    );
                }
            }
        }
        MiniGameRuntimeMailbox::SetMajorTelegraphActive(
            m_sceneToken,
            m_telegraphs.HasActiveMajor()
        );
    }

    void ApplyBoostPickups() {
        const auto& combatants = m_rules.GetCombatants();
        for (BoostSlot& slot : m_boostSlots) {
            if (!slot.active) {
                continue;
            }
            for (std::size_t index = 0; index < PlayerCount; ++index) {
                if (!combatants[index].alive || m_players[index].sliding ||
                    m_players[index].cell != slot.cell) {
                    continue;
                }
                m_players[index].boost.Activate(m_boostConfig);
                slot.active = false;
                slot.telegraphId = 0;
                SubmitPresentation(
                    RuntimePresentationCommandType::Success,
                    m_board->CellToWorld(slot.cell),
                    1.0f
                );
                break;
            }
        }
    }

    void UpdateRuleSnapshots() {
        for (PlayerRuntime& player : m_players) {
            m_rules.UpdateCombatant(
                player.state.playerId,
                player.state.position,
                player.state.facing
            );
        }
    }

    void ApplyCombatEvents() {
        for (const Backshot::BackshotEvent& event : m_rules.ConsumeEvents()) {
            const Vec2 position = event.shot.victim < PlayerCount
                ? m_players[event.shot.victim].state.position
                : Vec2{};
            if (event.eliminatedVictim) {
                SubmitPresentation(
                    RuntimePresentationCommandType::Success,
                    position,
                    2.2f
                );
            } else if (
                event.shot.resolution == Backshot::ShotResolution::FrontOrSideGuard
            ) {
                SubmitPresentation(
                    RuntimePresentationCommandType::Hit,
                    position,
                    0.8f
                );
            } else if (
                event.shot.resolution != Backshot::ShotResolution::Cooldown
            ) {
                SubmitPresentation(
                    RuntimePresentationCommandType::NearMiss,
                    position,
                    0.45f
                );
            }
        }
    }

    void UpdatePlayerVisuals() {
        const auto& combatants = m_rules.GetCombatants();
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            PlayerRuntime& player = m_players[index];
            const bool alive = index < combatants.size() && combatants[index].alive;
            if (!alive) {
                HideTransform(player.bodyTransform.TryGet());
                HideTransform(player.frontTransform.TryGet());
                HideTransform(player.rearTransform.TryGet());
                HideTransform(player.pathTransform.TryGet());
                continue;
            }

            const float pulse = 0.5f + 0.5f * std::sin(
                m_visualTimeSeconds * 7.0f + static_cast<float>(index)
            );
            UpdateBodyTransform(player, pulse);
            UpdateFacingMarkers(player, pulse);
            UpdateCommittedPath(player, pulse);
            if (MaterialComponent* material = player.bodyMaterial.TryGet()) {
                material->Material.EmissiveIntensity = player.boost.IsActive()
                    ? 2.8f + pulse * 1.4f
                    : 0.55f + pulse * 0.25f;
            }
        }
    }

    void UpdateBodyTransform(PlayerRuntime& player, float pulse) {
        if (TransformComponent* transform = player.bodyTransform.TryGet()) {
            transform->position = Vector3(
                player.state.position.x,
                0.58f,
                player.state.position.y
            );
            transform->scale = Vector3(
                0.72f + (player.boost.IsActive() ? pulse * 0.12f : 0.0f),
                1.0f,
                0.95f
            );
            transform->SetRotationEuler(Vector3(
                0.0f,
                std::atan2(player.state.facing.x, player.state.facing.y),
                0.0f
            ));
        }
    }

    void UpdateFacingMarkers(PlayerRuntime& player, float pulse) {
        const Vec2 side{-player.state.facing.y, player.state.facing.x};
        (void)side;
        const Vec2 front = player.state.position + player.state.facing * 0.82f;
        const Vec2 rear = player.state.position - player.state.facing * 0.82f;
        if (TransformComponent* transform = player.frontTransform.TryGet()) {
            transform->position = Vector3(front.x, 0.72f, front.y);
            transform->scale = Vector3(0.34f, 0.13f, 0.24f);
        }
        if (TransformComponent* transform = player.rearTransform.TryGet()) {
            transform->position = Vector3(rear.x, 0.72f, rear.y);
            transform->scale = Vector3(
                0.4f + pulse * 0.08f,
                0.13f,
                0.26f + pulse * 0.05f
            );
        }
    }

    void UpdateCommittedPath(PlayerRuntime& player, float pulse) {
        TransformComponent* transform = player.pathTransform.TryGet();
        if (!transform || !player.sliding || player.slide.path.size() < 2) {
            HideTransform(transform);
            return;
        }
        const Vec2 from = m_board->CellToWorld(player.slide.start);
        const Vec2 to = m_board->CellToWorld(player.slide.stop);
        const Vec2 delta = to - from;
        const float distance = Length(delta);
        transform->position = Vector3(
            (from.x + to.x) * 0.5f,
            0.08f,
            (from.y + to.y) * 0.5f
        );
        transform->scale = Vector3(
            0.09f + pulse * 0.025f,
            0.035f,
            (std::max)(0.2f, distance)
        );
        transform->SetRotationEuler(Vector3(
            0.0f,
            std::atan2(delta.x, delta.y),
            0.0f
        ));
    }

    void UpdateBoostVisuals() {
        for (BoostSlot& slot : m_boostSlots) {
            TransformComponent* transform = slot.transform.TryGet();
            MaterialComponent* material = slot.material.TryGet();
            if (!transform || !slot.active) {
                HideTransform(transform);
                continue;
            }
            const Vec2 world = m_board->CellToWorld(slot.cell);
            const float pulse = 0.5f + 0.5f * std::sin(
                m_visualTimeSeconds * 9.0f
            );
            transform->position = Vector3(world.x, 0.45f, world.y);
            transform->scale = Vector3(
                0.55f + pulse * 0.2f,
                0.55f + pulse * 0.2f,
                0.55f + pulse * 0.2f
            );
            transform->SetRotationEuler(Vector3(
                0.0f,
                m_visualTimeSeconds * 2.4f,
                0.0f
            ));
            if (material) {
                material->Material.EmissiveIntensity = 3.2f + pulse * 2.0f;
            }
        }
    }

    void UpdateBlockerVisual() {
        TransformComponent* transform = m_blockerTransform.TryGet();
        MaterialComponent* material = m_blockerMaterial.TryGet();
        if (!transform || !m_blocker.IsActive() || !m_blocker.Cell()) {
            HideTransform(transform);
            return;
        }
        const Vec2 world = m_board->CellToWorld(*m_blocker.Cell());
        const float pulse = 0.5f + 0.5f * std::sin(
            m_visualTimeSeconds * 8.0f
        );
        transform->position = Vector3(world.x, 0.48f, world.y);
        const bool warning =
            m_blocker.Phase() == Backshot::TemporaryBlockPhase::Warning;
        transform->scale = warning
            ? Vector3(1.05f + pulse * 0.25f, 0.12f, 1.05f + pulse * 0.25f)
            : Vector3(1.15f, 1.15f, 1.15f);
        if (material) {
            const DirectX::XMFLOAT4 color = warning
                ? DirectX::XMFLOAT4(1.0f, 0.62f, 0.08f, 1.0f)
                : m_blocker.Phase() == Backshot::TemporaryBlockPhase::Reopening
                    ? DirectX::XMFLOAT4(0.2f, 0.9f, 0.42f, 1.0f)
                    : DirectX::XMFLOAT4(1.0f, 0.15f, 0.06f, 1.0f);
            material->Material.BaseColor = color;
            material->Material.EmissiveColor = DirectX::XMFLOAT3(
                color.x,
                color.y,
                color.z
            );
            material->Material.EmissiveIntensity = 2.5f + pulse * 2.0f;
        }
    }

    void StartTracer(Vec2 from, Vec2 to, PlayerId playerId) {
        ShotTracer* selected = &m_tracers[0];
        for (ShotTracer& tracer : m_tracers) {
            if (tracer.remainingSeconds <= 0.0f) {
                selected = &tracer;
                break;
            }
            if (tracer.remainingSeconds < selected->remainingSeconds) {
                selected = &tracer;
            }
        }
        selected->from = from;
        selected->to = to;
        selected->remainingSeconds = 0.22f;
        if (MaterialComponent* material = selected->material.TryGet()) {
            const DirectX::XMFLOAT4 color = PlayerColor(playerId);
            material->Material.BaseColor = color;
            material->Material.EmissiveColor = DirectX::XMFLOAT3(
                color.x,
                color.y,
                color.z
            );
            material->Material.EmissiveIntensity = 4.2f;
        }
    }

    void UpdateTracers(float deltaTime) {
        for (ShotTracer& tracer : m_tracers) {
            tracer.remainingSeconds = (std::max)(
                0.0f,
                tracer.remainingSeconds - deltaTime
            );
            TransformComponent* transform = tracer.transform.TryGet();
            if (!transform || tracer.remainingSeconds <= 0.0f) {
                HideTransform(transform);
                continue;
            }
            const Vec2 delta = tracer.to - tracer.from;
            const float distance = Length(delta);
            transform->position = Vector3(
                (tracer.from.x + tracer.to.x) * 0.5f,
                0.62f,
                (tracer.from.y + tracer.to.y) * 0.5f
            );
            transform->scale = Vector3(0.055f, 0.055f, distance);
            transform->SetRotationEuler(Vector3(
                0.0f,
                std::atan2(delta.x, delta.y),
                0.0f
            ));
        }
    }

    void DrawAliveRow() const {
        const auto& combatants = m_rules.GetCombatants();
        std::vector<int> eliminations = m_rules.GetEliminations();
        DrawScoreRow(eliminations);
        MiniGameRuntimeUi ui(GetEntityRef().GetScene());
        if (!ui.IsAvailable()) {
            return;
        }
        const float y = 180.0f;
        for (std::size_t index = 0; index < combatants.size(); ++index) {
            const DirectX::XMFLOAT4 color = PlayerColor(
                static_cast<PlayerId>(index)
            );
            ui.DrawText(
                combatants[index].alive ? "ALIVE" : "OUT",
                28.0f + static_cast<float>(index) * 112.0f,
                y,
                13.0f,
                combatants[index].alive
                    ? D2D1::ColorF(color.x, color.y, color.z, 1.0f)
                    : D2D1::ColorF(0.42f, 0.45f, 0.5f, 1.0f),
                false
            );
        }
    }

    void DrawDynamicStatus() const {
        MiniGameRuntimeUi ui(GetEntityRef().GetScene());
        if (!ui.IsAvailable() || m_result || !m_board) {
            return;
        }
        const float width = 410.0f;
        const float x = ui.Width() - width - 22.0f;
        const float y = ui.Height() - 126.0f;
        ui.FillPanel(
            x,
            y,
            width,
            94.0f,
            D2D1::ColorF(0.015f, 0.024f, 0.045f, 0.88f)
        );
        ui.DrawText(
            "LAYOUT  " + m_board->Layout().name,
            x + 14.0f,
            y + 10.0f,
            15.0f,
            D2D1::ColorF(0.72f, 0.86f, 1.0f, 1.0f),
            false
        );
        if (m_players[0].boost.IsActive()) {
            ui.DrawText(
                "BOOST  " + FormatSeconds(
                    m_players[0].boost.RemainingSeconds()) + "s  ×1.4",
                x + 14.0f,
                y + 36.0f,
                16.0f,
                D2D1::ColorF(0.2f, 0.82f, 1.0f, 1.0f),
                false
            );
        } else {
            ui.DrawText(
                "BOOST  --",
                x + 14.0f,
                y + 36.0f,
                14.0f,
                D2D1::ColorF(0.48f, 0.56f, 0.68f, 1.0f),
                false
            );
        }
        if (m_blocker.IsActive()) {
            ui.DrawText(
                "ROUTE BLOCK  " + FormatSeconds(
                    m_blocker.RemainingSeconds()) + "s",
                x + 14.0f,
                y + 62.0f,
                15.0f,
                D2D1::ColorF(1.0f, 0.58f, 0.1f, 1.0f),
                false
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

    std::vector<Backshot::SlideCell> BuildReservedCells(
        std::size_t movingPlayer
    ) const {
        std::vector<Backshot::SlideCell> reserved;
        const auto& combatants = m_rules.GetCombatants();
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            if (index == movingPlayer || index >= combatants.size() ||
                !combatants[index].alive) {
                continue;
            }
            reserved.push_back(m_players[index].cell);
            if (m_players[index].sliding) {
                reserved.insert(
                    reserved.end(),
                    m_players[index].slide.path.begin(),
                    m_players[index].slide.path.end()
                );
            }
        }
        return reserved;
    }

    std::vector<Backshot::SlideCell> BuildOccupiedCells() const {
        std::vector<Backshot::SlideCell> cells;
        const auto& combatants = m_rules.GetCombatants();
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            if (index < combatants.size() && combatants[index].alive) {
                cells.push_back(m_players[index].cell);
            }
        }
        return cells;
    }

    std::vector<Backshot::SlideCell> BuildAllReservedPathCells() const {
        std::vector<Backshot::SlideCell> cells;
        for (const PlayerRuntime& player : m_players) {
            if (player.sliding) {
                cells.insert(
                    cells.end(),
                    player.slide.path.begin(),
                    player.slide.path.end()
                );
            }
        }
        return cells;
    }

    std::vector<Backshot::SlideCell> BuildDynamicEventExcludedCells() const {
        std::vector<Backshot::SlideCell> excluded = BuildOccupiedCells();
        const auto reserved = BuildAllReservedPathCells();
        excluded.insert(excluded.end(), reserved.begin(), reserved.end());
        if (m_blocker.Cell()) {
            excluded.push_back(*m_blocker.Cell());
        }
        for (const BoostSlot& slot : m_boostSlots) {
            if (slot.active || slot.warning) {
                excluded.push_back(slot.cell);
            }
        }
        return excluded;
    }

    bool HasLineOfSight(
        Backshot::SlideCell from,
        Backshot::SlideCell to
    ) const {
        if (from.x != to.x && from.y != to.y) {
            return false;
        }
        const int stepX = to.x == from.x ? 0 : (to.x > from.x ? 1 : -1);
        const int stepY = to.y == from.y ? 0 : (to.y > from.y ? 1 : -1);
        Backshot::SlideCell cursor{from.x + stepX, from.y + stepY};
        while (cursor != to) {
            if (m_board->IsBlocked(cursor)) {
                return false;
            }
            cursor.x += stepX;
            cursor.y += stepY;
        }
        return true;
    }

    bool IsFacingPlayer(PlayerId from, PlayerId to) const {
        if (from >= PlayerCount || to >= PlayerCount) {
            return false;
        }
        const Vec2 direction = NormalizeOrZero(
            m_players[to].state.position - m_players[from].state.position
        );
        return Dot(m_players[from].state.facing, direction) >= 0.9f;
    }

    bool IsPlayerControllable(const PlayerRuntime& player) const {
        const auto& combatants = m_rules.GetCombatants();
        return player.state.inputEnabled &&
            player.state.playerId < combatants.size() &&
            combatants[player.state.playerId].alive;
    }

    std::optional<Backshot::SlideDirection> ReadDirectionPressed() const {
        if (CustomScriptComponent::GetKeyDown(VK_UP) ||
            CustomScriptComponent::GetKeyDown('W')) {
            return Backshot::SlideDirection::Up;
        }
        if (CustomScriptComponent::GetKeyDown(VK_RIGHT) ||
            CustomScriptComponent::GetKeyDown('D')) {
            return Backshot::SlideDirection::Right;
        }
        if (CustomScriptComponent::GetKeyDown(VK_DOWN) ||
            CustomScriptComponent::GetKeyDown('S')) {
            return Backshot::SlideDirection::Down;
        }
        if (CustomScriptComponent::GetKeyDown(VK_LEFT) ||
            CustomScriptComponent::GetKeyDown('A')) {
            return Backshot::SlideDirection::Left;
        }
        return std::nullopt;
    }

    static Vec2 DirectionVector(
        Backshot::SlideDirection direction
    ) noexcept {
        switch (direction) {
        case Backshot::SlideDirection::Up: return {0.0f, 1.0f};
        case Backshot::SlideDirection::Right: return {1.0f, 0.0f};
        case Backshot::SlideDirection::Down: return {0.0f, -1.0f};
        case Backshot::SlideDirection::Left: return {-1.0f, 0.0f};
        }
        return {0.0f, 1.0f};
    }

    static DirectX::XMFLOAT4 RouteColor(
        Backshot::RouteCellType type
    ) noexcept {
        switch (type) {
        case Backshot::RouteCellType::Cross:
            return {0.72f, 0.52f, 0.12f, 1.0f};
        case Backshot::RouteCellType::TJunctionN:
        case Backshot::RouteCellType::TJunctionE:
        case Backshot::RouteCellType::TJunctionS:
        case Backshot::RouteCellType::TJunctionW:
            return {0.42f, 0.28f, 0.64f, 1.0f};
        case Backshot::RouteCellType::CornerNE:
        case Backshot::RouteCellType::CornerNW:
        case Backshot::RouteCellType::CornerSE:
        case Backshot::RouteCellType::CornerSW:
            return {0.12f, 0.42f, 0.58f, 1.0f};
        case Backshot::RouteCellType::DeadEndN:
        case Backshot::RouteCellType::DeadEndE:
        case Backshot::RouteCellType::DeadEndS:
        case Backshot::RouteCellType::DeadEndW:
            return {0.5f, 0.18f, 0.28f, 1.0f};
        default:
            return {0.14f, 0.24f, 0.34f, 1.0f};
        }
    }

    static void HideTransform(TransformComponent* transform) {
        if (!transform) {
            return;
        }
        transform->position = Vector3(0.0f, -1000.0f, 0.0f);
        transform->scale = Vector3();
    }

    static std::string FormatSeconds(float seconds) {
        const int tenths = static_cast<int>(
            (std::max)(0.0f, seconds) * 10.0f + 0.5f
        );
        return std::to_string(tenths / 10) + "." +
            std::to_string(tenths % 10);
    }

    std::uint64_t NextTelegraphId(std::uint64_t family) noexcept {
        return family * 100000u + m_nextTelegraphId++;
    }

    void ShutdownRules() {
        if (m_rulesShutdown) {
            return;
        }
        m_rules.Shutdown();
        m_rulesShutdown = true;
    }

    inline static std::atomic<std::uint32_t> s_roundCounter{0};

    Backshot::BackshotConfig m_combatConfig{};
    Backshot::BackshotRules m_rules;
    Backshot::BackshotBoostConfig m_boostConfig{};
    std::optional<Backshot::BackshotRouteBoard> m_board;
    std::array<PlayerRuntime, PlayerCount> m_players{};
    std::array<float, PlayerCount> m_cpuDecisionRemaining{};
    std::vector<RouteVisual> m_routeVisuals;
    std::array<BoostSlot, BoostPoolCapacity> m_boostSlots{};
    Backshot::TemporaryRouteBlockerModel m_blocker;
    Backshot::BackshotDeterministicCellSelector m_cellSelector;
    WorldEventTelegraphModel m_telegraphs;
    std::array<ShotTracer, TracerPoolCapacity> m_tracers{};
    ComponentRef<TransformComponent> m_blockerTransform;
    ComponentRef<MaterialComponent> m_blockerMaterial;
    std::optional<MiniGameResult> m_result;
    SceneToken m_sceneToken = 0;
    std::uint32_t m_roundIndex = 0;
    std::uint64_t m_nextTelegraphId = 1;
    std::uint64_t m_blockerTelegraphId = 0;
    float m_countdownRemainingSeconds = CountdownSeconds;
    float m_nextBoostElapsedSeconds = FirstBoostElapsedSeconds;
    float m_nextBlockerElapsedSeconds = FirstBlockerElapsedSeconds;
    float m_visualTimeSeconds = 0.0f;
    bool m_started = false;
    bool m_transitionSubmitted = false;
    bool m_rulesShutdown = false;
};

} // namespace MiniGameCollection::Runtime
