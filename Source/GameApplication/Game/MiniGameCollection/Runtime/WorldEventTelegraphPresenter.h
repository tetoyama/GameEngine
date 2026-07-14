#pragma once

#include "Game/MiniGameCollection/Core/WorldEventTelegraphModel.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeUi.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace MiniGameCollection::Runtime {

class WorldEventTelegraphPresenter final {
public:
    static void Draw(
        SceneContext* context,
        const WorldEventTelegraphModel& model,
        float worldWidth = 24.0f,
        float worldHeight = 18.0f
    ) {
        MiniGameRuntimeUi ui(context);
        if (!ui.IsAvailable()) {
            return;
        }

        const std::vector<TelegraphSnapshot> snapshots =
            model.GetVisibleSnapshots();
        const TelegraphSnapshot* major = nullptr;
        std::size_t minorStatusRow = 0;

        for (const TelegraphSnapshot& snapshot : snapshots) {
            if (snapshot.definition.priority == TelegraphPriority::Major &&
                !major) {
                major = &snapshot;
            }
            DrawWorldMarker(ui, snapshot, worldWidth, worldHeight);
        }

        if (major) {
            DrawMajorBanner(ui, *major);
        }

        for (const TelegraphSnapshot& snapshot : snapshots) {
            if (snapshot.definition.priority == TelegraphPriority::Major) {
                continue;
            }
            DrawMinorStatus(ui, snapshot, minorStatusRow++);
        }
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
        const float width = (std::min)(720.0f, ui.Width() - 48.0f);
        const float x = (ui.Width() - width) * 0.5f;
        constexpr float y = 92.0f;
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

    static void DrawMinorStatus(
        const MiniGameRuntimeUi& ui,
        const TelegraphSnapshot& snapshot,
        std::size_t row
    ) {
        constexpr float width = 280.0f;
        constexpr float height = 44.0f;
        const float x = ui.Width() - width - 24.0f;
        const float y = 118.0f + static_cast<float>(row) * 52.0f;
        ui.FillPanel(
            x,
            y,
            width,
            height,
            D2D1::ColorF(0.02f, 0.03f, 0.055f, 0.88f)
        );
        ui.DrawText(
            snapshot.definition.label + "  " +
                TimeText(snapshot.phaseRemainingSeconds) + "s",
            x + 12.0f,
            y + 11.0f,
            15.0f,
            PhaseColor(snapshot.phase),
            false
        );
    }

    static void DrawWorldMarker(
        const MiniGameRuntimeUi& ui,
        const TelegraphSnapshot& snapshot,
        float worldWidth,
        float worldHeight
    ) {
        if (snapshot.definition.shape == TelegraphShape::Screen) {
            return;
        }

        const float normalizedX = snapshot.definition.worldPosition.x /
            (std::max)(1.0f, worldWidth);
        const float normalizedY = snapshot.definition.worldPosition.y /
            (std::max)(1.0f, worldHeight);
        const float centerX = ui.Width() * (0.5f + normalizedX);
        const float centerY = ui.Height() * (0.5f - normalizedY);
        const float pulse = 0.5f + 0.5f * std::sin(
            snapshot.phaseProgress * 18.8495559f
        );
        const float baseSize = snapshot.definition.shape == TelegraphShape::Area
            ? 86.0f
            : snapshot.definition.shape == TelegraphShape::Path
                ? 68.0f
                : 52.0f;
        const float size = baseSize + pulse * 14.0f;
        const D2D1::ColorF color = PhaseColor(snapshot.phase);

        ui.FillPanel(
            centerX - size * 0.5f,
            centerY - 3.0f,
            size,
            6.0f,
            D2D1::ColorF(color.r, color.g, color.b, 0.78f)
        );
        ui.FillPanel(
            centerX - 3.0f,
            centerY - size * 0.5f,
            6.0f,
            size,
            D2D1::ColorF(color.r, color.g, color.b, 0.78f)
        );
        ui.DrawTextCentered(
            snapshot.definition.label,
            centerX,
            centerY + size * 0.5f + 5.0f,
            14.0f,
            color,
            true
        );
    }
};

} // namespace MiniGameCollection::Runtime
