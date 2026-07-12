#pragma once

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Graphics/mainRenderer.h"

#include <Windows.h>

#include <algorithm>
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

    // Runtime UIは論理解像度でレイアウトし、実画面へ一括縮小する。
    // これにより高解像度PlayerViewでHUDが巨大化してゲーム領域を圧迫しない。
    float Width() const noexcept {
        return PhysicalWidth() / LayoutScale();
    }

    float Height() const noexcept {
        return PhysicalHeight() / LayoutScale();
    }

    void FillPanel(
        float x,
        float y,
        float width,
        float height,
        D2D1::ColorF color = D2D1::ColorF(0.025f, 0.035f, 0.06f, 0.88f)
    ) const {
        if (MainRenderer* renderer = Renderer()) {
            const float scale = LayoutScale();
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

        const float scale = LayoutScale();
        const float physicalX = x * scale;
        const float physicalY = y * scale;
        const float physicalFontSize = fontSize * scale;
        const std::wstring owned(text);
        if (shadow) {
            const float shadowOffset = std::max(1.0f, 2.0f * scale);
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
    MainRenderer* Renderer() const noexcept {
        return m_context && m_context->manager
            ? m_context->manager->renderer
            : nullptr;
    }

    float PhysicalWidth() const noexcept {
        return m_context && m_context->manager
            ? std::max(1.0f, m_context->manager->PlayerScreenSize.x)
            : 1280.0f;
    }

    float PhysicalHeight() const noexcept {
        return m_context && m_context->manager
            ? std::max(1.0f, m_context->manager->PlayerScreenSize.y)
            : 720.0f;
    }

    float LayoutScale() const noexcept {
        const float height = PhysicalHeight();
        if (height >= 900.0f) {
            return 0.78f;
        }
        if (height >= 720.0f) {
            return 0.84f;
        }
        return 0.90f;
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
