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
    static constexpr float PracticeResetDisplaySeconds = 0.55f;

    void OnInitialize() override {
        m_sceneToken = GetRuntimeSceneToken();
        m_bypassBriefing =
            MiniGameRuntimeMailbox::ConsumeBriefingBypass(m_gameId);
        if (!m_bypassBriefing) {
            // PracticeはTutorial Guidanceとは別の実行コンテキスト。
            // Score/Hit/Item/Telegraphは本番同様に動かし、Resultと通常遷移だけ分離する。
            MiniGameRuntimeMailbox::BeginPractice(m_sceneToken);
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

        if (m_practiceResetPending) {
            m_practiceResetRemainingSeconds = (std::max)(
                0.0f,
                m_practiceResetRemainingSeconds - delta
            );
            if (m_practiceResetRemainingSeconds <= 0.0f) {
                ReloadPracticeRound();
            }
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

        // 各Game RuntimeがResultへ到達した場合は、公式Result状態を見せず
        // 練習ラウンドだけを初期状態から再生成する。
        if (MiniGameRuntimeMailbox::ConsumePracticeRoundFinished(m_sceneToken)) {
            BeginPracticeReset();
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

        // Game RuntimeのUpdate後にResult通知が届いたFrameでも、Render順1000の
        // reset coverを重ねることでResult panelの1Frame露出を防ぐ。
        if (!m_practiceResetPending &&
            MiniGameRuntimeMailbox::ConsumePracticeRoundFinished(m_sceneToken)) {
            BeginPracticeReset();
        }

        if (m_practiceResetPending) {
            DrawPracticeReset();
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
        MiniGameRuntimeMailbox::EndPractice(m_sceneToken);
        MiniGameRuntimeMailbox::EndGuidance(m_sceneToken);
        m_briefing.Clear();
    }

private:
    void BeginPracticeBriefing() {
        m_finished = false;
        m_releasePending = false;
        m_practiceResetPending = false;
        m_practiceTransitionSubmitted = false;
        m_readyRemainingSeconds = 0.0f;
        m_practiceResetRemainingSeconds = 0.0f;
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

    void BeginPracticeReset() {
        if (m_practiceResetPending || m_finished) {
            return;
        }
        m_practiceResetPending = true;
        m_practiceTransitionSubmitted = false;
        m_practiceResetRemainingSeconds = PracticeResetDisplaySeconds;
    }

    void DrawPracticeReset() const {
        MiniGameRuntimeUi ui(GetEntityRef().GetScene());
        if (!ui.IsAvailable()) {
            return;
        }

        const float panelWidth = ui.ResolvePanelWidth(620.0f, 420.0f);
        const float panelHeight = 156.0f;
        const float panelX = ui.CenteredX(panelWidth);
        const float panelY = (ui.Height() - panelHeight) * 0.5f;

        // 下でGame RuntimeがResult UIを描いていても完全に隠す。
        ui.FillPanel(
            0.0f,
            0.0f,
            ui.Width(),
            ui.Height(),
            D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.72f)
        );
        ui.FillPanel(
            panelX,
            panelY,
            panelWidth,
            panelHeight,
            D2D1::ColorF(0.018f, 0.027f, 0.052f, 0.98f)
        );
        ui.DrawTextCentered(
            "PRACTICE",
            ui.Width() * 0.5f,
            panelY + 22.0f,
            18.0f,
            D2D1::ColorF(0.36f, 0.78f, 1.0f, 1.0f),
            false
        );
        ui.DrawTextCentered(
            "RESETTING ROUND",
            ui.Width() * 0.5f,
            panelY + 50.0f,
            30.0f
        );
        ui.DrawTextCentered(
            "練習結果は記録されません",
            ui.Width() * 0.5f,
            panelY + 101.0f,
            17.0f,
            D2D1::ColorF(0.7f, 0.78f, 0.9f, 1.0f)
        );
    }

    void ReloadPracticeRound() {
        if (m_practiceTransitionSubmitted) {
            return;
        }

        const bool accepted = MiniGameRuntimeMailbox::SubmitPracticeTransition({
            .sceneToken = m_sceneToken,
            .sourceSceneName = GetRuntimeSceneName(),
            .targetScenePath = ScenePathForGame(m_gameId),
            .reason = TransitionRequest::Retry,
            .presentationWaitSeconds = 0.05f
        });
        if (accepted) {
            m_practiceTransitionSubmitted = true;
            return;
        }

        // 一時的に別Transitionが残っていた場合は、Result画面を露出させず再試行する。
        m_practiceResetRemainingSeconds = 0.1f;
    }

    void StartFullGameFromCleanState() {
        m_briefing.ConfirmReady();
        MiniGameRuntimeMailbox::MarkBriefingCompleted(m_gameId);
        MiniGameRuntimeMailbox::EndPractice(m_sceneToken);

        m_finished = true;
        m_releasePending = false;

        // Transition処理が同一frameで進んでも、再ロード先が先にbypassを
        // 取得できるよう、request送信前にone-shot stateをarmする。
        MiniGameRuntimeMailbox::ArmBriefingBypass(m_gameId);
        if (SubmitTransition(
                ScenePathForGame(m_gameId),
                TransitionRequest::Retry,
                0.05f)) {
            // 練習中のscore、timer、配置、乱数状態を破棄し、Matchとして再生成する。
            return;
        }

        // Transitionが受理されなかった場合はone-shot stateを取り消し、
        // Practiceを復元して説明だけ消える状態を避ける。
        MiniGameRuntimeMailbox::ConsumeBriefingBypass(m_gameId);
        MiniGameRuntimeMailbox::BeginPractice(m_sceneToken);
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
    float m_practiceResetRemainingSeconds = 0.0f;
    bool m_bypassBriefing = false;
    bool m_releasePending = false;
    bool m_practiceResetPending = false;
    bool m_practiceTransitionSubmitted = false;
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
