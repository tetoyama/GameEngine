#pragma once

#include <algorithm>

namespace MiniGameCollection {

struct MiniGameResponsiveViewport final {
    static constexpr float ReferenceWidth = 1280.0f;
    static constexpr float ReferenceHeight = 720.0f;
    static constexpr float MinimumScale = 0.01f;

    float physicalWidth = ReferenceWidth;
    float physicalHeight = ReferenceHeight;
    float scale = 1.0f;
    float logicalWidth = ReferenceWidth;
    float logicalHeight = ReferenceHeight;

    static constexpr MiniGameResponsiveViewport Build(
        float physicalWidth,
        float physicalHeight
    ) noexcept {
        const float safePhysicalWidth = physicalWidth > 1.0f
            ? physicalWidth
            : ReferenceWidth;
        const float safePhysicalHeight = physicalHeight > 1.0f
            ? physicalHeight
            : ReferenceHeight;
        const float resolvedScale = (std::max)(
            MinimumScale,
            (std::min)(
                safePhysicalWidth / ReferenceWidth,
                safePhysicalHeight / ReferenceHeight
            )
        );

        return {
            .physicalWidth = safePhysicalWidth,
            .physicalHeight = safePhysicalHeight,
            .scale = resolvedScale,
            .logicalWidth = safePhysicalWidth / resolvedScale,
            .logicalHeight = safePhysicalHeight / resolvedScale
        };
    }

    constexpr float SafeMarginX() const noexcept {
        return (std::clamp)(logicalWidth * 0.01875f, 20.0f, 48.0f);
    }

    constexpr float SafeMarginY() const noexcept {
        return (std::clamp)(logicalHeight * 0.025f, 18.0f, 42.0f);
    }

    constexpr float ResolvePanelWidth(
        float preferredWidth,
        float minimumWidth = 0.0f
    ) const noexcept {
        const float available = (std::max)(
            1.0f,
            logicalWidth - SafeMarginX() * 2.0f
        );
        const float clampedMinimum = (std::min)(
            (std::max)(0.0f, minimumWidth),
            available
        );
        return (std::clamp)(preferredWidth, clampedMinimum, available);
    }

    constexpr float CenteredX(float width) const noexcept {
        return (logicalWidth - width) * 0.5f;
    }

    constexpr bool IsPortrait() const noexcept {
        return logicalHeight > logicalWidth;
    }
};

constexpr bool MiniGameLayoutNearlyEqual(
    float lhs,
    float rhs,
    float epsilon = 0.01f
) noexcept {
    const float difference = lhs >= rhs ? lhs - rhs : rhs - lhs;
    return difference <= epsilon;
}

// 16:9は解像度にかかわらず同じ1280x720論理座標になる。
static_assert(MiniGameLayoutNearlyEqual(
    MiniGameResponsiveViewport::Build(1280.0f, 720.0f).scale,
    1.0f
));
static_assert(MiniGameLayoutNearlyEqual(
    MiniGameResponsiveViewport::Build(1920.0f, 1080.0f).logicalWidth,
    1280.0f
));
static_assert(MiniGameLayoutNearlyEqual(
    MiniGameResponsiveViewport::Build(2560.0f, 1440.0f).logicalHeight,
    720.0f
));

// 横長では高さを720に保ち、余剰横幅だけを安全な論理領域として公開する。
static_assert(
    MiniGameResponsiveViewport::Build(2560.0f, 1080.0f).logicalWidth > 1706.0f
);
static_assert(MiniGameLayoutNearlyEqual(
    MiniGameResponsiveViewport::Build(2560.0f, 1080.0f).logicalHeight,
    720.0f
));

// 4:3・縦長では幅を1280に保ち、余剰縦幅を公開してUIの重なりを防ぐ。
static_assert(MiniGameLayoutNearlyEqual(
    MiniGameResponsiveViewport::Build(1024.0f, 768.0f).logicalWidth,
    1280.0f
));
static_assert(
    MiniGameResponsiveViewport::Build(1024.0f, 768.0f).logicalHeight > 959.0f
);
static_assert(MiniGameResponsiveViewport::Build(720.0f, 1280.0f).IsPortrait());

} // namespace MiniGameCollection
