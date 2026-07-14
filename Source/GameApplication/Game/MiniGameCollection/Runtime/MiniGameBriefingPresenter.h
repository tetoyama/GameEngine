#pragma once

#include "Game/MiniGameCollection/Core/MiniGameBriefingModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeUi.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace MiniGameCollection::Runtime {

class MiniGameBriefingPresenter final {
public:
    static void Draw(
        SceneContext* context,
        std::string_view gameTitle,
        std::string_view objective,
        std::string_view controls,
        const std::array<std::string_view, 3>& tips,
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

        const float panelWidth = ui.ResolvePanelWidth(920.0f, 620.0f);
        const float panelX = ui.CenteredX(panelWidth);
        const float safeY = ui.SafeMarginY();
        const float topY = safeY;
        constexpr float topHeight = 184.0f;
        constexpr float bottomHeight = 72.0f;
        const float bottomY = ui.Height() - bottomHeight - safeY;

        // 実ゲームを見せたままルールカードと開始操作だけを重ねる。
        // MiniGameRuntimeUiのuniform-fit座標系により16:9、4:3、横長、縦長で
        // 同じ見かけの大きさを保ち、余剰領域へ安全にアンカーする。
        ui.FillPanel(
            0.0f,
            0.0f,
            ui.Width(),
            ui.Height(),
            D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.08f)
        );
        ui.FillPanel(
            panelX,
            topY,
            panelWidth,
            topHeight,
            D2D1::ColorF(0.018f, 0.027f, 0.052f, 0.94f)
        );

        ui.DrawText(
            "PRACTICE",
            panelX + 24.0f,
            topY + 13.0f,
            15.0f,
            D2D1::ColorF(0.36f, 0.78f, 1.0f, 1.0f),
            false
        );
        ui.DrawText(
            gameTitle,
            panelX + 24.0f,
            topY + 35.0f,
            28.0f,
            D2D1::ColorF(0.96f, 0.98f, 1.0f, 1.0f)
        );
        ui.DrawText(
            objective,
            panelX + 24.0f,
            topY + 72.0f,
            19.0f,
            D2D1::ColorF(1.0f, 0.88f, 0.34f, 1.0f)
        );
        ui.DrawText(
            controls,
            panelX + 24.0f,
            topY + 101.0f,
            16.0f,
            D2D1::ColorF(0.73f, 0.82f, 0.95f, 1.0f),
            false
        );

        for (std::size_t index = 0; index < tips.size(); ++index) {
            if (tips[index].empty()) {
                continue;
            }
            ui.DrawText(
                std::string("• ") + std::string(tips[index]),
                panelX + 28.0f,
                topY + 128.0f + static_cast<float>(index) * 18.0f,
                14.0f,
                D2D1::ColorF(0.83f, 0.88f, 0.96f, 1.0f),
                false
            );
        }

        ui.FillPanel(
            panelX,
            bottomY,
            panelWidth,
            bottomHeight,
            D2D1::ColorF(0.014f, 0.022f, 0.044f, 0.94f)
        );

        if (briefing.GetPhase() == BriefingPhase::Ready) {
            ui.DrawTextCentered(
                "本番用にゲームをリセットしています",
                ui.Width() * 0.5f,
                bottomY + 15.0f,
                20.0f,
                D2D1::ColorF(0.38f, 0.96f, 0.58f, 1.0f)
            );
            ui.DrawTextCentered(
                "練習中の得点・配置・アイテム状態は持ち越しません",
                ui.Width() * 0.5f,
                bottomY + 43.0f,
                13.0f,
                D2D1::ColorF(0.68f, 0.78f, 0.9f, 1.0f),
                false
            );
            return;
        }

        const float barWidth = panelWidth - 104.0f;
        constexpr float barHeight = 12.0f;
        const float barX = panelX + 52.0f;
        const float barY = bottomY + 12.0f;
        ui.FillPanel(
            barX,
            barY,
            barWidth,
            barHeight,
            D2D1::ColorF(0.07f, 0.10f, 0.16f, 0.96f)
        );

        if (briefing.GetPhase() == BriefingPhase::AwaitingSkipRelease) {
            ui.DrawTextCentered(
                "ENTERを一度離すと、本番開始を受け付けます",
                ui.Width() * 0.5f,
                bottomY + 38.0f,
                14.0f,
                D2D1::ColorF(0.74f, 0.82f, 0.94f, 1.0f),
                false
            );
            return;
        }

        const float startProgress = briefing.GetSkipProgress();
        if (startProgress > 0.0f) {
            ui.FillPanel(
                barX,
                barY,
                barWidth * startProgress,
                barHeight,
                D2D1::ColorF(0.22f, 0.68f, 1.0f, 0.98f)
            );
        }
        ui.DrawTextCentered(
            "自由に練習できます    ENTER長押し：本番をはじめる",
            ui.Width() * 0.5f,
            bottomY + 38.0f,
            14.0f,
            startProgress > 0.0f
                ? D2D1::ColorF(0.45f, 0.84f, 1.0f, 1.0f)
                : D2D1::ColorF(0.74f, 0.82f, 0.94f, 1.0f),
            false
        );
    }
};

} // namespace MiniGameCollection::Runtime
