#pragma once

#include "Game/MiniGameCollection/Core/WorldEventTelegraphModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeUi.h"

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace MiniGameCollection::Runtime {

// 2D側は画面全体イベントと補助的な状態表示だけを担当する。
// worldPositionを持つ危険範囲はWorldEventTelegraphRuntimeSupportが
// 実際の3D床面Geometryとして表示する。
class WorldEventTelegraphPresenter final {
public:
    static void DrawHud(
        SceneContext* context,
        const WorldEventTelegraphModel& model
    ) {
        MiniGameRuntimeUi ui(context);
        if (!ui.IsAvailable()) {
            return;
        }

        const std::vector<TelegraphSnapshot> snapshots =
            model.GetVisibleSnapshots();
        const TelegraphSnapshot* screenMajor = nullptr;
        std::size_t statusRow = 0;

        for (const TelegraphSnapshot& snapshot : snapshots) {
            if (snapshot.definition.shape == TelegraphShape::Screen &&
                snapshot.definition.priority == TelegraphPriority::Major &&
                !screenMajor) {
                screenMajor = &snapshot;
                continue;
            }

            // 位置は3D Markerで示す。HUDには名称と残り時間だけを端へ置く。
            DrawStatus(ui, snapshot, statusRow++);
        }

        if (screenMajor) {
            DrawMajorBanner(ui, *screenMajor);
        }
    }

    // 旧呼出しとの互換。worldWidth/worldHeightは疑似投影を廃止したため使用しない。
    static void Draw(
        SceneContext* context,
        const WorldEventTelegraphModel& model,
        float worldWidth = 24.0f,
        float worldHeight = 18.0f
    ) {
        (void)worldWidth;
        (void)worldHeight;
        DrawHud(context, model);
    }

private:
    static const char* PhaseName(TelegraphPhase phase) noexcept {
        switch (phase) {
        case TelegraphPhase::Warning: return "WARNING";
        case TelegraphPhase::Armed: return "ARMED";
        case TelegraphPhase::Resolving: return "NOW";
        case TelegraphPhase::Aftermath: return "RESULT";
        default: return "";
        }
    }

    static D2D1::ColorF PhaseColor(TelegraphPhase phase) noexcept {
        switch (phase) {
        case TelegraphPhase::Warning:
            return D2D1::ColorF(1.0f, 0.84f, 0.22f, 1.0f);
        case TelegraphPhase::Armed:
            return D2D1::ColorF(1.0f, 0.42f, 0.08f, 1.0f);
        case TelegraphPhase::Resolving:
            return D2D1::ColorF(1.0f, 0.18f, 0.08f, 1.0f);
        case TelegraphPhase::Aftermath:
            return D2D1::ColorF(0.35f, 0.94f, 0.56f, 1.0f);
        default:
            return D2D1::ColorF(0.8f, 0.85f, 0.95f, 1.0f);
        }
    }

    static std::string TimeText(float seconds) {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(seconds < 1.0f ? 1 : 0)
               << (std::max)(0.0f, seconds);
        return stream.str();
    }

    static void DrawMajorBanner(
        const MiniGameRuntimeUi& ui,
        const TelegraphSnapshot& snapshot
    ) {
        const float width = ui.ResolvePanelWidth(720.0f, 480.0f);
        const float x = ui.CenteredX(width);
        const float y = ui.SafeMarginY() + 92.0f;
        constexpr float height = 80.0f;
        const D2D1::ColorF color = PhaseColor(snapshot.phase);

        ui.FillPanel(
            x,
            y,
            width,
            height,
            D2D1::ColorF(0.015f, 0.022f, 0.042f, 0.94f)
        );
        ui.FillPanel(
            x,
            y + height - 8.0f,
            width * (std::clamp)(snapshot.phaseProgress, 0.0f, 1.0f),
            8.0f,
            color
        );
        ui.DrawTextCentered(
            std::string(PhaseName(snapshot.phase)) + "  /  " +
                snapshot.definition.label,
            ui.Width() * 0.5f,
            y + 14.0f,
            23.0f,
            color
        );
        ui.DrawTextCentered(
            TimeText(snapshot.phaseRemainingSeconds) + "s",
            ui.Width() * 0.5f,
            y + 45.0f,
            16.0f,
            D2D1::ColorF(0.94f, 0.97f, 1.0f, 1.0f),
            false
        );
    }

    static void DrawStatus(
        const MiniGameRuntimeUi& ui,
        const TelegraphSnapshot& snapshot,
        std::size_t row
    ) {
        const float width = ui.ResolvePanelWidth(292.0f, 220.0f);
        constexpr float height = 44.0f;
        const float x = ui.Width() - width - ui.SafeMarginX();
        const float y = ui.SafeMarginY() + 118.0f +
            static_cast<float>(row) * 52.0f;
        const D2D1::ColorF color = PhaseColor(snapshot.phase);

        ui.FillPanel(
            x,
            y,
            width,
            height,
            D2D1::ColorF(0.02f, 0.03f, 0.055f, 0.88f)
        );
        ui.FillPanel(
            x,
            y + height - 4.0f,
            width * (std::clamp)(snapshot.phaseProgress, 0.0f, 1.0f),
            4.0f,
            color
        );
        ui.DrawText(
            snapshot.definition.label + "  " +
                TimeText(snapshot.phaseRemainingSeconds) + "s",
            x + 12.0f,
            y + 10.0f,
            15.0f,
            color,
            false
        );
    }
};

} // namespace MiniGameCollection::Runtime
