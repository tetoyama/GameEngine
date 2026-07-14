#pragma once

#include <algorithm>
#include <cmath>

namespace MiniGameCollection {

struct MiniGameResponsiveCameraSettings final {
    static constexpr float DefaultReferenceAspect = 16.0f / 9.0f;
    static constexpr float DefaultReferenceVerticalFov = 0.8f;
    static constexpr float DefaultMaximumVerticalFov = 2.09439510239f; // 120 degrees

    float referenceAspect = DefaultReferenceAspect;
    float referenceVerticalFov = DefaultReferenceVerticalFov;
    float maximumVerticalFov = DefaultMaximumVerticalFov;
};

struct MiniGameResponsiveCameraProjection final {
    float physicalAspect = MiniGameResponsiveCameraSettings::DefaultReferenceAspect;
    float coverageScale = 1.0f;
    float verticalFov = MiniGameResponsiveCameraSettings::DefaultReferenceVerticalFov;
};

class MiniGameResponsiveCamera final {
public:
    static constexpr float ResolveCoverageScale(
        float physicalAspect,
        float referenceAspect =
            MiniGameResponsiveCameraSettings::DefaultReferenceAspect
    ) noexcept {
        const float safeAspect = physicalAspect > 0.01f
            ? physicalAspect
            : referenceAspect;
        const float safeReferenceAspect = referenceAspect > 0.01f
            ? referenceAspect
            : MiniGameResponsiveCameraSettings::DefaultReferenceAspect;
        return safeAspect < safeReferenceAspect
            ? safeReferenceAspect / safeAspect
            : 1.0f;
    }

    static MiniGameResponsiveCameraProjection Build(
        float physicalWidth,
        float physicalHeight,
        MiniGameResponsiveCameraSettings settings = {}
    ) noexcept {
        const float safeWidth = (std::max)(1.0f, physicalWidth);
        const float safeHeight = (std::max)(1.0f, physicalHeight);
        const float safeReferenceAspect = settings.referenceAspect > 0.01f
            ? settings.referenceAspect
            : MiniGameResponsiveCameraSettings::DefaultReferenceAspect;
        const float safeReferenceFov = (std::clamp)(
            settings.referenceVerticalFov,
            0.1f,
            2.8f
        );
        const float safeMaximumFov = (std::clamp)(
            settings.maximumVerticalFov,
            safeReferenceFov,
            3.0f
        );
        const float physicalAspect = safeWidth / safeHeight;
        const float coverageScale = ResolveCoverageScale(
            physicalAspect,
            safeReferenceAspect
        );

        // XMMatrixPerspectiveFovLHのFOVは垂直FOV。
        // 狭い画面ではtan(vFov/2)をreferenceAspect / physicalAspect倍し、
        // 16:9時と同じ水平表示範囲を維持する。横長では基準FOVを保ち、
        // 余った横幅を自然に追加表示する。
        const float referenceHalfTangent = std::tan(safeReferenceFov * 0.5f);
        const float resolvedFov = 2.0f * std::atan(
            referenceHalfTangent * coverageScale
        );

        return {
            .physicalAspect = physicalAspect,
            .coverageScale = coverageScale,
            .verticalFov = (std::clamp)(
                resolvedFov,
                safeReferenceFov,
                safeMaximumFov
            )
        };
    }
};

static_assert(
    MiniGameResponsiveCamera::ResolveCoverageScale(16.0f / 9.0f) == 1.0f
);
static_assert(
    MiniGameResponsiveCamera::ResolveCoverageScale(21.0f / 9.0f) == 1.0f
);
static_assert(
    MiniGameResponsiveCamera::ResolveCoverageScale(4.0f / 3.0f) > 1.33f
);
static_assert(
    MiniGameResponsiveCamera::ResolveCoverageScale(9.0f / 16.0f) > 3.15f
);

} // namespace MiniGameCollection
