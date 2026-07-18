#pragma once

#include "Game/MiniGameCollection/Core/MiniGameMath.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeMailbox.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeUi.h"

#include "Scene/Component/CustomScriptComponent.h"
#include "Scene/Component/entityNameComponent.h"
#include "Scene/Component/materialComponent.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Component/textureComponent.h"
#include "Scene/Component/transformComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MiniGameCollection::Runtime {

class MiniGameRuntimeScriptBase : public CustomScriptComponent {
public:
    explicit MiniGameRuntimeScriptBase(std::string scriptName)
        : CustomScriptComponent(std::move(scriptName)) {
    }

    YAML::Node encode() override {
        YAML::Node node;
        node["ScriptName"] = scriptName;
        return node;
    }

    bool decode(SceneContext* context, const YAML::Node& node) override {
        (void)context;
        if (node["ScriptName"]) {
            scriptName = node["ScriptName"].as<std::string>();
        }
        return true;
    }

    // Inspectorは開発者向けデバッグUIなのでImGuiの使用を許可する。
    void inspector(SceneContext* context) override {
        (void)context;
        ImGui::TextUnformatted(scriptName.c_str());
    }

protected:
    struct CubeVisualRefs {
        EntityRef entity;
        ComponentRef<TransformComponent> transform;
        ComponentRef<MaterialComponent> material;
    };

    enum class ResultMenuAction : std::uint8_t {
        Retry = 0,
        NextGame = 1,
        ReturnToTitle = 2
    };

    using CubeReadyCallback = std::function<void(const CubeVisualRefs&)>;

    bool QueueCube(
        std::string name,
        Vector3 position,
        Vector3 scale,
        DirectX::XMFLOAT4 color,
        CubeReadyCallback ready = {}
    ) {
        CommandEntity pending = QueueCreateEntity();
        if (!pending.IsPending() && !pending.IsExisting()) {
            return false;
        }

        QueueAddComponent<NameComponent>(pending);
        QueueAddComponent<TransformComponent>(pending);
        QueueAddComponent<MaterialComponent>(pending);
        QueueAddComponent<TextureComponent>(pending);
        QueueAddComponent<ModelRendererComponent>(pending);

        return QueueEntitySetup(
            pending,
            [
                name = std::move(name),
                position,
                scale,
                color,
                ready = std::move(ready)
            ](Entity entity, SceneContext& context) mutable {
                auto* nameComponent =
                    context.component->GetComponent<NameComponent>(entity);
                auto* transform =
                    context.component->GetComponent<TransformComponent>(entity);
                auto* material =
                    context.component->GetComponent<MaterialComponent>(entity);
                auto* texture =
                    context.component->GetComponent<TextureComponent>(entity);
                auto* renderer =
                    context.component->GetComponent<ModelRendererComponent>(entity);

                const std::string materialRole = name;
                if (nameComponent) {
                    nameComponent->name = std::move(name);
                }
                if (transform) {
                    transform->position = position;
                    transform->scale = scale;
                }
                if (material) {
                    ApplyCubeMaterialPreset(materialRole, color, *material);
                }
                if (texture && context.manager && context.manager->resource) {
                    texture->m_TextureData =
                        context.manager->resource->Load<TextureData>(
                            "Asset\\Texture\\white.tga"
                        );
                    texture->UV_Slice_X = 1.0f;
                    texture->UV_Slice_Y = 1.0f;
                    texture->AnimationNum = 0;
                }
                if (renderer) {
                    renderer->modelFilePath = "Asset\\Model\\cube.obj";
                    renderer->isBlender = false;
                    renderer->CreateModel(&context);
                }

                if (ready) {
                    ready({
                        .entity = EntityRef(entity, &context),
                        .transform = ComponentRef<TransformComponent>(
                            entity,
                            &context
                        ),
                        .material = ComponentRef<MaterialComponent>(
                            entity,
                            &context
                        )
                    });
                }
            }
        );
    }

    Vec2 ReadMovementInput() const noexcept {
        Vec2 input{};
        if (GetKey('A') || GetKey(VK_LEFT)) input.x -= 1.0f;
        if (GetKey('D') || GetKey(VK_RIGHT)) input.x += 1.0f;
        if (GetKey('W') || GetKey(VK_UP)) input.y += 1.0f;
        if (GetKey('S') || GetKey(VK_DOWN)) input.y -= 1.0f;
        return ClampLength(input, 1.0f);
    }

    // 旧ResultコードのR/N/B/Escape判定を、矢印＋Spaceの共通Menuへ変換する。
    // これにより各Game固有Runtimeを増やさず、Escape終了要求とも競合しない。
    bool GetKeyDown(int keyCode) const {
        const std::optional<ResultMenuAction> legacyAction =
            ResultMenuActionForLegacyKey(keyCode);
        if (!legacyAction) {
            return CustomScriptComponent::GetKeyDown(keyCode);
        }

        // 既存RuntimeはResult入力の先頭で必ずRを問い合わせる。
        // ここを1Frame分のMenu入力処理開始点として利用する。
        if (keyCode == 'R') {
            m_resultMenuInputProcessedThisFrame = false;
            m_confirmedResultMenuAction.reset();
        }

        ProcessResultMenuInput();
        if (!m_confirmedResultMenuAction ||
            *m_confirmedResultMenuAction != *legacyAction) {
            return false;
        }

        m_confirmedResultMenuAction.reset();
        m_resultMenuSelection = 0;
        return true;
    }

    bool IsReturnToSelectionPressed() const noexcept {
        return GetKeyDown('B') || GetKeyDown(VK_BACK);
    }

    SceneToken GetRuntimeSceneToken() const noexcept {
        SceneContext* scene = GetEntityRef().GetScene();
        return scene ? static_cast<SceneToken>(scene->contextID) : 0;
    }

    std::string GetRuntimeSceneName() const {
        SceneContext* context = GetEntityRef().GetScene();
        if (!context || !context->manager || !context->manager->sceneManager) {
            return {};
        }
        for (const auto& [name, scene] :
            context->manager->sceneManager->GetActiveScenes()) {
            if (scene && scene->GetSceneContext() == context) {
                return name;
            }
        }
        return {};
    }

    bool SubmitTransition(
        std::string targetScenePath,
        TransitionRequest reason,
        float presentationWaitSeconds = 0.35f
    ) {
        return MiniGameRuntimeMailbox::SubmitTransition({
            .sceneToken = GetRuntimeSceneToken(),
            .sourceSceneName = GetRuntimeSceneName(),
            .targetScenePath = std::move(targetScenePath),
            .reason = reason,
            .presentationWaitSeconds = presentationWaitSeconds
        });
    }

    void SubmitPresentation(
        RuntimePresentationCommandType type,
        Vec2 position = {},
        float intensity = 1.0f
    ) {
        MiniGameRuntimeMailbox::SubmitPresentation({
            .type = type,
            .sceneToken = GetRuntimeSceneToken(),
            .position = position,
            .intensity = intensity
        });
    }

    static DirectX::XMFLOAT4 PlayerColor(
        PlayerId playerId,
        float alpha = 1.0f
    ) {
        switch (playerId % 4) {
        case 0: return {0.18f, 0.62f, 1.0f, alpha};
        case 1: return {1.0f, 0.25f, 0.22f, alpha};
        case 2: return {0.98f, 0.78f, 0.16f, alpha};
        default: return {0.34f, 0.9f, 0.42f, alpha};
        }
    }

    void DrawScreenHeader(
        const char* title,
        const char* rule,
        const char* controls,
        float remainingSeconds
    ) const {
        MiniGameRuntimeUi ui(GetEntityRef().GetScene());
        if (!ui.IsAvailable()) {
            return;
        }

        const float panelWidth = std::min(
            780.0f,
            std::max(480.0f, ui.Width() - 36.0f)
        );
        const float panelX = (ui.Width() - panelWidth) * 0.5f;
        ui.FillPanel(panelX, 14.0f, panelWidth, 92.0f);
        ui.DrawText(title, panelX + 24.0f, 22.0f, 27.0f);

        std::ostringstream timer;
        timer << "TIME " << std::fixed << std::setprecision(1)
              << std::max(0.0f, remainingSeconds);
        ui.DrawText(
            timer.str(),
            panelX + panelWidth - 145.0f,
            25.0f,
            21.0f,
            D2D1::ColorF(1.0f, 0.86f, 0.28f, 1.0f)
        );
        ui.DrawText(rule, panelX + 24.0f, 56.0f, 18.0f);
        ui.DrawText(
            controls,
            panelX + 24.0f,
            80.0f,
            15.0f,
            D2D1::ColorF(0.68f, 0.76f, 0.88f, 1.0f)
        );

        DrawStrategyGuide(
            ui,
            title ? std::string_view(title) : std::string_view{}
        );
    }

    void DrawScoreRow(const std::vector<int>& scores) const {
        MiniGameRuntimeUi ui(GetEntityRef().GetScene());
        if (!ui.IsAvailable() || scores.empty()) {
            return;
        }

        const float panelWidth = std::min(
            680.0f,
            std::max(480.0f, ui.Width() - 52.0f)
        );
        const float panelX = (ui.Width() - panelWidth) * 0.5f;
        constexpr float panelY = 116.0f;
        constexpr float panelHeight = 58.0f;
        ui.FillPanel(
            panelX,
            panelY,
            panelWidth,
            panelHeight,
            D2D1::ColorF(0.025f, 0.035f, 0.06f, 0.84f)
        );

        const int highScore = *std::max_element(scores.begin(), scores.end());
        const std::size_t leaderCount = static_cast<std::size_t>(
            std::count(scores.begin(), scores.end(), highScore)
        );
        const float cellWidth =
            panelWidth / static_cast<float>(scores.size());

        for (std::size_t index = 0; index < scores.size(); ++index) {
            const DirectX::XMFLOAT4 playerColor = PlayerColor(
                static_cast<PlayerId>(index)
            );
            const bool isPlayer = index == 0;
            const bool isLeader = scores[index] == highScore;

            ui.FillPanel(
                panelX + cellWidth * static_cast<float>(index) + 3.0f,
                panelY + 3.0f,
                cellWidth - 6.0f,
                panelHeight - 6.0f,
                D2D1::ColorF(
                    playerColor.x,
                    playerColor.y,
                    playerColor.z,
                    isPlayer ? 0.24f : isLeader ? 0.15f : 0.055f
                )
            );

            std::string label = isPlayer
                ? "YOU P1  "
                : "CPU P" + std::to_string(index + 1) + "  ";
            label += std::to_string(scores[index]);
            ui.DrawText(
                label,
                panelX + cellWidth * static_cast<float>(index) + 12.0f,
                panelY + 25.0f,
                isPlayer ? 19.0f : 17.0f,
                D2D1::ColorF(
                    playerColor.x,
                    playerColor.y,
                    playerColor.z,
                    1.0f
                )
            );

            if (isLeader) {
                ui.DrawText(
                    leaderCount == 1 ? "LEAD" : "TIED LEAD",
                    panelX + cellWidth * static_cast<float>(index) + 12.0f,
                    panelY + 7.0f,
                    12.0f,
                    D2D1::ColorF(1.0f, 0.88f, 0.34f, 1.0f),
                    false
                );
            }
        }
    }

    void DrawResultPanel(
        const MiniGameResult& result,
        const char* legacyHelpText = nullptr
    ) const {
        (void)legacyHelpText;

        m_resultMenuInputProcessedThisFrame = false;
        m_confirmedResultMenuAction.reset();

        MiniGameRuntimeUi ui(GetEntityRef().GetScene());
        if (!ui.IsAvailable()) {
            return;
        }

        constexpr float panelWidth = 620.0f;
        constexpr float menuRowHeight = 42.0f;
        constexpr float menuGap = 8.0f;
        const float rankingHeight =
            static_cast<float>(result.players.size()) * 34.0f;
        const float panelHeight =
            176.0f + rankingHeight +
            menuRowHeight * 3.0f + menuGap * 2.0f + 50.0f;
        const float panelX = (ui.Width() - panelWidth) * 0.5f;
        const float panelY = (ui.Height() - panelHeight) * 0.5f;

        ui.FillPanel(
            panelX,
            panelY,
            panelWidth,
            panelHeight,
            D2D1::ColorF(0.018f, 0.025f, 0.045f, 0.96f)
        );
        ui.DrawTextCentered(
            "RESULT",
            ui.Width() * 0.5f,
            panelY + 18.0f,
            34.0f
        );

        if (result.isTie) {
            ui.DrawTextCentered(
                "DRAW",
                ui.Width() * 0.5f,
                panelY + 60.0f,
                25.0f,
                D2D1::ColorF(1.0f, 0.86f, 0.28f, 1.0f)
            );
        } else if (!result.players.empty()) {
            const bool playerWon = result.players.front().playerId == 0;
            ui.DrawTextCentered(
                playerWon
                    ? "YOU WIN"
                    : "WINNER  P" +
                        std::to_string(result.players.front().playerId + 1),
                ui.Width() * 0.5f,
                panelY + 60.0f,
                25.0f,
                playerWon
                    ? D2D1::ColorF(0.3f, 0.75f, 1.0f, 1.0f)
                    : D2D1::ColorF(1.0f, 0.86f, 0.28f, 1.0f)
            );
        }

        float rowY = panelY + 98.0f;
        for (const PlayerResult& player : result.players) {
            const DirectX::XMFLOAT4 playerColor =
                PlayerColor(player.playerId);
            std::string row = std::to_string(player.rank) + ".  ";
            row += player.playerId == 0
                ? "YOU P1"
                : "CPU P" + std::to_string(player.playerId + 1);
            row += "   SCORE " + std::to_string(player.score);
            if (player.eliminated) {
                row += "   OUT";
            }
            ui.DrawText(
                row,
                panelX + 78.0f,
                rowY,
                20.0f,
                D2D1::ColorF(
                    playerColor.x,
                    playerColor.y,
                    playerColor.z,
                    1.0f
                )
            );
            rowY += 34.0f;
        }

        const std::array<std::string_view, 3> labels{
            "もう一度",
            "次のゲーム",
            "タイトルに戻る"
        };
        float menuY = rowY + 14.0f;
        for (std::size_t index = 0; index < labels.size(); ++index) {
            const bool selected = index == m_resultMenuSelection;
            const float itemX = panelX + 92.0f;
            const float itemWidth = panelWidth - 184.0f;

            ui.FillPanel(
                itemX,
                menuY,
                itemWidth,
                menuRowHeight,
                selected
                    ? D2D1::ColorF(0.12f, 0.42f, 0.72f, 0.92f)
                    : D2D1::ColorF(0.035f, 0.055f, 0.09f, 0.80f)
            );
            ui.DrawText(
                selected ? ">" : " ",
                itemX + 18.0f,
                menuY + 9.0f,
                20.0f,
                selected
                    ? D2D1::ColorF(1.0f, 0.9f, 0.28f, 1.0f)
                    : D2D1::ColorF(0.5f, 0.58f, 0.7f, 1.0f),
                false
            );
            ui.DrawTextCentered(
                labels[index],
                ui.Width() * 0.5f,
                menuY + 8.0f,
                selected ? 21.0f : 19.0f,
                selected
                    ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f)
                    : D2D1::ColorF(0.70f, 0.78f, 0.90f, 1.0f),
                false
            );
            menuY += menuRowHeight + menuGap;
        }

        ui.DrawTextCentered(
            "矢印キー：選択    SPACE：決定",
            ui.Width() * 0.5f,
            panelY + panelHeight - 34.0f,
            15.0f,
            D2D1::ColorF(0.72f, 0.8f, 0.92f, 1.0f),
            false
        );
    }

private:
    static bool ContainsRole(
        std::string_view name,
        std::string_view token
    ) noexcept {
        return name.find(token) != std::string_view::npos;
    }

    static void ApplyCubeMaterialPreset(
        std::string_view role,
        DirectX::XMFLOAT4 color,
        MaterialComponent& material
    ) {
        material.ShaderID = 1;
        material.Material.BaseColor = color;
        material.Material.Metallic = 0.08f;
        material.Material.Roughness = 0.58f;
        material.Material.AO = 1.0f;
        material.Material.EmissiveColor = DirectX::XMFLOAT3(
            color.x * 0.08f,
            color.y * 0.08f,
            color.z * 0.08f
        );
        material.Material.EmissiveIntensity = 0.0f;
        material.Material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
        material.Material.MaterialFlags &=
            ~MATERIAL_FLAG_USE_ENVIRONMENT_MAP;

        if (ContainsRole(role, "Player_")) {
            material.Material.Metallic = 0.24f;
            material.Material.Roughness = 0.30f;
            material.Material.EmissiveColor = DirectX::XMFLOAT3(
                color.x * 0.58f,
                color.y * 0.58f,
                color.z * 0.58f
            );
            material.Material.EmissiveIntensity = 0.58f;
        } else if (
            ContainsRole(role, "Sheep_") ||
            ContainsRole(role, "SheepField")
        ) {
            material.Material.Metallic = 0.0f;
            material.Material.Roughness =
                ContainsRole(role, "SheepField") ? 0.86f : 0.96f;
        } else if (
            ContainsRole(role, "Tile_") ||
            ContainsRole(role, "TerritoryTile")
        ) {
            material.Material.Metallic = 0.10f;
            material.Material.Roughness = 0.44f;
            material.Material.EmissiveColor = DirectX::XMFLOAT3(
                color.x * 0.22f,
                color.y * 0.22f,
                color.z * 0.22f
            );
            material.Material.EmissiveIntensity = 0.12f;
        } else if (ContainsRole(role, "Pen")) {
            material.Material.Metallic = 0.16f;
            material.Material.Roughness = 0.32f;
            material.Material.EmissiveColor = DirectX::XMFLOAT3(
                color.x * 0.72f,
                color.y * 0.72f,
                color.z * 0.72f
            );
            material.Material.EmissiveIntensity = 0.85f;
        } else if (
            ContainsRole(role, "Wall") ||
            ContainsRole(role, "Obstacle")
        ) {
            material.Material.Metallic = 0.38f;
            material.Material.Roughness = 0.40f;
        } else if (
            ContainsRole(role, "Floor") ||
            ContainsRole(role, "Field")
        ) {
            material.Material.Metallic = 0.03f;
            material.Material.Roughness = 0.84f;
        } else if (
            ContainsRole(role, "ShotTracer") ||
            ContainsRole(role, "FallbackEffect")
        ) {
            material.Material.Metallic = 0.12f;
            material.Material.Roughness = 0.14f;
            material.Material.EmissiveColor = DirectX::XMFLOAT3(
                color.x,
                color.y,
                color.z
            );
            material.Material.EmissiveIntensity = 3.2f;
        }
    }

    static void DrawStrategyGuide(
        MiniGameRuntimeUi& ui,
        std::string_view title
    ) {
        std::string line1;
        std::string line2;

        if (title == "COLOR TERRITORY") {
            line1 =
                "灰色は+1点。敵色を奪うと、自分+1 / 相手-1で点差が2動く";
            line2 =
                "序盤は空地を広げ、終盤は首位の色を削る。"
                "自分の色を踏み直しても得点なし";
        } else if (title == "SHEEP ROUNDUP") {
            line1 =
                "羊は近いプレイヤーから反対方向へ逃げる。"
                "追うだけでは囲いへ入らない";
            line2 =
                "自分の囲いと羊を結ぶ線の反対側へ回り込み、"
                "横から来る敵の押しを崩す";
        } else if (title == "BACKSHOT") {
            line1 =
                "移動方向が照準。背面だけ即撃破、"
                "正面と側面からの弾は防御される";
            line2 =
                "射撃後は約0.82秒撃てない。障害物で射線を切り、"
                "相手が別方向を向いた瞬間を狙う";
        } else {
            return;
        }

        const float width = std::min(
            850.0f,
            std::max(520.0f, ui.Width() - 36.0f)
        );
        const float x = (ui.Width() - width) * 0.5f;
        const float y = std::max(184.0f, ui.Height() - 82.0f);
        ui.FillPanel(
            x,
            y,
            width,
            66.0f,
            D2D1::ColorF(0.018f, 0.027f, 0.048f, 0.82f)
        );
        ui.DrawText(
            "YOU = P1 / BLUE",
            x + 16.0f,
            y + 8.0f,
            14.0f,
            D2D1::ColorF(0.28f, 0.68f, 1.0f, 1.0f),
            false
        );
        ui.DrawText(
            line1,
            x + 150.0f,
            y + 8.0f,
            14.0f,
            D2D1::ColorF(0.92f, 0.95f, 1.0f, 1.0f),
            false
        );
        ui.DrawText(
            line2,
            x + 16.0f,
            y + 36.0f,
            13.0f,
            D2D1::ColorF(0.7f, 0.79f, 0.9f, 1.0f),
            false
        );
    }

    std::optional<ResultMenuAction> ResultMenuActionForLegacyKey(
        int keyCode
    ) const noexcept {
        if (keyCode == 'R') {
            return ResultMenuAction::Retry;
        }
        if (keyCode == 'N' || keyCode == VK_RETURN) {
            return ResultMenuAction::NextGame;
        }
        if (
            keyCode == 'B' ||
            keyCode == VK_BACK ||
            keyCode == VK_ESCAPE
        ) {
            return ResultMenuAction::ReturnToTitle;
        }
        return std::nullopt;
    }

    void ProcessResultMenuInput() const {
        if (m_resultMenuInputProcessedThisFrame) {
            return;
        }
        m_resultMenuInputProcessedThisFrame = true;

        int movement = 0;
        if (
            CustomScriptComponent::GetKeyDown(VK_UP) ||
            CustomScriptComponent::GetKeyDown(VK_LEFT)
        ) {
            movement = -1;
        } else if (
            CustomScriptComponent::GetKeyDown(VK_DOWN) ||
            CustomScriptComponent::GetKeyDown(VK_RIGHT)
        ) {
            movement = 1;
        }

        if (movement != 0) {
            const int next = std::clamp(
                static_cast<int>(m_resultMenuSelection) + movement,
                0,
                2
            );
            m_resultMenuSelection = static_cast<std::size_t>(next);
        }

        if (CustomScriptComponent::GetKeyDown(VK_SPACE)) {
            m_confirmedResultMenuAction =
                static_cast<ResultMenuAction>(m_resultMenuSelection);
        }
    }

    mutable std::size_t m_resultMenuSelection = 0;
    mutable bool m_resultMenuInputProcessedThisFrame = false;
    mutable std::optional<ResultMenuAction> m_confirmedResultMenuAction;
};

} // namespace MiniGameCollection::Runtime
