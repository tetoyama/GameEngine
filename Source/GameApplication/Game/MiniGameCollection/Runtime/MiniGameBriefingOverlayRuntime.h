#pragma once

#include "Game/MiniGameCollection/Core/MiniGameBriefingModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameBriefingPresenter.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MiniGameCollection::Runtime {

class MiniGameBriefingOverlayRuntimeBase : public MiniGameRuntimeScriptBase {
public:
    MiniGameBriefingOverlayRuntimeBase(
        std::string scriptName,
        MiniGameId gameId,
        std::string title,
        std::string objective,
        std::string controls,
        std::array<std::string, 3> tips
    )
        : MiniGameRuntimeScriptBase(std::move(scriptName)),
          m_gameId(gameId),
          m_title(std::move(title)),
          m_objective(std::move(objective)),
          m_controls(std::move(controls)),
          m_tips(std::move(tips)) {
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, -1000);
        SetExecutionOrder(SystemTaskDomain::Fixed, SystemPhase::Default, -1000);
        SetExecutionOrder(SystemTaskDomain::Render, SystemPhase::Late, 1000);
        SetIgnoreSceneUpdateSuspension(true);
    }

protected:
    static constexpr float ReadyDisplaySeconds = 0.4f;

    void OnInitialize() override {
        m_sceneToken = GetRuntimeSceneToken();
        m_bypassBriefing =
            MiniGameRuntimeMailbox::ConsumeBriefingBypass(m_gameId);
        if (!m_bypassBriefing) {
            // Full-game practiceではCustomScriptComponent::SuspendSceneUpdatesを使わない。
            // 本番Runtime、CPU、timer、item、telegraphを実際に動かして理解させる。
            MiniGameRuntimeMailbox::BeginGuidance(m_sceneToken);
        }
    }

    void OnStart() override {
        if (m_bypassBriefing) {
            m_finished = true;
            return;
        }
        BeginPracticeBriefing();
    }

    void OnUpdate(float dt) override {
        const float delta = (std::max)(0.0f, dt);
        if (m_finished) {
            return;
        }

        if (m_releasePending) {
            StartFullGameFromCleanState();
            return;
        }

        if (m_briefing.IsReady()) {
            m_readyRemainingSeconds = (std::max)(
                0.0f,
                m_readyRemainingSeconds - delta
            );
            if (m_readyRemainingSeconds <= 0.0f) {
                m_releasePending = true;
            }
            return;
        }

        const BriefingEvent events = m_briefing.Tick(
            delta,
            {
                .stepSucceeded = false,
                .resetRequested = false,
                .skipKeyHeld = CustomScriptComponent::GetKey(VK_RETURN)
            }
        );

        if (HasBriefingEvent(events, BriefingEvent::ReadyReached)) {
            m_readyRemainingSeconds = ReadyDisplaySeconds;
        }
    }

    void OnFixedUpdate(float dt) override { (void)dt; }

    void OnDraw() override {
        if (m_finished) {
            return;
        }

        const std::array<std::string_view, 3> tips{
            m_tips[0],
            m_tips[1],
            m_tips[2]
        };
        MiniGameBriefingPresenter::Draw(
            GetEntityRef().GetScene(),
            m_title,
            m_objective,
            m_controls,
            tips,
            m_briefing
        );
    }

    void OnEditorUpdate(float dt) override { (void)dt; }

    void OnStop() override {
        MiniGameRuntimeMailbox::EndGuidance(m_sceneToken);
        m_briefing.Clear();
    }

private:
    void BeginPracticeBriefing() {
        m_finished = false;
        m_releasePending = false;
        m_readyRemainingSeconds = 0.0f;
        m_briefing.Clear();
        m_briefing.SetSteps({
            {
                "本番と同じゲームを自由に練習する",
                0.0f,
                true
            }
        });
        m_briefing.SetSkipSettings({
            .holdSeconds = 1.0f,
            .requireReleaseToArm = true
        });
        m_briefing.Begin(
            MiniGameRuntimeMailbox::ResolveBriefingMode(m_gameId)
        );
    }

    void StartFullGameFromCleanState() {
        m_briefing.ConfirmReady();
        MiniGameRuntimeMailbox::MarkBriefingCompleted(m_gameId);
        MiniGameRuntimeMailbox::EndGuidance(m_sceneToken);

        m_finished = true;
        m_releasePending = false;

        if (SubmitTransition(
                ScenePathForGame(m_gameId),
                TransitionRequest::Retry,
                0.05f)) {
            // 同じSceneを再ロードし、練習中のscore、timer、配置、乱数状態を
            // 完全に破棄する。次の1回だけBriefingを表示しない。
            MiniGameRuntimeMailbox::ArmBriefingBypass(m_gameId);
            return;
        }

        // Transitionが受理されなかった場合に、説明だけ消えて操作不能にしない。
        MiniGameRuntimeMailbox::BeginGuidance(m_sceneToken);
        BeginPracticeBriefing();
    }

    static std::string ScenePathForGame(MiniGameId gameId) {
        switch (gameId) {
        case MiniGameId::ColorTerritory:
            return "Asset/Game/MiniGameCollection/Scene/ColorTerritory/ColorTerritory.scene";
        case MiniGameId::SheepRoundup:
            return "Asset/Game/MiniGameCollection/Scene/SheepRoundup/SheepRoundup.scene";
        case MiniGameId::Backshot:
            return "Asset/Game/MiniGameCollection/Scene/Backshot/Backshot.scene";
        default:
            return {};
        }
    }

    MiniGameId m_gameId = MiniGameId::ColorTerritory;
    std::string m_title;
    std::string m_objective;
    std::string m_controls;
    std::array<std::string, 3> m_tips{};
    MiniGameBriefingModel m_briefing;
    SceneToken m_sceneToken = 0;
    float m_readyRemainingSeconds = 0.0f;
    bool m_bypassBriefing = false;
    bool m_releasePending = false;
    bool m_finished = false;
};

class ColorTerritoryBriefingRuntime final
    : public MiniGameBriefingOverlayRuntimeBase {
public:
    ColorTerritoryBriefingRuntime()
        : MiniGameBriefingOverlayRuntimeBase(
            "ColorTerritoryBriefingRuntime",
            MiniGameId::ColorTerritory,
            "COLOR TERRITORY",
            "塗ったマスが最も多いプレイヤーを目指す",
            "WASD / 矢印：移動",
            {
                "相手色を踏むと、自分+1 / 相手-1",
                "BOMBは先に触れて自分色へ。赤い3×3から離れる",
                "STARは加速+無敵。触れた相手を硬直させる"
            }
        ) {
    }
};

class SheepRoundupBriefingRuntime final
    : public MiniGameBriefingOverlayRuntimeBase {
public:
    SheepRoundupBriefingRuntime()
        : MiniGameBriefingOverlayRuntimeBase(
            "SheepRoundupBriefingRuntime",
            MiniGameId::SheepRoundup,
            "SHEEP ROUNDUP",
            "羊を自分の囲いへ追い込み、最も多く得点する",
            "WASD / 矢印：移動",
            {
                "羊は近づいたプレイヤーの反対方向へ逃げる",
                "囲い ← 羊 ← YOU の順に回り込んで押す",
                "NORMAL SHEEP = 1 POINT / GOLDEN SHEEP = 3 POINTS"
            }
        ) {
    }
};

class BackshotBriefingRuntime final
    : public MiniGameBriefingOverlayRuntimeBase {
public:
    BackshotBriefingRuntime()
        : MiniGameBriefingOverlayRuntimeBase(
            "BackshotBriefingRuntime",
            MiniGameId::Backshot,
            "BACKSHOT",
            "routeを滑り、相手の赤い背面を撃って生き残る",
            "WASD / 矢印：進行方向    SPACE：射撃",
            {
                "WHITE FRONT = GUARD / RED REAR = ELIMINATION",
                "Cornerは自動旋回。T字 / Crossでは停止して方向を選ぶ",
                "BOOST SLIDESで加速。黄色routeの閉鎖予告は迂回する"
            }
        ) {
    }
};

} // namespace MiniGameCollection::Runtime
