#pragma once

#include "Game/MiniGameCollection/Core/MiniGameBriefingModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeUi.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace MiniGameCollection::Runtime {

class MiniGameBriefingPresenter final {
public:
    static void Draw(
        SceneContext* context,
        std::string_view gameTitle,
        const MiniGameBriefingModel& briefing
    ) {
        if (
            briefing.GetPhase() == BriefingPhase::Inactive ||
            briefing.GetPhase() == BriefingPhase::Complete
        ) {
            return;
        }

        MiniGameRuntimeUi ui(context);
        if (!ui.IsAvailable()) {
            return;
        }

        const float panelWidth = (std::min)(
            820.0f,
            (std::max)(520.0f, ui.Width() - 48.0f)
        );
        const float panelX = (ui.Width() - panelWidth) * 0.5f;
        constexpr float topY = 22.0f;
        constexpr float topHeight = 122.0f;
        constexpr float bottomHeight = 70.0f;
        const float bottomY = ui.Height() - bottomHeight - 22.0f;

        // Game本体を完全に隠さず、説明用Emissive cubeだけを前景として読める濃度。
        ui.FillPanel(
            0.0f,
            0.0f,
            ui.Width(),
            ui.Height(),
            D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.24f)
        );
        ui.FillPanel(
            panelX,
            topY,
            panelWidth,
            topHeight,
            D2D1::ColorF(0.018f, 0.027f, 0.052f, 0.96f)
        );

        ui.DrawTextCentered(
            std::string("HOW TO PLAY  /  ") + std::string(gameTitle),
            ui.Width() * 0.5f,
            topY + 12.0f,
            22.0f,
            D2D1::ColorF(0.94f, 0.97f, 1.0f, 1.0f)
        );

        if (briefing.GetPhase() == BriefingPhase::Ready) {
            ui.DrawTextCentered(
                briefing.WasSkipped()
                    ? "説明をスキップしました"
                    : "操作確認 完了",
                ui.Width() * 0.5f,
                topY + 50.0f,
                25.0f,
                D2D1::ColorF(0.35f, 0.95f, 0.56f, 1.0f)
            );
            ui.DrawTextCentered(
                "READY  —  まもなくカウントダウン",
                ui.Width() * 0.5f,
                topY + 84.0f,
                17.0f,
                D2D1::ColorF(1.0f, 0.86f, 0.28f, 1.0f),
                false
            );
            return;
        }

        const std::size_t displayStep = (std::min)(
            briefing.GetCurrentStepIndex() + 1,
            briefing.GetStepCount()
        );
        ui.DrawTextCentered(
            "STEP " + std::to_string(displayStep) + " / " +
                std::to_string(briefing.GetStepCount()),
            ui.Width() * 0.5f,
            topY + 47.0f,
            14.0f,
            D2D1::ColorF(0.55f, 0.72f, 0.94f, 1.0f),
            false
        );
        ui.DrawTextCentered(
            briefing.GetCurrentPrompt(),
            ui.Width() * 0.5f,
            topY + 72.0f,
            22.0f,
            D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f)
        );

        // Skip UIは下端へ分離し、中央の3D体感デモ領域を覆わない。
        ui.FillPanel(
            panelX,
            bottomY,
            panelWidth,
            bottomHeight,
            D2D1::ColorF(0.014f, 0.022f, 0.044f, 0.92f)
        );
        const float barWidth = panelWidth - 96.0f;
        constexpr float barHeight = 12.0f;
        const float barX = panelX + 48.0f;
        const float barY = bottomY + 13.0f;
        ui.FillPanel(
            barX,
            barY,
            barWidth,
            barHeight,
            D2D1::ColorF(0.07f, 0.10f, 0.16f, 0.94f)
        );

        if (briefing.GetPhase() == BriefingPhase::AwaitingSkipRelease) {
            ui.DrawTextCentered(
                "ENTERを一度離すと、長押しスキップを使用できます",
                ui.Width() * 0.5f,
                bottomY + 37.0f,
                14.0f,
                D2D1::ColorF(0.72f, 0.80f, 0.92f, 1.0f),
                false
            );
            return;
        }

        const float skipProgress = briefing.GetSkipProgress();
        if (skipProgress > 0.0f) {
            ui.FillPanel(
                barX,
                barY,
                barWidth * skipProgress,
                barHeight,
                D2D1::ColorF(0.22f, 0.64f, 1.0f, 0.96f)
            );
        }
        ui.DrawTextCentered(
            "ENTER長押し：説明をスキップ",
            ui.Width() * 0.5f,
            bottomY + 37.0f,
            14.0f,
            skipProgress > 0.0f
                ? D2D1::ColorF(0.45f, 0.82f, 1.0f, 1.0f)
                : D2D1::ColorF(0.72f, 0.80f, 0.92f, 1.0f),
            false
        );
    }
};

} // namespace MiniGameCollection::Runtime
