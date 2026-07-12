#pragma once

#include "Game/MiniGameCollection/ColorTerritory/ColorTerritoryItemModel.h"
#include "Game/MiniGameCollection/ColorTerritory/ColorTerritoryRules.h"
#include "Game/MiniGameCollection/Core/MiniGameCpuDecisionClock.h"
#include "Game/MiniGameCollection/Core/MiniGamePlayerModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"

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
    static constexpr std::size_t ItemSlotCount = 3;
    static constexpr float GameDurationSeconds = 40.0f;
    static constexpr float TileSpacing = 1.1f;
    static constexpr float ItemPickupRadius = 0.72f;
    static constexpr float StarTouchRadius = 0.92f;
    static constexpr float ItemFallHeight = 7.5f;
    static constexpr const char* ScenePath =
        "Asset/Game/MiniGameCollection/Scene/ColorTerritory/ColorTerritory.scene";
    static constexpr const char* NextScenePath =
        "Asset/Game/MiniGameCollection/Scene/SheepRoundup/SheepRoundup.scene";

    struct TileVisual {
        ComponentRef<MaterialComponent> material;
        DirectX::XMFLOAT4 baseColor{0.19f, 0.21f, 0.24f, 1.0f};
        float flashRemainingSeconds = 0.0f;
        float flashDurationSeconds = 0.0f;
        float flashIntensity = 0.0f;
    };

    struct PlayerRuntime {
        MiniGamePlayerState state{};
        ComponentRef<TransformComponent> transform;
        ComponentRef<MaterialComponent> material;
        ComponentRef<TransformComponent> starAuraTransform;
        ComponentRef<MaterialComponent> starAuraMaterial;
        std::optional<ColorTerritory::TileCoord> cpuTarget;
        ColorTerritory::TerritoryPlayerPowerState power;
    };

    struct ItemVisual {
        ComponentRef<TransformComponent> coreTransform;
        ComponentRef<MaterialComponent> coreMaterial;
        ComponentRef<TransformComponent> haloTransform;
        ComponentRef<MaterialComponent> haloMaterial;
        ComponentRef<TransformComponent> beamTransform;
        ComponentRef<MaterialComponent> beamMaterial;
    };

    struct ItemRuntime {
        ColorTerritory::TerritoryItemType type =
            ColorTerritory::TerritoryItemType::Bomb;
        ColorTerritory::TerritoryItemPhase phase =
            ColorTerritory::TerritoryItemPhase::Inactive;
        ColorTerritory::TileCoord tile{};
        std::optional<PlayerId> owner;
        float remainingSeconds = 0.0f;
        float totalSeconds = 0.0f;
        float ageSeconds = 0.0f;
        std::uint32_t serial = 0;
        ItemVisual visual;
    };

    struct EventBanner {
        std::string text;
        D2D1::ColorF color = D2D1::ColorF(1.0f, 0.85f, 0.25f, 1.0f);
        float remainingSeconds = 0.0f;
        float durationSeconds = 0.0f;
    };

    void OnStart() override {
        m_sceneToken = GetRuntimeSceneToken();
        m_rules.Prepare();
        m_players = {};
        m_players[0].state = {.playerId = 0, .position = {-4.5f, -2.5f}};
        m_players[1].state = {.playerId = 1, .position = {4.5f, -2.5f}};
        m_players[2].state = {.playerId = 2, .position = {4.5f, 2.5f}};
        m_players[3].state = {.playerId = 3, .position = {-4.5f, 2.5f}};
        for (PlayerRuntime& player : m_players) {
            player.power.Reset();
            player.state.inputEnabled = false;
        }

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

        m_items = {};
        m_itemRandom.Reset(0xC01017E1u);
        m_nextItemSpawnSeconds = 0.35f;
        m_nextItemSerial = 1;
        m_itemRushStarted = false;
        m_eventBanner = {};
        m_visualTimeSeconds = 0.0f;

        QueueBoardVisuals();
        QueuePlayerVisuals();
        QueueBoundaryVisuals();
        QueueItemVisuals();
        QueueStarAuraVisuals();

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
        m_started = false;
        m_rulesShutdown = false;
    }

    void OnUpdate(float dt) override {
        if (m_rulesShutdown || m_transitionSubmitted) {
            return;
        }

        const float delta = std::max(0.0f, dt);
        m_visualTimeSeconds += delta;
        m_eventBanner.remainingSeconds = std::max(
            0.0f,
            m_eventBanner.remainingSeconds - delta
        );
        for (PlayerRuntime& player : m_players) {
            player.power.Tick(delta);
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
            UpdateTileVisuals(delta);
            UpdateItemVisuals();
            UpdatePlayerVisuals();
            return;
        }

        if (!m_rules.IsFinished()) {
            UpdatePlayers(delta);
            m_rules.Tick(delta);
            if (!m_rules.IsFinished()) {
                UpdateItemRush(delta);
            }
            ApplyPaintEvents();
            UpdateTileVisuals(delta);
            UpdateItemVisuals();
            UpdatePlayerVisuals();
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
                    SubmitPresentation(RuntimePresentationCommandType::Success, {}, 1.25f);
                } else {
                    SubmitPresentation(RuntimePresentationCommandType::Failure);
                }
            }
        } else {
            UpdateTileVisuals(delta);
            UpdateItemVisuals();
            UpdatePlayerVisuals();
            UpdateResultInput();
        }
    }

    void OnFixedUpdate(float dt) override { (void)dt; }

    void OnDraw() override {
        DrawScreenHeader(
            "COLOR TERRITORY",
            "床を奪え。後半20秒はBOMBとSTARが降ってくる！",
            "WASD / 矢印：移動   BOMBは先に触れて自分色へ   STARは加速・無敵",
            m_rules.GetRemainingSeconds()
        );
        if (const auto* board = m_rules.TryGetBoard()) {
            DrawScoreRow(board->GetScores());
        }
        DrawItemHud();
        if (m_result) {
            DrawResultPanel(*m_result);
        }
    }

    void OnEditorUpdate(float dt) override { (void)dt; }

    void OnStop() override {
        ShutdownRules();
        MiniGameRuntimeMailbox::UnregisterRulesShutdown(m_sceneToken);
        MiniGameRuntimeMailbox::ClearForScene(m_sceneToken);
    }

    void QueueBoardVisuals() {
        m_tiles.assign(static_cast<std::size_t>(BoardWidth * BoardHeight), {});
        for (int y = 0; y < BoardHeight; ++y) {
            for (int x = 0; x < BoardWidth; ++x) {
                const std::size_t index = static_cast<std::size_t>(y * BoardWidth + x);
                QueueCube(
                    "TerritoryTile_" + std::to_string(index),
                    TileWorldPosition({x, y}),
                    Vector3(1.0f, 0.12f, 1.0f),
                    NeutralTileColor(),
                    [this, index](const CubeVisualRefs& refs) {
                        if (index < m_tiles.size()) {
                            m_tiles[index].material = refs.material;
                            m_tiles[index].baseColor = NeutralTileColor();
                        }
                    }
                );
            }
        }
    }

    void QueuePlayerVisuals() {
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            QueueCube(
                "TerritoryPlayer_" + std::to_string(index + 1),
                ToWorld(m_players[index].state.position, 0.68f),
                Vector3(0.72f, 1.1f, 0.72f),
                PlayerColor(static_cast<PlayerId>(index)),
                [this, index](const CubeVisualRefs& refs) {
                    m_players[index].transform = refs.transform;
                    m_players[index].material = refs.material;
                }
            );
        }
    }

    void QueueBoundaryVisuals() {
        const float width = BoardWidth * TileSpacing;
        const float height = BoardHeight * TileSpacing;
        const DirectX::XMFLOAT4 color(0.08f, 0.1f, 0.14f, 1.0f);
        QueueCube("TerritoryWallNorth", {0.0f, 0.65f, height * 0.5f + 0.35f},
            {width + 0.8f, 1.2f, 0.35f}, color);
        QueueCube("TerritoryWallSouth", {0.0f, 0.65f, -height * 0.5f - 0.35f},
            {width + 0.8f, 1.2f, 0.35f}, color);
        QueueCube("TerritoryWallEast", {width * 0.5f + 0.35f, 0.65f, 0.0f},
            {0.35f, 1.2f, height}, color);
        QueueCube("TerritoryWallWest", {-width * 0.5f - 0.35f, 0.65f, 0.0f},
            {0.35f, 1.2f, height}, color);
    }

    void QueueItemVisuals() {
        for (std::size_t index = 0; index < ItemSlotCount; ++index) {
            QueueCube(
                "TerritoryItemCore_" + std::to_string(index),
                HiddenPosition(), {}, {1.0f, 0.35f, 0.08f, 1.0f},
                [this, index](const CubeVisualRefs& refs) {
                    m_items[index].visual.coreTransform = refs.transform;
                    m_items[index].visual.coreMaterial = refs.material;
                    ConfigureGlow(refs.material, {1.0f, 0.35f, 0.08f, 1.0f}, 4.0f);
                }
            );
            QueueCube(
                "TerritoryItemHalo_" + std::to_string(index),
                HiddenPosition(), {}, {1.0f, 0.55f, 0.08f, 1.0f},
                [this, index](const CubeVisualRefs& refs) {
                    m_items[index].visual.haloTransform = refs.transform;
                    m_items[index].visual.haloMaterial = refs.material;
                    ConfigureGlow(refs.material, {1.0f, 0.55f, 0.08f, 1.0f}, 3.0f);
                }
            );
            QueueCube(
                "TerritoryItemBeam_" + std::to_string(index),
                HiddenPosition(), {}, {0.25f, 0.8f, 1.0f, 1.0f},
                [this, index](const CubeVisualRefs& refs) {
                    m_items[index].visual.beamTransform = refs.transform;
                    m_items[index].visual.beamMaterial = refs.material;
                    ConfigureGlow(refs.material, {0.25f, 0.8f, 1.0f, 1.0f}, 5.0f);
                }
            );
        }
    }

    void QueueStarAuraVisuals() {
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            QueueCube(
                "TerritoryStarAura_" + std::to_string(index),
                HiddenPosition(), {}, {1.0f, 0.88f, 0.18f, 1.0f},
                [this, index](const CubeVisualRefs& refs) {
                    m_players[index].starAuraTransform = refs.transform;
                    m_players[index].starAuraMaterial = refs.material;
                    ConfigureGlow(refs.material, {1.0f, 0.88f, 0.18f, 1.0f}, 5.0f);
                }
            );
        }
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

                if (m_players[playerIndex].power.HasStar()) {
                    if (const auto target = FindNearestOpponent(playerIndex)) {
                        inputs[playerIndex].move = NormalizeOrZero(
                            m_players[*target].state.position -
                            m_players[playerIndex].state.position
                        );
                    }
                } else if (const auto escape = ComputeBombEscapeDirection(playerIndex)) {
                    inputs[playerIndex].move = *escape;
                    m_players[playerIndex].cpuTarget.reset();
                    clock.ClearTarget();
                } else {
                    if (clock.Tick(deltaTime) && clock.CanChangeTarget()) {
                        if (const auto itemTarget = ChooseCpuItemTarget(playerIndex)) {
                            m_players[playerIndex].cpuTarget = *itemTarget;
                        } else {
                            ColorTerritory::CpuTargetContext context;
                            context.self = static_cast<PlayerId>(playerIndex);
                            context.currentTile = WorldToTile(m_players[playerIndex].state.position);
                            context.remainingTimeRatio = m_rules.GetRemainingTimeRatio();
                            context.crowdByTile = crowd;
                            const auto decision = ColorTerritory::TerritoryCpuEvaluator::ChooseTarget(
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
                                            decision->target.x +
                                                (playerIndex % 2 == 0 ? 1 : -1),
                                            0,
                                            BoardWidth - 1
                                        ),
                                        decision->target.y
                                    };
                                }
                            }
                        }
                        clock.CommitTarget();
                    }

                    if (m_players[playerIndex].cpuTarget) {
                        const Vec2 target = TileWorldPosition2D(*m_players[playerIndex].cpuTarget);
                        const Vec2 toTarget = target - m_players[playerIndex].state.position;
                        inputs[playerIndex].move = NormalizeOrZero(toTarget);
                        if (LengthSquared(toTarget) < 0.18f) {
                            m_players[playerIndex].cpuTarget.reset();
                            clock.ClearTarget();
                        }
                    }
                }
            }
        }

        const MovementBounds bounds{
            {-BoardWidth * TileSpacing * 0.5f, -BoardHeight * TileSpacing * 0.5f},
            { BoardWidth * TileSpacing * 0.5f,  BoardHeight * TileSpacing * 0.5f}
        };
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            PlayerRuntime& player = m_players[index];
            player.state.inputEnabled = !player.power.IsStunned();
            if (player.power.IsStunned()) {
                inputs[index].move = {};
                player.state.velocity = {};
            }

            MiniGamePlayerConfig config = m_playerConfig;
            config.maximumSpeed *= player.power.ResolveSpeedMultiplier(m_itemConfig);
            if (player.power.HasStar()) {
                config.acceleration *= 1.35f;
                config.turnResponsiveness *= 1.25f;
            }
            MiniGamePlayerModel::Tick(player.state, inputs[index], config, bounds, deltaTime);
        }

        ApplyStarContactEffects();
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
    }

    void ApplyStarContactEffects() {
        for (std::size_t sourceIndex = 0; sourceIndex < PlayerCount; ++sourceIndex) {
            PlayerRuntime& source = m_players[sourceIndex];
            if (!source.power.HasStar()) {
                continue;
            }
            for (std::size_t targetIndex = 0; targetIndex < PlayerCount; ++targetIndex) {
                if (sourceIndex == targetIndex ||
                    Distance(source.state.position, m_players[targetIndex].state.position) >
                        StarTouchRadius) {
                    continue;
                }
                if (!source.power.TryConsumeStarTouch(
                        static_cast<PlayerId>(targetIndex),
                        m_itemConfig.starTouchCooldownSeconds)) {
                    continue;
                }
                PlayerRuntime& target = m_players[targetIndex];
                if (!target.power.ApplyStun(m_itemConfig.starTouchStunSeconds)) {
                    continue;
                }
                target.state.velocity = {};
                MiniGamePlayerModel::ApplyKnockback(
                    target.state,
                    target.state.position - source.state.position,
                    2.4f,
                    4.0f
                );
                SubmitPresentation(RuntimePresentationCommandType::Hit,
                    target.state.position, 1.05f);
                SetBanner(
                    "P" + std::to_string(sourceIndex + 1) + " STAR HIT  P" +
                        std::to_string(targetIndex + 1) + " STUN!",
                    D2D1::ColorF(1.0f, 0.88f, 0.2f, 1.0f),
                    0.8f
                );
            }
        }
    }

    void UpdateItemRush(float deltaTime) {
        const float remaining = m_rules.GetRemainingSeconds();
        if (!m_itemRushStarted &&
            remaining <= m_itemConfig.rushStartRemainingSeconds) {
            m_itemRushStarted = true;
            m_nextItemSpawnSeconds = 0.35f;
            SetBanner("ITEM RUSH!  BOMB / STAR INCOMING",
                D2D1::ColorF(1.0f, 0.55f, 0.12f, 1.0f), 1.55f);
            SubmitPresentation(RuntimePresentationCommandType::Hit, {}, 1.25f);
        }
        if (!m_itemRushStarted) {
            return;
        }

        m_nextItemSpawnSeconds -= deltaTime;
        if (m_nextItemSpawnSeconds <= 0.0f) {
            if (SpawnItem()) {
                m_nextItemSpawnSeconds = m_itemRandom.NextRange(
                    m_itemConfig.minimumSpawnIntervalSeconds,
                    m_itemConfig.maximumSpawnIntervalSeconds
                );
            } else {
                m_nextItemSpawnSeconds = 0.75f;
            }
        }

        for (ItemRuntime& item : m_items) {
            if (item.phase == ColorTerritory::TerritoryItemPhase::Inactive) {
                continue;
            }
            item.ageSeconds += deltaTime;
            item.remainingSeconds = std::max(0.0f, item.remainingSeconds - deltaTime);
            if (item.phase == ColorTerritory::TerritoryItemPhase::Falling) {
                if (item.remainingSeconds <= 0.0f) {
                    LandItem(item);
                }
                continue;
            }

            if (item.type == ColorTerritory::TerritoryItemType::Bomb) {
                TryClaimBomb(item);
                if (item.remainingSeconds <= 0.0f) {
                    ExplodeBomb(item);
                }
            } else if (!TryCollectStar(item) && item.remainingSeconds <= 0.0f) {
                const Vec2 position = TileWorldPosition2D(item.tile);
                SetBanner("STAR VANISHED",
                    D2D1::ColorF(0.55f, 0.72f, 0.95f, 1.0f), 0.7f);
                SubmitPresentation(RuntimePresentationCommandType::NearMiss,
                    position, 0.65f);
                DeactivateItem(item);
            }
        }
    }

    bool SpawnItem() {
        ItemRuntime* item = nullptr;
        for (ItemRuntime& candidate : m_items) {
            if (candidate.phase == ColorTerritory::TerritoryItemPhase::Inactive) {
                item = &candidate;
                break;
            }
        }
        if (!item) {
            return false;
        }

        ColorTerritory::TileCoord tile = m_itemRandom.NextTile(BoardWidth, BoardHeight);
        for (int attempt = 0; attempt < 10 && !IsSpawnTileSafe(tile); ++attempt) {
            tile = m_itemRandom.NextTile(BoardWidth, BoardHeight);
        }
        item->type = m_itemRandom.NextType();
        item->phase = ColorTerritory::TerritoryItemPhase::Falling;
        item->tile = tile;
        item->owner.reset();
        item->remainingSeconds = m_itemConfig.fallingSeconds;
        item->totalSeconds = m_itemConfig.fallingSeconds;
        item->ageSeconds = 0.0f;
        item->serial = m_nextItemSerial++;

        const Vec2 position = TileWorldPosition2D(tile);
        SetBanner(
            item->type == ColorTerritory::TerritoryItemType::Bomb
                ? "WARNING: BOMB DROPPING"
                : "STAR DROPPING - CLAIM IT!",
            item->type == ColorTerritory::TerritoryItemType::Bomb
                ? D2D1::ColorF(1.0f, 0.28f, 0.08f, 1.0f)
                : D2D1::ColorF(1.0f, 0.9f, 0.2f, 1.0f),
            0.9f
        );
        SubmitPresentation(RuntimePresentationCommandType::NearMiss, position, 0.75f);
        SubmitPresentation(RuntimePresentationCommandType::Score, position, 0.42f);
        FlashArea(tile, 0.8f, 0.55f);
        return true;
    }

    void LandItem(ItemRuntime& item) {
        item.phase = ColorTerritory::TerritoryItemPhase::Active;
        item.ageSeconds = 0.0f;
        item.remainingSeconds = item.type == ColorTerritory::TerritoryItemType::Bomb
            ? m_itemConfig.bombFuseSeconds
            : m_itemConfig.starGroundLifetimeSeconds;
        item.totalSeconds = item.remainingSeconds;
        const Vec2 position = TileWorldPosition2D(item.tile);
        SetBanner(
            item.type == ColorTerritory::TerritoryItemType::Bomb
                ? "BOMB LANDED - TOUCH TO CLAIM, THEN ESCAPE!"
                : "STAR LANDED - SPEED + INVINCIBLE",
            item.type == ColorTerritory::TerritoryItemType::Bomb
                ? D2D1::ColorF(1.0f, 0.42f, 0.08f, 1.0f)
                : D2D1::ColorF(1.0f, 0.9f, 0.22f, 1.0f),
            1.1f
        );
        SubmitPresentation(RuntimePresentationCommandType::Hit, position, 0.7f);
        FlashArea(item.tile, 0.9f, 0.4f);
    }

    void TryClaimBomb(ItemRuntime& item) {
        if (item.owner) {
            return;
        }
        const Vec2 position = TileWorldPosition2D(item.tile);
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            if (Distance(m_players[index].state.position, position) > ItemPickupRadius) {
                continue;
            }
            item.owner = static_cast<PlayerId>(index);
            item.remainingSeconds = std::max(
                item.remainingSeconds,
                m_itemConfig.bombClaimGraceSeconds
            );
            item.totalSeconds = std::max(item.totalSeconds, item.remainingSeconds);
            SetBanner(
                "P" + std::to_string(index + 1) + " CLAIMED BOMB - AREA WILL TURN " +
                    PlayerColorName(index),
                ToD2DColor(PlayerColor(static_cast<PlayerId>(index))),
                1.05f
            );
            SubmitPresentation(RuntimePresentationCommandType::Success, position, 1.15f);
            FlashArea(item.tile, 1.5f, 0.6f);
            return;
        }
    }

    bool TryCollectStar(ItemRuntime& item) {
        const Vec2 position = TileWorldPosition2D(item.tile);
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            if (Distance(m_players[index].state.position, position) > ItemPickupRadius) {
                continue;
            }
            PlayerRuntime& player = m_players[index];
            player.power.ActivateStar(m_itemConfig.starBuffSeconds);
            player.state.inputEnabled = true;
            SetBanner(
                "P" + std::to_string(index + 1) +
                    " STAR POWER!  SPEED UP / INVINCIBLE / TOUCH STUN",
                D2D1::ColorF(1.0f, 0.9f, 0.2f, 1.0f),
                1.35f
            );
            SubmitPresentation(RuntimePresentationCommandType::Success,
                player.state.position, 1.9f);
            SubmitPresentation(RuntimePresentationCommandType::Score,
                player.state.position, 1.15f);
            FlashArea(item.tile, 1.8f, 0.62f);
            DeactivateItem(item);
            return true;
        }
        return false;
    }

    void ExplodeBomb(ItemRuntime& item) {
        const Vec2 center = TileWorldPosition2D(item.tile);
        const std::optional<PlayerId> owner = item.owner;
        const std::size_t changedTiles = m_rules.ApplyBombArea(
            item.tile,
            m_itemConfig.bombTileRadius,
            owner
        );
        FlashArea(item.tile, 2.8f, 0.78f);

        int stunnedPlayers = 0;
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            PlayerRuntime& player = m_players[index];
            const Vec2 fromCenter = player.state.position - center;
            if (Length(fromCenter) > m_itemConfig.bombStunRadius) {
                continue;
            }
            if (!player.power.ApplyStun(m_itemConfig.bombStunSeconds)) {
                if (player.power.HasStar()) {
                    SubmitPresentation(RuntimePresentationCommandType::Score,
                        player.state.position, 0.85f);
                }
                continue;
            }
            ++stunnedPlayers;
            player.state.velocity = {};
            MiniGamePlayerModel::ApplyKnockback(
                player.state,
                LengthSquared(fromCenter) > 0.0001f
                    ? fromCenter
                    : Vec2{index % 2 == 0 ? 1.0f : -1.0f, 0.5f},
                3.2f,
                5.0f
            );
            SubmitPresentation(RuntimePresentationCommandType::Hit,
                player.state.position, 1.0f);
        }

        SetBanner(
            owner
                ? "BOMB PAINT!  P" + std::to_string(*owner + 1) + " +" +
                    std::to_string(changedTiles) + " TILES"
                : "BOMB BLAST!  " + std::to_string(changedTiles) +
                    " TILES ERASED / " + std::to_string(stunnedPlayers) + " STUNNED",
            owner
                ? ToD2DColor(PlayerColor(*owner))
                : D2D1::ColorF(1.0f, 0.22f, 0.06f, 1.0f),
            1.45f
        );
        SubmitPresentation(
            owner
                ? RuntimePresentationCommandType::Success
                : RuntimePresentationCommandType::Hit,
            center,
            owner ? 2.25f : 2.5f
        );
        DeactivateItem(item);
    }

    void DeactivateItem(ItemRuntime& item) {
        item.phase = ColorTerritory::TerritoryItemPhase::Inactive;
        item.owner.reset();
        item.remainingSeconds = 0.0f;
        item.totalSeconds = 0.0f;
        item.ageSeconds = 0.0f;
        HideItemVisual(item.visual);
    }

    std::optional<ColorTerritory::TileCoord> ChooseCpuItemTarget(
        std::size_t playerIndex
    ) const {
        const ColorTerritory::TerritoryBoard* board = m_rules.TryGetBoard();
        const int selfScore = board
            ? board->GetScore(static_cast<PlayerId>(playerIndex))
            : 0;
        const int leaderScore = board && !board->GetScores().empty()
            ? *std::max_element(board->GetScores().begin(), board->GetScores().end())
            : selfScore;
        const float trailingBonus = static_cast<float>(leaderScore - selfScore) * 0.35f;

        const ItemRuntime* best = nullptr;
        float bestUtility = -1000.0f;
        for (const ItemRuntime& item : m_items) {
            if (item.phase == ColorTerritory::TerritoryItemPhase::Inactive ||
                (item.type == ColorTerritory::TerritoryItemType::Bomb && item.owner) ||
                (item.type == ColorTerritory::TerritoryItemType::Bomb &&
                 item.phase == ColorTerritory::TerritoryItemPhase::Active &&
                 item.remainingSeconds < 1.15f)) {
                continue;
            }
            const float distance = Distance(
                m_players[playerIndex].state.position,
                TileWorldPosition2D(item.tile)
            );
            float utility = item.type == ColorTerritory::TerritoryItemType::Star
                ? 11.0f + trailingBonus
                : 7.0f + trailingBonus * 0.5f;
            utility -= distance * 0.72f;
            utility -= item.phase == ColorTerritory::TerritoryItemPhase::Falling ? 0.8f : 0.0f;
            if (utility > bestUtility) {
                bestUtility = utility;
                best = &item;
            }
        }
        return best
            ? std::optional<ColorTerritory::TileCoord>(best->tile)
            : std::nullopt;
    }

    std::optional<Vec2> ComputeBombEscapeDirection(std::size_t playerIndex) const {
        if (m_players[playerIndex].power.HasStar()) {
            return std::nullopt;
        }
        Vec2 escape{};
        for (const ItemRuntime& item : m_items) {
            if (item.type != ColorTerritory::TerritoryItemType::Bomb ||
                item.phase != ColorTerritory::TerritoryItemPhase::Active ||
                item.remainingSeconds > 1.35f) {
                continue;
            }
            const Vec2 away = m_players[playerIndex].state.position -
                TileWorldPosition2D(item.tile);
            const float distance = Length(away);
            const float radius = m_itemConfig.bombStunRadius + 1.1f;
            if (distance < radius) {
                escape += NormalizeOrZero(away) * ((radius - distance) / radius);
            }
        }
        return LengthSquared(escape) > 0.0001f
            ? std::optional<Vec2>(NormalizeOrZero(escape))
            : std::nullopt;
    }

    std::optional<std::size_t> FindNearestOpponent(std::size_t playerIndex) const {
        std::optional<std::size_t> result;
        float bestDistance = 100000.0f;
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            if (index == playerIndex) {
                continue;
            }
            const float distance = Distance(
                m_players[playerIndex].state.position,
                m_players[index].state.position
            );
            if (distance < bestDistance) {
                bestDistance = distance;
                result = index;
            }
        }
        return result;
    }

    bool IsSpawnTileSafe(ColorTerritory::TileCoord tile) const {
        const Vec2 position = TileWorldPosition2D(tile);
        for (const PlayerRuntime& player : m_players) {
            if (Distance(player.state.position, position) < 1.5f) {
                return false;
            }
        }
        for (const ItemRuntime& item : m_items) {
            if (item.phase != ColorTerritory::TerritoryItemPhase::Inactive &&
                Distance(TileWorldPosition2D(item.tile), position) < 1.8f) {
                return false;
            }
        }
        return true;
    }

    void UpdatePlayerVisuals() {
        for (std::size_t index = 0; index < PlayerCount; ++index) {
            PlayerRuntime& player = m_players[index];
            const bool star = player.power.HasStar();
            const bool stunned = player.power.IsStunned();
            const float starPulse = 0.5f + 0.5f *
                std::sin(m_visualTimeSeconds * 13.0f + static_cast<float>(index));
            const float stunWobble = std::sin(
                m_visualTimeSeconds * 26.0f + static_cast<float>(index) * 1.7f
            );

            if (TransformComponent* transform = player.transform.TryGet()) {
                transform->position = ToWorld(
                    player.state.position,
                    star ? 0.76f + starPulse * 0.08f : 0.68f
                );
                const float yaw = std::atan2(player.state.forward.x, player.state.forward.y);
                transform->SetRotationEuler({
                    stunned ? stunWobble * 0.16f : 0.0f,
                    yaw + (star ? m_visualTimeSeconds * 0.9f : 0.0f),
                    stunned ? stunWobble * 0.24f : 0.0f
                });
                if (star) {
                    const float scale = 1.0f + starPulse * 0.12f;
                    transform->scale = {0.72f * scale, 1.1f * scale, 0.72f * scale};
                } else if (stunned) {
                    transform->scale = {
                        0.82f + std::abs(stunWobble) * 0.08f,
                        0.78f,
                        0.82f + std::abs(stunWobble) * 0.08f
                    };
                } else {
                    transform->scale = {0.72f, 1.1f, 0.72f};
                }
            }

            if (MaterialComponent* material = player.material.TryGet()) {
                const DirectX::XMFLOAT4 color = PlayerColor(static_cast<PlayerId>(index));
                material->Material.BaseColor = stunned
                    ? DirectX::XMFLOAT4(color.x * 0.42f, color.y * 0.42f,
                        color.z * 0.5f, 1.0f)
                    : color;
                material->Material.EmissiveColor = star
                    ? DirectX::XMFLOAT3(1.0f, 0.72f + starPulse * 0.25f, 0.08f)
                    : DirectX::XMFLOAT3(color.x * 0.58f, color.y * 0.58f,
                        color.z * 0.58f);
                material->Material.EmissiveIntensity = star
                    ? 3.4f + starPulse * 2.2f
                    : stunned ? 0.08f : 0.58f;
            }

            if (TransformComponent* aura = player.starAuraTransform.TryGet()) {
                if (!star) {
                    aura->position = HiddenPosition();
                    aura->scale = {};
                } else {
                    const float angle = m_visualTimeSeconds * 5.5f +
                        static_cast<float>(index) * 1.4f;
                    aura->position = {
                        player.state.position.x + std::cos(angle) * 0.98f,
                        1.0f + std::sin(angle * 1.7f) * 0.22f,
                        player.state.position.y + std::sin(angle) * 0.98f
                    };
                    aura->scale = {0.22f, 0.22f, 0.22f};
                    aura->SetRotationEuler({angle, angle * 1.7f, angle * 0.6f});
                }
            }
            if (MaterialComponent* auraMaterial = player.starAuraMaterial.TryGet()) {
                auraMaterial->Material.EmissiveIntensity = 4.0f + starPulse * 2.5f;
            }
        }
    }

    void UpdateItemVisuals() {
        for (ItemRuntime& item : m_items) {
            if (item.phase == ColorTerritory::TerritoryItemPhase::Inactive) {
                HideItemVisual(item.visual);
                continue;
            }

            const Vec2 position = TileWorldPosition2D(item.tile);
            const bool bomb = item.type == ColorTerritory::TerritoryItemType::Bomb;
            const float ratio = item.totalSeconds > 0.0f
                ? std::clamp(item.remainingSeconds / item.totalSeconds, 0.0f, 1.0f)
                : 0.0f;
            const float urgency = 1.0f - ratio;
            const float pulse = 0.5f + 0.5f * std::sin(
                item.ageSeconds * (bomb ? 6.0f + urgency * 18.0f : 10.0f) +
                static_cast<float>(item.serial)
            );

            float y = 0.86f + std::sin(item.ageSeconds * 4.0f) * 0.1f;
            if (item.phase == ColorTerritory::TerritoryItemPhase::Falling) {
                const float progress = item.totalSeconds > 0.0f
                    ? 1.0f - item.remainingSeconds / item.totalSeconds
                    : 1.0f;
                const float eased = 1.0f - std::pow(std::max(0.0f, 1.0f - progress), 3.0f);
                y = std::lerp(ItemFallHeight, 0.86f, eased);
            }

            DirectX::XMFLOAT4 color = bomb
                ? DirectX::XMFLOAT4(1.0f, 0.2f + pulse * 0.22f, 0.04f, 1.0f)
                : DirectX::XMFLOAT4(1.0f, 0.78f + pulse * 0.2f, 0.08f, 1.0f);
            if (bomb && item.owner) {
                color = PlayerColor(*item.owner);
            }
            SetGlow(item.visual.coreMaterial, color, 3.8f + pulse * 2.5f);
            SetGlow(item.visual.haloMaterial, color, 2.5f + pulse * 2.0f);
            SetGlow(item.visual.beamMaterial, color, 4.5f);

            if (TransformComponent* core = item.visual.coreTransform.TryGet()) {
                core->position = {position.x, y, position.y};
                const float scale = (bomb ? 0.62f : 0.5f) * (1.0f + pulse * 0.17f);
                core->scale = bomb
                    ? Vector3(scale, scale, scale)
                    : Vector3(scale * 1.3f, scale * 0.62f, scale * 0.28f);
                core->SetRotationEuler({
                    item.ageSeconds * 1.4f,
                    item.ageSeconds * (bomb ? 2.2f : 5.0f) + (bomb ? 0.0f : 0.78f),
                    item.ageSeconds * 1.1f
                });
            }
            if (TransformComponent* halo = item.visual.haloTransform.TryGet()) {
                halo->position = {position.x, 0.14f, position.y};
                const float scale = (bomb ? 1.35f : 1.05f) +
                    pulse * (bomb ? 0.65f : 0.38f);
                halo->scale = {scale, 0.045f, scale};
                halo->SetRotationEuler({0.0f, item.ageSeconds * 2.6f, 0.0f});
            }
            if (TransformComponent* beam = item.visual.beamTransform.TryGet()) {
                if (item.phase == ColorTerritory::TerritoryItemPhase::Falling) {
                    const float height = std::max(0.2f, y - 0.2f);
                    beam->position = {position.x, 0.15f + height * 0.5f, position.y};
                    beam->scale = {0.1f + pulse * 0.05f, height,
                        0.1f + pulse * 0.05f};
                } else if (!bomb) {
                    beam->position = {position.x, 1.55f, position.y};
                    beam->scale = {0.06f, 1.5f + pulse * 0.5f, 0.06f};
                } else {
                    beam->position = HiddenPosition();
                    beam->scale = {};
                }
            }
        }
    }

    void ApplyPaintEvents() {
        for (const ColorTerritory::TerritoryPaintEvent& event :
            m_rules.ConsumePaintEvents()) {
            const std::size_t index = static_cast<std::size_t>(
                event.tile.y * BoardWidth + event.tile.x
            );
            if (index >= m_tiles.size()) {
                continue;
            }
            TileVisual& tile = m_tiles[index];
            tile.baseColor = event.newOwner == ColorTerritory::UnclaimedOwner
                ? NeutralTileColor()
                : PlayerColor(static_cast<PlayerId>(event.newOwner));
            tile.flashDurationSeconds =
                event.source == ColorTerritory::TerritoryPaintSource::Movement
                    ? 0.24f : 0.62f;
            tile.flashRemainingSeconds = tile.flashDurationSeconds;
            tile.flashIntensity = event.source == ColorTerritory::TerritoryPaintSource::Movement
                ? event.changedLeader ? 1.6f : 0.65f
                : event.source == ColorTerritory::TerritoryPaintSource::BombPaint
                    ? 2.4f : 2.0f;

            if (event.source == ColorTerritory::TerritoryPaintSource::Movement) {
                SubmitPresentation(RuntimePresentationCommandType::Score,
                    TileWorldPosition2D(event.tile),
                    event.changedLeader ? 1.8f : 0.65f);
            }
        }
    }

    void UpdateTileVisuals(float deltaTime) {
        for (TileVisual& tile : m_tiles) {
            tile.flashRemainingSeconds = std::max(
                0.0f,
                tile.flashRemainingSeconds - deltaTime
            );
            const float envelope = tile.flashDurationSeconds > 0.0f
                ? tile.flashRemainingSeconds / tile.flashDurationSeconds
                : 0.0f;
            if (MaterialComponent* material = tile.material.TryGet()) {
                const float boost = envelope * tile.flashIntensity;
                material->Material.BaseColor = {
                    std::min(1.0f, tile.baseColor.x + boost * 0.32f),
                    std::min(1.0f, tile.baseColor.y + boost * 0.28f),
                    std::min(1.0f, tile.baseColor.z + boost * 0.2f),
                    1.0f
                };
                material->Material.EmissiveColor = {
                    tile.baseColor.x,
                    tile.baseColor.y,
                    tile.baseColor.z
                };
                material->Material.EmissiveIntensity =
                    (IsNeutralColor(tile.baseColor) ? 0.02f : 0.12f) + boost * 2.1f;
            }
        }
    }

    void FlashArea(
        ColorTerritory::TileCoord center,
        float intensity,
        float durationSeconds
    ) {
        for (int y = center.y - m_itemConfig.bombTileRadius;
             y <= center.y + m_itemConfig.bombTileRadius; ++y) {
            for (int x = center.x - m_itemConfig.bombTileRadius;
                 x <= center.x + m_itemConfig.bombTileRadius; ++x) {
                if (x < 0 || y < 0 || x >= BoardWidth || y >= BoardHeight) {
                    continue;
                }
                TileVisual& tile = m_tiles[static_cast<std::size_t>(y * BoardWidth + x)];
                tile.flashDurationSeconds = std::max(tile.flashDurationSeconds,
                    durationSeconds);
                tile.flashRemainingSeconds = std::max(tile.flashRemainingSeconds,
                    durationSeconds);
                tile.flashIntensity = std::max(tile.flashIntensity, intensity);
            }
        }
    }

    void DrawItemHud() const {
        MiniGameRuntimeUi ui(GetEntityRef().GetScene());
        if (!ui.IsAvailable() || m_result) {
            return;
        }

        const float width = std::min(820.0f, std::max(520.0f, ui.Width() - 40.0f));
        const float x = (ui.Width() - width) * 0.5f;
        const float y = 182.0f;
        ui.FillPanel(x, y, width, 48.0f,
            D2D1::ColorF(0.018f, 0.028f, 0.052f, 0.88f));

        std::ostringstream status;
        if (!m_itemRushStarted) {
            status << "ITEM RUSH IN " << std::fixed << std::setprecision(1)
                   << std::max(0.0f, m_rules.GetRemainingSeconds() -
                        m_itemConfig.rushStartRemainingSeconds);
        } else {
            status << "ITEM RUSH  ";
            bool hasItem = false;
            for (const ItemRuntime& item : m_items) {
                if (item.phase == ColorTerritory::TerritoryItemPhase::Inactive) {
                    continue;
                }
                if (hasItem) {
                    status << "   |   ";
                }
                hasItem = true;
                status << (item.type == ColorTerritory::TerritoryItemType::Bomb
                    ? "BOMB " : "STAR ");
                if (item.phase == ColorTerritory::TerritoryItemPhase::Falling) {
                    status << "DROPPING";
                } else {
                    status << std::fixed << std::setprecision(1)
                           << item.remainingSeconds << "s";
                    if (item.type == ColorTerritory::TerritoryItemType::Bomb) {
                        status << (item.owner
                            ? " [P" + std::to_string(*item.owner + 1) + " PAINT]"
                            : " [ERASE]");
                    }
                }
            }
            if (!hasItem) {
                status << "NEXT DROP " << std::fixed << std::setprecision(1)
                       << std::max(0.0f, m_nextItemSpawnSeconds) << "s";
            }
        }
        ui.DrawText(status.str(), x + 16.0f, y + 15.0f, 15.0f,
            D2D1::ColorF(0.9f, 0.94f, 1.0f, 1.0f), false);

        std::ostringstream self;
        if (m_players[0].power.HasStar()) {
            self << "YOU: STAR " << std::fixed << std::setprecision(1)
                 << m_players[0].power.starRemainingSeconds << "s";
        } else if (m_players[0].power.IsStunned()) {
            self << "YOU: STUN " << std::fixed << std::setprecision(1)
                 << m_players[0].power.stunRemainingSeconds << "s";
        }
        if (!self.str().empty()) {
            ui.DrawText(self.str(), x + width - 150.0f, y + 15.0f, 15.0f,
                m_players[0].power.HasStar()
                    ? D2D1::ColorF(1.0f, 0.9f, 0.2f, 1.0f)
                    : D2D1::ColorF(1.0f, 0.36f, 0.2f, 1.0f), false);
        }

        if (m_eventBanner.remainingSeconds > 0.0f && !m_eventBanner.text.empty()) {
            const float bannerWidth = std::min(760.0f, ui.Width() - 60.0f);
            const float bannerX = (ui.Width() - bannerWidth) * 0.5f;
            const float bannerY = 238.0f;
            ui.FillPanel(
                bannerX,
                bannerY,
                bannerWidth,
                52.0f,
                D2D1::ColorF(
                    m_eventBanner.color.r * 0.12f,
                    m_eventBanner.color.g * 0.12f,
                    m_eventBanner.color.b * 0.12f,
                    0.9f
                )
            );
            ui.DrawTextCentered(m_eventBanner.text, ui.Width() * 0.5f,
                bannerY + 13.0f, 21.0f, m_eventBanner.color);
        }
    }

    void SetBanner(std::string text, D2D1::ColorF color, float durationSeconds) {
        m_eventBanner.text = std::move(text);
        m_eventBanner.color = color;
        m_eventBanner.remainingSeconds = std::max(0.0f, durationSeconds);
        m_eventBanner.durationSeconds = m_eventBanner.remainingSeconds;
    }

    void UpdateWarnings() {
        const float remaining = m_rules.GetRemainingSeconds();
        if (!m_warning10Played && remaining <= 10.0f) {
            m_warning10Played = true;
            SetBanner("10 SECONDS - BOMB CHAOS INTENSIFIES",
                D2D1::ColorF(1.0f, 0.52f, 0.12f, 1.0f), 0.9f);
            SubmitPresentation(RuntimePresentationCommandType::NearMiss, {}, 0.7f);
        }
        if (!m_warning5Played && remaining <= 5.0f) {
            m_warning5Played = true;
            SetBanner("FINAL 5 - TAKE THE LEAD!",
                D2D1::ColorF(1.0f, 0.25f, 0.08f, 1.0f), 0.9f);
            SubmitPresentation(RuntimePresentationCommandType::Hit, {}, 0.8f);
        }
    }

    void UpdateResultInput() {
        if (GetKeyDown('R')) {
            m_transitionSubmitted = SubmitTransition(ScenePath, TransitionRequest::Retry);
        } else if (GetKeyDown(VK_ESCAPE)) {
            m_transitionSubmitted = SubmitTransition({}, TransitionRequest::Selection);
        } else if (GetKeyDown('N')) {
            m_transitionSubmitted = SubmitTransition(NextScenePath,
                TransitionRequest::NextGame);
        }
    }

    std::vector<std::uint8_t> BuildCrowdMap() const {
        std::vector<std::uint8_t> crowd(
            static_cast<std::size_t>(BoardWidth * BoardHeight), 0);
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

    static void ConfigureGlow(
        ComponentRef<MaterialComponent> ref,
        DirectX::XMFLOAT4 color,
        float intensity
    ) {
        if (MaterialComponent* material = ref.TryGet()) {
            material->ShaderID = 1;
            material->Material.BaseColor = color;
            material->Material.Metallic = 0.22f;
            material->Material.Roughness = 0.16f;
            material->Material.EmissiveColor = {color.x, color.y, color.z};
            material->Material.EmissiveIntensity = intensity;
            material->Material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
            material->Material.MaterialFlags &= ~MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
        }
    }

    static void SetGlow(
        ComponentRef<MaterialComponent> ref,
        DirectX::XMFLOAT4 color,
        float intensity
    ) {
        if (MaterialComponent* material = ref.TryGet()) {
            material->Material.BaseColor = color;
            material->Material.EmissiveColor = {color.x, color.y, color.z};
            material->Material.EmissiveIntensity = intensity;
        }
    }

    static void HideItemVisual(ItemVisual& visual) {
        for (ComponentRef<TransformComponent> ref : {
                visual.coreTransform,
                visual.haloTransform,
                visual.beamTransform}) {
            if (TransformComponent* transform = ref.TryGet()) {
                transform->position = HiddenPosition();
                transform->scale = {};
            }
        }
    }

    static bool IsNeutralColor(const DirectX::XMFLOAT4& color) noexcept {
        const DirectX::XMFLOAT4 neutral = NeutralTileColor();
        return std::abs(color.x - neutral.x) < 0.001f &&
            std::abs(color.y - neutral.y) < 0.001f &&
            std::abs(color.z - neutral.z) < 0.001f;
    }

    static DirectX::XMFLOAT4 NeutralTileColor() noexcept {
        return {0.19f, 0.21f, 0.24f, 1.0f};
    }

    static Vector3 HiddenPosition() noexcept {
        return {0.0f, -1000.0f, 0.0f};
    }

    static D2D1::ColorF ToD2DColor(DirectX::XMFLOAT4 color) noexcept {
        return D2D1::ColorF(color.x, color.y, color.z, color.w);
    }

    static std::string PlayerColorName(std::size_t index) {
        switch (index % 4) {
        case 0: return "BLUE";
        case 1: return "RED";
        case 2: return "YELLOW";
        default: return "GREEN";
        }
    }

    static Vector3 TileWorldPosition(ColorTerritory::TileCoord tile) {
        const Vec2 position = TileWorldPosition2D(tile);
        return {position.x, 0.0f, position.y};
    }

    static Vec2 TileWorldPosition2D(ColorTerritory::TileCoord tile) {
        return {
            (static_cast<float>(tile.x) - (BoardWidth - 1) * 0.5f) * TileSpacing,
            (static_cast<float>(tile.y) - (BoardHeight - 1) * 0.5f) * TileSpacing
        };
    }

    static ColorTerritory::TileCoord WorldToTile(Vec2 position) {
        return {
            std::clamp(
                static_cast<int>(std::lround(position.x / TileSpacing +
                    (BoardWidth - 1) * 0.5f)),
                0,
                BoardWidth - 1
            ),
            std::clamp(
                static_cast<int>(std::lround(position.y / TileSpacing +
                    (BoardHeight - 1) * 0.5f)),
                0,
                BoardHeight - 1
            )
        };
    }

    static Vector3 ToWorld(Vec2 position, float y) {
        return {position.x, y, position.y};
    }

    void ShutdownRules() {
        if (m_rulesShutdown) {
            return;
        }
        m_rulesShutdown = true;
        for (PlayerRuntime& player : m_players) {
            player.state.inputEnabled = false;
            player.power.Reset();
        }
        for (ItemRuntime& item : m_items) {
            DeactivateItem(item);
        }
        m_rules.Shutdown();
        SubmitPresentation(RuntimePresentationCommandType::Cancel);
    }

    ColorTerritory::ColorTerritoryRules m_rules;
    std::array<PlayerRuntime, PlayerCount> m_players;
    std::array<ItemRuntime, ItemSlotCount> m_items;
    std::vector<MiniGameCpuDecisionClock> m_cpuClocks;
    ColorTerritory::TerritoryItemConfig m_itemConfig;
    ColorTerritory::TerritoryItemRandom m_itemRandom;
    MiniGamePlayerConfig m_playerConfig{
        .acceleration = 18.0f,
        .deceleration = 22.0f,
        .maximumSpeed = 4.2f,
        .turnResponsiveness = 10.0f,
        .knockbackDamping = 9.0f,
        .collisionRadius = 0.42f
    };
    std::vector<TileVisual> m_tiles;
    std::optional<MiniGameResult> m_result;
    EventBanner m_eventBanner;
    SceneToken m_sceneToken = 0;
    float m_countdownRemainingSeconds = 3.0f;
    float m_nextItemSpawnSeconds = 0.35f;
    float m_visualTimeSeconds = 0.0f;
    std::uint32_t m_nextItemSerial = 1;
    bool m_started = false;
    bool m_rulesShutdown = false;
    bool m_transitionSubmitted = false;
    bool m_warning10Played = false;
    bool m_warning5Played = false;
    bool m_itemRushStarted = false;
};

} // namespace MiniGameCollection::Runtime
