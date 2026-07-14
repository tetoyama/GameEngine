#pragma once

#include "Game/MiniGameCollection/Core/MiniGameBriefingModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameBriefingPresenter.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace MiniGameCollection::Runtime {

class MiniGameBriefingOverlayRuntimeBase : public MiniGameRuntimeScriptBase {
public:
    MiniGameBriefingOverlayRuntimeBase(
        std::string scriptName,
        MiniGameId gameId,
        std::string title
    )
        : MiniGameRuntimeScriptBase(std::move(scriptName)),
          m_gameId(gameId),
          m_title(std::move(title)) {
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Default, -1000);
        SetExecutionOrder(SystemTaskDomain::Fixed, SystemPhase::Default, -1000);
        SetExecutionOrder(SystemTaskDomain::Render, SystemPhase::Late, 1000);
        SetIgnoreSceneUpdateSuspension(true);
    }

protected:
    struct DemoCube {
        ComponentRef<TransformComponent> transform;
        ComponentRef<MaterialComponent> material;
    };

    static constexpr std::size_t DemoCubeCount = 16;
    static constexpr float ReadyDisplaySeconds = 0.55f;

    void OnInitialize() override {
        m_sceneContext = GetEntityRef().GetScene();
        m_sceneToken = GetRuntimeSceneToken();
        if (m_sceneContext && !m_suspensionOwned) {
            CustomScriptComponent::SuspendSceneUpdates(m_sceneContext);
            m_suspensionOwned = true;
        }
        MiniGameRuntimeMailbox::BeginGuidance(m_sceneToken);
    }

    void OnStart() override {
        m_finished = false;
        m_releasePending = false;
        m_readyRemainingSeconds = 0.0f;
        m_briefing.SetSteps(BuildSteps());
        m_briefing.SetSkipSettings({
            .holdSeconds = 1.0f,
            .requireReleaseToArm = true
        });
        m_briefing.Begin(
            MiniGameRuntimeMailbox::ResolveBriefingMode(m_gameId)
        );
        QueueDemoCubes();
        ResetCurrentStep();
    }

    void OnUpdate(float dt) override {
        const float delta = (std::max)(0.0f, dt);
        if (m_finished) {
            return;
        }

        if (m_releasePending) {
            CompleteAndReleaseGame();
            return;
        }

        if (m_briefing.IsReady()) {
            m_readyRemainingSeconds = (std::max)(
                0.0f,
                m_readyRemainingSeconds - delta
            );
            if (m_readyRemainingSeconds <= 0.0f) {
                // Countdown開始をBriefing完了frameと分離する。
                m_releasePending = true;
            }
            return;
        }

        const bool resetRequested =
            CustomScriptComponent::GetKeyDown('R');
        const bool stepSucceeded = resetRequested
            ? false
            : UpdateCurrentStep(delta);

        const BriefingEvent events = m_briefing.Tick(
            delta,
            {
                .stepSucceeded = stepSucceeded,
                .resetRequested = resetRequested,
                .skipKeyHeld = CustomScriptComponent::GetKey(VK_RETURN)
            }
        );

        if (HasBriefingEvent(events, BriefingEvent::StepReset)) {
            ResetCurrentStep();
        }
        if (HasBriefingEvent(events, BriefingEvent::StepStarted)) {
            ResetCurrentStep();
        }
        if (HasBriefingEvent(events, BriefingEvent::ReadyReached)) {
            HideAllDemoCubes();
            m_readyRemainingSeconds = ReadyDisplaySeconds;
        }
    }

    void OnFixedUpdate(float dt) override { (void)dt; }

    void OnDraw() override {
        if (m_finished) {
            return;
        }
        MiniGameBriefingPresenter::Draw(
            GetEntityRef().GetScene(),
            m_title,
            m_briefing
        );
        DrawCurrentStepStatus();
    }

    void OnEditorUpdate(float dt) override { (void)dt; }

    void OnStop() override {
        HideAllDemoCubes();
        MiniGameRuntimeMailbox::EndGuidance(m_sceneToken);
        ReleaseSceneSuspension();
        m_briefing.Clear();
    }

    virtual std::vector<BriefingStepDefinition> BuildSteps() const = 0;
    virtual void ResetCurrentStep() = 0;
    virtual bool UpdateCurrentStep(float deltaTime) = 0;
    virtual void DrawCurrentStepStatus() const {}

    std::size_t CurrentStep() const noexcept {
        return m_briefing.GetCurrentStepIndex();
    }

    BriefingMode Mode() const noexcept {
        return m_briefing.GetMode();
    }

    Vec2 ReadDemoMovement() const noexcept {
        return ReadMovementInput();
    }

    bool IsSpacePressed() const noexcept {
        return CustomScriptComponent::GetKeyDown(VK_SPACE);
    }

    void DrawDemoStatus(
        std::string_view text,
        D2D1::ColorF color = D2D1::ColorF(0.72f, 0.86f, 1.0f, 1.0f)
    ) const {
        MiniGameRuntimeUi ui(GetEntityRef().GetScene());
        if (!ui.IsAvailable() || text.empty()) {
            return;
        }
        const float width = (std::min)(680.0f, ui.Width() - 48.0f);
        const float x = (ui.Width() - width) * 0.5f;
        const float y = (std::max)(310.0f, ui.Height() - 154.0f);
        ui.FillPanel(
            x,
            y,
            width,
            54.0f,
            D2D1::ColorF(0.012f, 0.02f, 0.04f, 0.88f)
        );
        ui.DrawTextCentered(
            text,
            ui.Width() * 0.5f,
            y + 15.0f,
            17.0f,
            color,
            false
        );
        ui.DrawTextCentered(
            "R：この手順をやり直す",
            ui.Width() * 0.5f,
            y + 36.0f,
            12.0f,
            D2D1::ColorF(0.55f, 0.64f, 0.78f, 1.0f),
            false
        );
    }

    void SetDemoCube(
        std::size_t index,
        Vec2 position,
        Vector3 scale,
        DirectX::XMFLOAT4 color,
        float height = 0.45f,
        float emissive = 0.35f
    ) {
        if (index >= m_demoCubes.size()) {
            return;
        }
        if (TransformComponent* transform = m_demoCubes[index].transform.TryGet()) {
            transform->position = Vector3(position.x, height, position.y);
            transform->scale = scale;
        }
        if (MaterialComponent* material = m_demoCubes[index].material.TryGet()) {
            material->Material.BaseColor = color;
            material->Material.EmissiveColor = DirectX::XMFLOAT3(
                color.x,
                color.y,
                color.z
            );
            material->Material.EmissiveIntensity = emissive;
        }
    }

    void HideDemoCube(std::size_t index) {
        if (index >= m_demoCubes.size()) {
            return;
        }
        if (TransformComponent* transform = m_demoCubes[index].transform.TryGet()) {
            transform->position = HiddenDemoPosition();
            transform->scale = Vector3();
        }
    }

    void HideAllDemoCubes() {
        for (std::size_t index = 0; index < m_demoCubes.size(); ++index) {
            HideDemoCube(index);
        }
    }

    static Vec2 ClampDemoPosition(Vec2 position) noexcept {
        return {
            (std::clamp)(position.x, -4.4f, 4.4f),
            (std::clamp)(position.y, -2.8f, 2.8f)
        };
    }

private:
    void QueueDemoCubes() {
        for (std::size_t index = 0; index < m_demoCubes.size(); ++index) {
            QueueCube(
                "BriefingDemo_" + m_title + "_" + std::to_string(index),
                HiddenDemoPosition(),
                Vector3(),
                DirectX::XMFLOAT4(0.3f, 0.7f, 1.0f, 1.0f),
                [this, index](const CubeVisualRefs& refs) {
                    m_demoCubes[index].transform = refs.transform;
                    m_demoCubes[index].material = refs.material;
                    HideDemoCube(index);
                    ResetCurrentStep();
                }
            );
        }
    }

    void CompleteAndReleaseGame() {
        m_briefing.ConfirmReady();
        MiniGameRuntimeMailbox::MarkBriefingCompleted(m_gameId);
        MiniGameRuntimeMailbox::EndGuidance(m_sceneToken);
        HideAllDemoCubes();
        m_finished = true;
        m_releasePending = false;
        SubmitPresentation(RuntimePresentationCommandType::Countdown);
        ReleaseSceneSuspension();
    }

    void ReleaseSceneSuspension() {
        if (m_sceneContext && m_suspensionOwned) {
            CustomScriptComponent::ResumeSceneUpdates(m_sceneContext);
            m_suspensionOwned = false;
        }
    }

    static Vector3 HiddenDemoPosition() noexcept {
        return Vector3(0.0f, -1000.0f, 0.0f);
    }

    MiniGameId m_gameId = MiniGameId::ColorTerritory;
    std::string m_title;
    MiniGameBriefingModel m_briefing;
    std::array<DemoCube, DemoCubeCount> m_demoCubes{};
    SceneContext* m_sceneContext = nullptr;
    SceneToken m_sceneToken = 0;
    float m_readyRemainingSeconds = 0.0f;
    bool m_suspensionOwned = false;
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
            "COLOR TERRITORY"
        ) {
    }

private:
    static constexpr std::size_t PlayerCube = 0;
    static constexpr std::size_t TargetBegin = 1;
    static constexpr std::size_t EnemyTileCube = 4;
    static constexpr std::size_t BombCube = 5;
    static constexpr std::size_t StarCube = 6;
    static constexpr std::size_t BlastBegin = 7;

    std::vector<BriefingStepDefinition> BuildSteps() const override {
        return {
            {"矢印キー / WASDで、光っている3マスを塗る", 0.45f, true},
            {"赤いマスへ移動し、相手の色を奪う", 0.45f, false},
            {"BOMBへ触れ、自分の色に変える", 0.45f, true},
            {"爆発範囲の外まで逃げる", 0.55f, false},
            {"STARを取り、加速した状態で移動する", 0.55f, true}
        };
    }

    void ResetCurrentStep() override {
        HideAllDemoCubes();
        m_player = {-3.2f, 0.0f};
        m_touchedTargets.fill(false);
        m_starCollected = false;
        m_starTravel = 0.0f;

        switch (CurrentStep()) {
        case 0:
            for (std::size_t index = 0; index < 3; ++index) {
                SetDemoCube(
                    TargetBegin + index,
                    {-1.5f + static_cast<float>(index) * 1.5f, 0.0f},
                    Vector3(1.05f, 0.12f, 1.05f),
                    {0.22f, 0.72f, 1.0f, 1.0f},
                    0.05f,
                    1.2f
                );
            }
            break;
        case 1:
            SetDemoCube(
                EnemyTileCube,
                {0.0f, 0.0f},
                Vector3(1.2f, 0.12f, 1.2f),
                {1.0f, 0.2f, 0.18f, 1.0f},
                0.05f,
                0.9f
            );
            break;
        case 2:
            SetDemoCube(
                BombCube,
                {0.0f, 0.0f},
                Vector3(0.85f, 0.85f, 0.85f),
                {1.0f, 0.34f, 0.06f, 1.0f},
                0.52f,
                3.0f
            );
            break;
        case 3:
            m_player = {0.0f, 0.0f};
            for (int y = -1; y <= 1; ++y) {
                for (int x = -1; x <= 1; ++x) {
                    const std::size_t index = BlastBegin +
                        static_cast<std::size_t>((y + 1) * 3 + x + 1);
                    SetDemoCube(
                        index,
                        {static_cast<float>(x), static_cast<float>(y)},
                        Vector3(0.9f, 0.08f, 0.9f),
                        {1.0f, 0.22f, 0.06f, 1.0f},
                        0.04f,
                        1.3f
                    );
                }
            }
            break;
        case 4:
            m_player = {-2.5f, 0.0f};
            SetDemoCube(
                StarCube,
                {0.0f, 0.0f},
                Vector3(0.72f, 0.72f, 0.72f),
                {1.0f, 0.9f, 0.12f, 1.0f},
                0.58f,
                4.0f
            );
            break;
        default:
            break;
        }
        UpdatePlayerVisual();
    }

    bool UpdateCurrentStep(float deltaTime) override {
        const Vec2 movement = ReadDemoMovement();
        const float speed = m_starCollected ? 5.8f : 3.7f;
        const Vec2 previous = m_player;
        m_player = ClampDemoPosition(
            m_player + movement * speed * deltaTime
        );
        UpdatePlayerVisual();

        switch (CurrentStep()) {
        case 0: {
            for (std::size_t index = 0; index < 3; ++index) {
                const Vec2 target{
                    -1.5f + static_cast<float>(index) * 1.5f,
                    0.0f
                };
                if (Distance(m_player, target) <= 0.62f) {
                    m_touchedTargets[index] = true;
                    SetDemoCube(
                        TargetBegin + index,
                        target,
                        Vector3(1.05f, 0.12f, 1.05f),
                        {0.12f, 0.42f, 0.95f, 1.0f},
                        0.05f,
                        0.25f
                    );
                }
            }
            return std::all_of(
                m_touchedTargets.begin(),
                m_touchedTargets.end(),
                [](bool touched) { return touched; }
            );
        }
        case 1:
            return Distance(m_player, {0.0f, 0.0f}) <= 0.62f;
        case 2:
            if (Distance(m_player, {0.0f, 0.0f}) <= 0.68f) {
                SetDemoCube(
                    BombCube,
                    {0.0f, 0.0f},
                    Vector3(0.95f, 0.95f, 0.95f),
                    {0.18f, 0.62f, 1.0f, 1.0f},
                    0.52f,
                    3.6f
                );
                return true;
            }
            return false;
        case 3:
            return Distance(m_player, {0.0f, 0.0f}) >= 2.65f;
        case 4:
            if (!m_starCollected &&
                Distance(m_player, {0.0f, 0.0f}) <= 0.68f) {
                m_starCollected = true;
                HideDemoCube(StarCube);
            }
            if (m_starCollected) {
                m_starTravel += Distance(previous, m_player);
                return m_starTravel >= 1.25f;
            }
            return false;
        default:
            return false;
        }
    }

    void DrawCurrentStepStatus() const override {
        switch (CurrentStep()) {
        case 0: {
            const int count = static_cast<int>(std::count(
                m_touchedTargets.begin(),
                m_touchedTargets.end(),
                true
            ));
            DrawDemoStatus("塗ったマス  " + std::to_string(count) + " / 3");
            break;
        }
        case 1:
            DrawDemoStatus("敵色を奪うと、自分+1 / 相手-1");
            break;
        case 2:
            DrawDemoStatus("先に触れたBOMBは、自分の色で周囲を塗る");
            break;
        case 3:
            DrawDemoStatus("赤い3×3の外へ移動する");
            break;
        case 4:
            DrawDemoStatus(
                m_starCollected
                    ? "STAR ACTIVE：速度上昇 + 無敵"
                    : "黄色いSTARへ移動する",
                D2D1::ColorF(1.0f, 0.9f, 0.22f, 1.0f)
            );
            break;
        default:
            break;
        }
    }

    void UpdatePlayerVisual() {
        SetDemoCube(
            PlayerCube,
            m_player,
            Vector3(0.68f, 1.0f, 0.68f),
            m_starCollected
                ? DirectX::XMFLOAT4(1.0f, 0.88f, 0.16f, 1.0f)
                : DirectX::XMFLOAT4(0.16f, 0.62f, 1.0f, 1.0f),
            0.62f,
            m_starCollected ? 3.5f : 0.8f
        );
    }

    Vec2 m_player{};
    std::array<bool, 3> m_touchedTargets{};
    bool m_starCollected = false;
    float m_starTravel = 0.0f;
};

class SheepRoundupBriefingRuntime final
    : public MiniGameBriefingOverlayRuntimeBase {
public:
    SheepRoundupBriefingRuntime()
        : MiniGameBriefingOverlayRuntimeBase(
            "SheepRoundupBriefingRuntime",
            MiniGameId::SheepRoundup,
            "SHEEP ROUNDUP"
        ) {
    }

private:
    static constexpr std::size_t PlayerCube = 0;
    static constexpr std::size_t SheepCube = 1;
    static constexpr std::size_t PenCube = 2;
    static constexpr std::size_t ScoreCube = 3;

    std::vector<BriefingStepDefinition> BuildSteps() const override {
        return {
            {"羊へ近づき、反対方向へ逃げることを確かめる", 0.45f, true},
            {"羊から見て、囲いと反対側へ回り込む", 0.45f, false},
            {"羊を青い囲いへ押し込む", 0.55f, true},
            {"矢印キーを押し、通常羊が1点と確認する", 0.45f, false},
            {"金羊を囲いへ入れ、3点を獲得する", 0.55f, true}
        };
    }

    void ResetCurrentStep() override {
        HideAllDemoCubes();
        m_pen = {-3.1f, 0.0f};
        m_sheep = {0.0f, 0.0f};
        m_player = CurrentStep() == 0
            ? Vec2{-3.0f, 0.0f}
            : Vec2{2.5f, 0.0f};
        m_contactObserved = false;
        m_golden = CurrentStep() == 4;

        SetDemoCube(
            PenCube,
            m_pen,
            Vector3(1.45f, 0.12f, 2.1f),
            {0.12f, 0.46f, 1.0f, 1.0f},
            0.05f,
            1.4f
        );
        UpdateVisuals();

        if (CurrentStep() == 3) {
            m_sheep = m_pen;
            SetDemoCube(
                ScoreCube,
                {-1.4f, 1.6f},
                Vector3(1.5f, 0.12f, 0.5f),
                {0.22f, 0.9f, 0.42f, 1.0f},
                0.2f,
                2.0f
            );
            UpdateVisuals();
        }
    }

    bool UpdateCurrentStep(float deltaTime) override {
        const Vec2 movement = ReadDemoMovement();
        m_player = ClampDemoPosition(
            m_player + movement * 3.6f * deltaTime
        );

        const float distance = Distance(m_player, m_sheep);
        if (distance < 1.55f && CurrentStep() != 3) {
            const Vec2 flee = NormalizeOrZero(m_sheep - m_player);
            m_sheep = ClampDemoPosition(
                m_sheep + flee * 1.5f * deltaTime
            );
            m_contactObserved = true;
        }
        UpdateVisuals();

        switch (CurrentStep()) {
        case 0:
            return m_contactObserved && distance < 1.2f;
        case 1: {
            const Vec2 toPlayer = NormalizeOrZero(m_player - m_sheep);
            const Vec2 toPen = NormalizeOrZero(m_pen - m_sheep);
            return Distance(m_player, m_sheep) >= 1.0f &&
                Dot(toPlayer, toPen) <= -0.72f;
        }
        case 2:
            return Distance(m_sheep, m_pen) <= 0.78f;
        case 3:
            return LengthSquared(movement) > 0.01f;
        case 4:
            return Distance(m_sheep, m_pen) <= 0.78f;
        default:
            return false;
        }
    }

    void DrawCurrentStepStatus() const override {
        switch (CurrentStep()) {
        case 0:
            DrawDemoStatus("羊は最も近いプレイヤーから反対方向へ逃げる");
            break;
        case 1:
            DrawDemoStatus("青い囲い ← 羊 ← YOU の順に並ぶ");
            break;
        case 2:
            DrawDemoStatus("追いかけるより、反対側から押す");
            break;
        case 3:
            DrawDemoStatus("NORMAL SHEEP  =  1 POINT");
            break;
        case 4:
            DrawDemoStatus(
                "GOLDEN SHEEP  =  3 POINTS",
                D2D1::ColorF(1.0f, 0.82f, 0.16f, 1.0f)
            );
            break;
        default:
            break;
        }
    }

    void UpdateVisuals() {
        SetDemoCube(
            PlayerCube,
            m_player,
            Vector3(0.7f, 1.0f, 0.7f),
            {0.16f, 0.62f, 1.0f, 1.0f},
            0.62f,
            0.8f
        );
        SetDemoCube(
            SheepCube,
            m_sheep,
            Vector3(0.78f, 0.72f, 0.92f),
            m_golden
                ? DirectX::XMFLOAT4(1.0f, 0.72f, 0.08f, 1.0f)
                : DirectX::XMFLOAT4(0.92f, 0.94f, 1.0f, 1.0f),
            0.5f,
            m_golden ? 3.5f : 0.12f
        );
    }

    Vec2 m_player{};
    Vec2 m_sheep{};
    Vec2 m_pen{};
    bool m_contactObserved = false;
    bool m_golden = false;
};

class BackshotBriefingRuntime final
    : public MiniGameBriefingOverlayRuntimeBase {
public:
    BackshotBriefingRuntime()
        : MiniGameBriefingOverlayRuntimeBase(
            "BackshotBriefingRuntime",
            MiniGameId::Backshot,
            "BACKSHOT"
        ) {
    }

private:
    static constexpr std::size_t PlayerCube = 0;
    static constexpr std::size_t TargetCube = 1;
    static constexpr std::size_t FrontCube = 2;
    static constexpr std::size_t RearCube = 3;
    static constexpr std::size_t BoostCube = 4;
    static constexpr std::size_t BlockerCube = 5;
    static constexpr std::size_t RouteBegin = 6;

    std::vector<BriefingStepDefinition> BuildSteps() const override {
        return {
            {"矢印キーを押し、停止地点まで一直線に滑る", 0.45f, true},
            {"右へ進んだ後、上へ入力してCornerを通る", 0.45f, false},
            {"分岐点で停止し、次の方向を選ぶ", 0.45f, false},
            {"白い正面からSPACEで撃ち、Guardを確認する", 0.55f, true},
            {"赤い背面へ回り、SPACEで撃破する", 0.55f, true},
            {"BOOSTを取り、効果中に2回滑る", 0.55f, true},
            {"黄色い閉鎖予告を避け、別routeを選ぶ", 0.55f, true}
        };
    }

    void ResetCurrentStep() override {
        HideAllDemoCubes();
        m_player = {-3.2f, 0.0f};
        m_progress = 0.0f;
        m_firstDirectionDone = false;
        m_directionLatch = false;
        m_slideCount = 0;
        m_boostCollected = false;

        DrawRoute(CurrentStep());
        if (CurrentStep() == 3 || CurrentStep() == 4) {
            SetTargetVisuals();
        }
        if (CurrentStep() == 5) {
            SetDemoCube(
                BoostCube,
                {-1.0f, 0.0f},
                Vector3(0.65f, 0.65f, 0.65f),
                {0.12f, 0.72f, 1.0f, 1.0f},
                0.55f,
                4.0f
            );
        }
        if (CurrentStep() == 6) {
            SetDemoCube(
                BlockerCube,
                {0.0f, 1.5f},
                Vector3(1.0f, 0.18f, 1.0f),
                {1.0f, 0.62f, 0.08f, 1.0f},
                0.12f,
                2.8f
            );
        }
        UpdatePlayerVisual();
    }

    bool UpdateCurrentStep(float deltaTime) override {
        const Vec2 movement = ReadDemoMovement();
        const bool hasInput = LengthSquared(movement) > 0.01f;
        const bool horizontal = std::abs(movement.x) > 0.5f;
        const bool vertical = std::abs(movement.y) > 0.5f;

        switch (CurrentStep()) {
        case 0:
            if (hasInput) {
                m_progress += deltaTime;
                m_player = Lerp({-3.2f, 0.0f}, {3.2f, 0.0f},
                    (std::clamp)(m_progress / 0.7f, 0.0f, 1.0f));
            }
            UpdatePlayerVisual();
            return m_progress >= 0.7f;
        case 1:
            if (!m_firstDirectionDone && movement.x > 0.5f) {
                m_firstDirectionDone = true;
                m_player = {1.2f, -1.4f};
            } else if (m_firstDirectionDone && movement.y > 0.5f) {
                m_player = {1.2f, 1.8f};
                UpdatePlayerVisual();
                return true;
            }
            UpdatePlayerVisual();
            return false;
        case 2:
            if (!m_firstDirectionDone && horizontal) {
                m_firstDirectionDone = true;
                m_player = {0.0f, 0.0f};
            } else if (m_firstDirectionDone && vertical) {
                m_player = {0.0f, 2.0f};
                UpdatePlayerVisual();
                return true;
            }
            UpdatePlayerVisual();
            return false;
        case 3:
            return IsSpacePressed();
        case 4:
            m_player = ClampDemoPosition(
                m_player + movement * 3.5f * deltaTime
            );
            UpdatePlayerVisual();
            return Distance(m_player, {1.8f, -1.15f}) <= 0.9f &&
                IsSpacePressed();
        case 5:
            m_player = ClampDemoPosition(
                m_player + movement * (m_boostCollected ? 5.5f : 3.4f) *
                    deltaTime
            );
            if (!m_boostCollected && Distance(m_player, {-1.0f, 0.0f}) <= 0.7f) {
                m_boostCollected = true;
                HideDemoCube(BoostCube);
            }
            if (m_boostCollected) {
                if (hasInput && !m_directionLatch) {
                    ++m_slideCount;
                    m_directionLatch = true;
                } else if (!hasInput) {
                    m_directionLatch = false;
                }
            }
            UpdatePlayerVisual();
            return m_slideCount >= 2;
        case 6:
            if (movement.y > 0.5f) {
                return false;
            }
            return horizontal || movement.y < -0.5f;
        default:
            return false;
        }
    }

    void DrawCurrentStepStatus() const override {
        switch (CurrentStep()) {
        case 0:
            DrawDemoStatus("入力後は途中で方向転換できない");
            break;
        case 1:
            DrawDemoStatus("Cornerでは接続された方向へ自動で曲がる");
            break;
        case 2:
            DrawDemoStatus("T字 / Crossでは停止し、次の入力を選ぶ");
            break;
        case 3:
            DrawDemoStatus("WHITE FRONT  =  GUARD");
            break;
        case 4:
            DrawDemoStatus(
                "RED REAR  =  ELIMINATION",
                D2D1::ColorF(1.0f, 0.3f, 0.24f, 1.0f)
            );
            break;
        case 5:
            DrawDemoStatus(
                "BOOST SLIDES  " + std::to_string(m_slideCount) + " / 2",
                D2D1::ColorF(0.25f, 0.78f, 1.0f, 1.0f)
            );
            break;
        case 6:
            DrawDemoStatus("黄色routeはまもなく閉鎖。左右か下を選ぶ");
            break;
        default:
            break;
        }
    }

    void DrawRoute(std::size_t step) {
        const DirectX::XMFLOAT4 routeColor(0.18f, 0.28f, 0.42f, 1.0f);
        if (step == 1) {
            const std::array<Vec2, 5> positions{
                Vec2{-2.4f, -1.4f}, {-1.2f, -1.4f}, {0.0f, -1.4f},
                {1.2f, -1.4f}, {1.2f, 0.0f}
            };
            for (std::size_t index = 0; index < positions.size(); ++index) {
                SetDemoCube(
                    RouteBegin + index,
                    positions[index],
                    Vector3(1.0f, 0.08f, 1.0f),
                    routeColor,
                    0.03f,
                    0.3f
                );
            }
        } else if (step == 2 || step == 6) {
            const std::array<Vec2, 5> positions{
                Vec2{0.0f, 0.0f}, {-1.4f, 0.0f}, {1.4f, 0.0f},
                {0.0f, 1.4f}, {0.0f, -1.4f}
            };
            for (std::size_t index = 0; index < positions.size(); ++index) {
                SetDemoCube(
                    RouteBegin + index,
                    positions[index],
                    Vector3(1.0f, 0.08f, 1.0f),
                    routeColor,
                    0.03f,
                    0.3f
                );
            }
        } else {
            for (std::size_t index = 0; index < 7; ++index) {
                SetDemoCube(
                    RouteBegin + index,
                    {-3.6f + static_cast<float>(index) * 1.2f, 0.0f},
                    Vector3(1.0f, 0.08f, 1.0f),
                    routeColor,
                    0.03f,
                    0.3f
                );
            }
        }
    }

    void SetTargetVisuals() {
        SetDemoCube(
            TargetCube,
            {1.8f, 0.0f},
            Vector3(0.9f, 1.0f, 1.15f),
            {1.0f, 0.24f, 0.2f, 1.0f},
            0.62f,
            0.8f
        );
        SetDemoCube(
            FrontCube,
            {1.8f, 1.0f},
            Vector3(0.52f, 0.18f, 0.3f),
            {0.92f, 0.98f, 1.0f, 1.0f},
            0.72f,
            2.5f
        );
        SetDemoCube(
            RearCube,
            {1.8f, -1.0f},
            Vector3(0.52f, 0.18f, 0.3f),
            {1.0f, 0.12f, 0.08f, 1.0f},
            0.72f,
            3.2f
        );
    }

    void UpdatePlayerVisual() {
        SetDemoCube(
            PlayerCube,
            m_player,
            Vector3(0.78f, 1.0f, 1.0f),
            m_boostCollected
                ? DirectX::XMFLOAT4(0.12f, 0.82f, 1.0f, 1.0f)
                : DirectX::XMFLOAT4(0.16f, 0.62f, 1.0f, 1.0f),
            0.62f,
            m_boostCollected ? 3.0f : 0.8f
        );
    }

    Vec2 m_player{};
    float m_progress = 0.0f;
    bool m_firstDirectionDone = false;
    bool m_directionLatch = false;
    bool m_boostCollected = false;
    int m_slideCount = 0;
};

} // namespace MiniGameCollection::Runtime
