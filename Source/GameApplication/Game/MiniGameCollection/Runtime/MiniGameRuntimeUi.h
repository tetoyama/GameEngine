#pragma once

#include "Game/MiniGameCollection/Core/MiniGameResponsiveLayout.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Graphics/mainRenderer.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace MiniGameCollection::Runtime {

class MiniGameRuntimeUi {
public:
    explicit MiniGameRuntimeUi(SceneContext* context)
        : m_context(context) {
    }

    bool IsAvailable() const noexcept {
        return Renderer() != nullptr;
    }

    // 1280x720を基準とするuniform-fit座標系。
    // 16:9では解像度にかかわらず同じ論理座標を返し、4:3・縦長・横長では
    // 余った方向だけ論理領域を拡張する。UIの縦横比を壊さず画面外も防ぐ。
    float Width() const noexcept {
        return Viewport().logicalWidth;
    }

    float Height() const noexcept {
        return Viewport().logicalHeight;
    }

    float Scale() const noexcept {
        return Viewport().scale;
    }

    float SafeMarginX() const noexcept {
        return Viewport().SafeMarginX();
    }

    float SafeMarginY() const noexcept {
        return Viewport().SafeMarginY();
    }

    float ResolvePanelWidth(
        float preferredWidth,
        float minimumWidth = 0.0f
    ) const noexcept {
        return Viewport().ResolvePanelWidth(preferredWidth, minimumWidth);
    }

    float CenteredX(float width) const noexcept {
        return Viewport().CenteredX(width);
    }

    bool IsPortrait() const noexcept {
        return Viewport().IsPortrait();
    }

    void FillPanel(
        float x,
        float y,
        float width,
        float height,
        D2D1::ColorF color = D2D1::ColorF(0.025f, 0.035f, 0.06f, 0.88f)
    ) const {
        if (MainRenderer* renderer = Renderer()) {
            const float scale = Scale();
            renderer->FillRect2D(
                x * scale,
                y * scale,
                width * scale,
                height * scale,
                color
            );
        }
    }

    void DrawText(
        std::string_view text,
        float x,
        float y,
        float fontSize,
        D2D1::ColorF color = D2D1::ColorF(0.94f, 0.96f, 1.0f, 1.0f),
        bool shadow = true
    ) const {
        DrawText(ToWide(text), x, y, fontSize, color, shadow);
    }

    void DrawText(
        std::wstring_view text,
        float x,
        float y,
        float fontSize,
        D2D1::ColorF color = D2D1::ColorF(0.94f, 0.96f, 1.0f, 1.0f),
        bool shadow = true
    ) const {
        MainRenderer* renderer = Renderer();
        if (!renderer || text.empty()) {
            return;
        }

        const float scale = Scale();
        const float physicalX = x * scale;
        const float physicalY = y * scale;
        const float physicalFontSize = fontSize * scale;
        const std::wstring owned(text);
        if (shadow) {
            const float shadowOffset = (std::max)(1.0f, 2.0f * scale);
            renderer->DrawText2D(
                owned,
                physicalX + shadowOffset,
                physicalY + shadowOffset,
                physicalFontSize,
                D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.82f)
            );
        }
        renderer->DrawText2D(
            owned,
            physicalX,
            physicalY,
            physicalFontSize,
            color
        );
    }

    void DrawTextCentered(
        std::string_view text,
        float centerX,
        float y,
        float fontSize,
        D2D1::ColorF color = D2D1::ColorF(0.94f, 0.96f, 1.0f, 1.0f),
        bool shadow = true
    ) const {
        const std::wstring wide = ToWide(text);
        DrawText(
            wide,
            centerX - EstimateWidth(wide, fontSize) * 0.5f,
            y,
            fontSize,
            color,
            shadow
        );
    }

    static std::wstring ToWide(std::string_view text) {
        if (text.empty()) {
            return {};
        }

        const int required = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0
        );
        if (required <= 0) {
            return std::wstring(text.begin(), text.end());
        }

        std::wstring result(static_cast<std::size_t>(required), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            required
        );
        return result;
    }

private:
    MiniGameResponsiveViewport Viewport() const noexcept {
        return MiniGameResponsiveViewport::Build(
            PhysicalWidth(),
            PhysicalHeight()
        );
    }

    MainRenderer* Renderer() const noexcept {
        return m_context && m_context->manager
            ? m_context->manager->renderer
            : nullptr;
    }

    float PhysicalWidth() const noexcept {
        const float width = m_context && m_context->manager
            ? m_context->manager->PlayerScreenSize.x
            : MiniGameResponsiveViewport::ReferenceWidth;
        return std::isfinite(width) && width > 1.0f
            ? width
            : MiniGameResponsiveViewport::ReferenceWidth;
    }

    float PhysicalHeight() const noexcept {
        const float height = m_context && m_context->manager
            ? m_context->manager->PlayerScreenSize.y
            : MiniGameResponsiveViewport::ReferenceHeight;
        return std::isfinite(height) && height > 1.0f
            ? height
            : MiniGameResponsiveViewport::ReferenceHeight;
    }

    static float EstimateWidth(std::wstring_view text, float fontSize) noexcept {
        float units = 0.0f;
        for (wchar_t character : text) {
            units += character <= 0x7f ? 0.58f : 1.0f;
        }
        return units * fontSize;
    }

    SceneContext* m_context = nullptr;
};

} // namespace MiniGameCollection::Runtime
