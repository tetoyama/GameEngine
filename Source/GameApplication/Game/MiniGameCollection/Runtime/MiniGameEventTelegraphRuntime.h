#pragma once

#include "Game/MiniGameCollection/ColorTerritory/ColorTerritoryItemModel.h"
#include "Game/MiniGameCollection/Core/WorldEventTelegraphModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"
#include "Game/MiniGameCollection/Runtime/WorldEventTelegraphPresenter.h"
#include "Game/MiniGameCollection/SheepRoundup/SheepRoundupRules.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

namespace MiniGameCollection::Runtime {

class ColorTerritoryEventTelegraphRuntime final
    : public MiniGameRuntimeScriptBase {
public:
    ColorTerritoryEventTelegraphRuntime()
        : MiniGameRuntimeScriptBase("ColorTerritoryEventTelegraphRuntime") {
        // Major開始frameから通常Score演出を抑制できるようゲーム本体より先に更新する。
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, -50);
        SetExecutionOrder(SystemTaskDomain::Render, SystemPhase::Late, 300);
    }

private:
    struct ItemForecastRecord {
        ColorTerritory::TerritoryItemType type =
            ColorTerritory::TerritoryItemType::Bomb;
        Vec2 position{};
    };

    static constexpr float CountdownSeconds = 3.0f;
    static constexpr float ItemRushWarningGameElapsedSeconds = 17.0f;
    static constexpr float ItemFallingSeconds = 2.8f;
    static constexpr float BombFuseSeconds = 3.4f;
    static constexpr float TileSpacing = 1.1f;
    static constexpr int BoardWidth = 11;
    static constexpr int BoardHeight = 7;

    void OnInitialize() override {
        ColorTerritory::TerritoryItemRandom::SetForecastObserver(
            [this](const ColorTerritory::TerritoryItemForecast& forecast) {
                SubmitItemForecast(forecast);
            }
        );
    }

    void OnStart() override {
        m_sceneToken = GetRuntimeSceneToken();
        m_elapsedSeconds = 0.0f;
        m_rushWarningScheduled = false;
        m_nextTelegraphId = 1;
        m_itemForecasts.clear();
        m_telegraphs.Clear();
        MiniGameRuntimeMailbox::SetMajorTelegraphActive(m_sceneToken, false);
    }

    void OnUpdate(float dt) override {
        const float delta = (std::max)(0.0f, dt);
        m_elapsedSeconds += delta;
        const float gameElapsed = (std::max)(
            0.0f,
            m_elapsedSeconds - CountdownSeconds
        );

        if (!m_rushWarningScheduled &&
            gameElapsed >= ItemRushWarningGameElapsedSeconds) {
            m_rushWarningScheduled = true;
            m_telegraphs.Submit({
                .id = NextId(10u),
                .sceneToken = m_sceneToken,
                .priority = TelegraphPriority::Major,
                .worldPosition = {},
                .shape = TelegraphShape::Screen,
                .radius = 0.0f,
                .warningSeconds = 3.0f,
                .armedSeconds = 0.0f,
                .resolvingSeconds = 0.16f,
                .aftermathSeconds = 0.9f,
                .label = "ITEM RUSH IN 3"
            });
        }

        const auto events = m_telegraphs.Tick(delta);
        for (const TelegraphEvent& event : events) {
            if (event.type != TelegraphEventType::Resolve) {
                continue;
            }
            const auto found = m_itemForecasts.find(event.id);
            if (found == m_itemForecasts.end()) {
                continue;
            }
            const ItemForecastRecord record = found->second;
            m_itemForecasts.erase(found);
            if (record.type == ColorTerritory::TerritoryItemType::Bomb) {
                m_telegraphs.Submit({
                    .id = NextId(30u),
                    .sceneToken = m_sceneToken,
                    .priority = TelegraphPriority::Major,
                    .worldPosition = record.position,
                    .shape = TelegraphShape::Area,
                    .radius = 2.25f,
                    .warningSeconds = BombFuseSeconds,
                    .armedSeconds = 0.0f,
                    .resolvingSeconds = 0.18f,
                    .aftermathSeconds = 1.0f,
                    .label = "BOMB 3x3 / MOVE OUT"
                });
            } else {
                m_telegraphs.Submit({
                    .id = NextId(40u),
                    .sceneToken = m_sceneToken,
                    .priority = TelegraphPriority::Minor,
                    .worldPosition = record.position,
                    .shape = TelegraphShape::Point,
                    .radius = 0.9f,
                    .warningSeconds = 5.0f,
                    .armedSeconds = 0.0f,
                    .resolvingSeconds = 0.08f,
                    .aftermathSeconds = 0.4f,
                    .label = "STAR AVAILABLE"
                });
            }
        }

        MiniGameRuntimeMailbox::SetMajorTelegraphActive(
            m_sceneToken,
            m_telegraphs.HasActiveMajor()
        );
    }

    void OnFixedUpdate(float dt) override { (void)dt; }

    void OnDraw() override {
        WorldEventTelegraphPresenter::Draw(
            GetEntityRef().GetScene(),
            m_telegraphs,
            13.0f,
            9.0f
        );
    }

    void OnEditorUpdate(float dt) override { (void)dt; }

    void OnStop() override {
        ColorTerritory::TerritoryItemRandom::ClearForecastObserver();
        MiniGameRuntimeMailbox::SetMajorTelegraphActive(m_sceneToken, false);
        m_telegraphs.ClearForScene(m_sceneToken);
        m_itemForecasts.clear();
    }

    void SubmitItemForecast(
        const ColorTerritory::TerritoryItemForecast& forecast
    ) {
        if (m_sceneToken == 0) {
            return;
        }
        const Vec2 position = TileToWorld(forecast.tile);
        const std::uint64_t id = NextId(
            forecast.type == ColorTerritory::TerritoryItemType::Bomb
                ? 20u
                : 21u
        );
        m_itemForecasts[id] = {
            .type = forecast.type,
            .position = position
        };
        m_telegraphs.Submit({
            .id = id,
            .sceneToken = m_sceneToken,
            .priority = forecast.type == ColorTerritory::TerritoryItemType::Bomb
                ? TelegraphPriority::Major
                : TelegraphPriority::Minor,
            .worldPosition = position,
            .shape = TelegraphShape::Ring,
            .radius = forecast.type == ColorTerritory::TerritoryItemType::Bomb
                ? 1.65f
                : 1.0f,
            .warningSeconds = ItemFallingSeconds,
            .armedSeconds = 0.0f,
            .resolvingSeconds = 0.1f,
            .aftermathSeconds = 0.35f,
            .label = forecast.type == ColorTerritory::TerritoryItemType::Bomb
                ? "BOMB DROPPING"
                : "STAR DROPPING"
        });

        // Observerはゲーム本体のSpawnItem中に呼ばれる。
        // Bombだけは同一frameの通常Item演出より先にMajor gateを立てる。
        if (forecast.type == ColorTerritory::TerritoryItemType::Bomb) {
            MiniGameRuntimeMailbox::SetMajorTelegraphActive(
                m_sceneToken,
                true
            );
        }
    }

    static Vec2 TileToWorld(ColorTerritory::TileCoord tile) noexcept {
        return {
            (static_cast<float>(tile.x) -
                static_cast<float>(BoardWidth - 1) * 0.5f) * TileSpacing,
            (static_cast<float>(tile.y) -
                static_cast<float>(BoardHeight - 1) * 0.5f) * TileSpacing
        };
    }

    std::uint64_t NextId(std::uint64_t family) noexcept {
        return family * 100000u + m_nextTelegraphId++;
    }

    WorldEventTelegraphModel m_telegraphs;
    std::unordered_map<std::uint64_t, ItemForecastRecord> m_itemForecasts;
    SceneToken m_sceneToken = 0;
    std::uint64_t m_nextTelegraphId = 1;
    float m_elapsedSeconds = 0.0f;
    bool m_rushWarningScheduled = false;
};

class SheepRoundupEventTelegraphRuntime final
    : public MiniGameRuntimeScriptBase {
public:
    SheepRoundupEventTelegraphRuntime()
        : MiniGameRuntimeScriptBase("SheepRoundupEventTelegraphRuntime") {
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, -50);
        SetExecutionOrder(SystemTaskDomain::Render, SystemPhase::Late, 300);
    }

private:
    static constexpr float CountdownSeconds = 3.0f;
    static constexpr float RushWarningGameElapsedSeconds = 21.0f;

    void OnInitialize() override {
        SheepRoundup::SheepRoundupRules::SetSpawnWarningObserver(
            [this](const SheepRoundup::SheepSpawnWarning& warning) {
                SubmitSpawnWarning(warning);
            }
        );
    }

    void OnStart() override {
        m_sceneToken = GetRuntimeSceneToken();
        m_elapsedSeconds = 0.0f;
        m_rushWarningScheduled = false;
        m_nextTelegraphId = 1;
        m_telegraphs.Clear();
        MiniGameRuntimeMailbox::SetMajorTelegraphActive(m_sceneToken, false);
    }

    void OnUpdate(float dt) override {
        const float delta = (std::max)(0.0f, dt);
        m_elapsedSeconds += delta;
        const float gameElapsed = (std::max)(
            0.0f,
            m_elapsedSeconds - CountdownSeconds
        );
        if (!m_rushWarningScheduled &&
            gameElapsed >= RushWarningGameElapsedSeconds) {
            m_rushWarningScheduled = true;
            m_telegraphs.Submit({
                .id = NextId(50u),
                .sceneToken = m_sceneToken,
                .priority = TelegraphPriority::Major,
                .worldPosition = {},
                .shape = TelegraphShape::Screen,
                .radius = 0.0f,
                .warningSeconds = 4.0f,
                .armedSeconds = 0.0f,
                .resolvingSeconds = 0.18f,
                .aftermathSeconds = 0.9f,
                .label = "FLOCK RUSH IN 4"
            });
        }

        m_telegraphs.Tick(delta);
        MiniGameRuntimeMailbox::SetMajorTelegraphActive(
            m_sceneToken,
            m_telegraphs.HasActiveMajor()
        );
    }

    void OnFixedUpdate(float dt) override { (void)dt; }

    void OnDraw() override {
        WorldEventTelegraphPresenter::Draw(
            GetEntityRef().GetScene(),
            m_telegraphs,
            22.0f,
            15.0f
        );
    }

    void OnEditorUpdate(float dt) override { (void)dt; }

    void OnStop() override {
        SheepRoundup::SheepRoundupRules::ClearSpawnWarningObserver();
        MiniGameRuntimeMailbox::SetMajorTelegraphActive(m_sceneToken, false);
        m_telegraphs.ClearForScene(m_sceneToken);
    }

    void SubmitSpawnWarning(
        const SheepRoundup::SheepSpawnWarning& warning
    ) {
        if (m_sceneToken == 0) {
            return;
        }
        m_telegraphs.Submit({
            .id = NextId(warning.golden ? 61u : 60u),
            .sceneToken = m_sceneToken,
            .priority = warning.golden
                ? TelegraphPriority::Major
                : TelegraphPriority::Minor,
            .worldPosition = warning.position,
            .shape = TelegraphShape::Ring,
            .radius = warning.golden ? 1.5f : 0.9f,
            .warningSeconds = warning.warningSeconds,
            .armedSeconds = 0.0f,
            .resolvingSeconds = warning.golden ? 0.12f : 0.05f,
            .aftermathSeconds = warning.golden ? 0.8f : 0.25f,
            .label = warning.golden
                ? "GOLDEN SHEEP / 3 POINTS"
                : "SHEEP INCOMING"
        });

        if (warning.golden) {
            MiniGameRuntimeMailbox::SetMajorTelegraphActive(
                m_sceneToken,
                true
            );
        }
    }

    std::uint64_t NextId(std::uint64_t family) noexcept {
        return family * 100000u + m_nextTelegraphId++;
    }

    WorldEventTelegraphModel m_telegraphs;
    SceneToken m_sceneToken = 0;
    std::uint64_t m_nextTelegraphId = 1;
    float m_elapsedSeconds = 0.0f;
    bool m_rushWarningScheduled = false;
};

} // namespace MiniGameCollection::Runtime
