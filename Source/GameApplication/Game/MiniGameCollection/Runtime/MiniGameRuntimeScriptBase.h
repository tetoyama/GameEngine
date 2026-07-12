#pragma once

#include "Game/MiniGameCollection/Core/MiniGameMath.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeMailbox.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeUi.h"

#include "Scene/Component/CustomScriptComponent.h"
#include "Scene/Component/entityNameComponent.h"
#include "Scene/Component/materialComponent.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Component/transformComponent.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
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
                auto* renderer =
                    context.component->GetComponent<ModelRendererComponent>(entity);

                if (nameComponent) {
                    nameComponent->name = std::move(name);
                }
                if (transform) {
                    transform->position = position;
                    transform->scale = scale;
                }
                if (material) {
                    material->ShaderID = 0;
                    material->Material.BaseColor = color;
                    material->Material.Metallic = 0.05f;
                    material->Material.Roughness = 0.68f;
                    material->Material.MaterialFlags &=
                        ~MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
                }
                if (renderer) {
                    renderer->modelFilePath = "Asset\\Model\\cube.obj";
                    renderer->isBlender = false;
                    renderer->CreateModel(&context);
                }

                if (ready) {
                    ready({
                        .entity = EntityRef(entity, &context),
                        .transform = ComponentRef<TransformComponent>(entity, &context),
                        .material = ComponentRef<MaterialComponent>(entity, &context)
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

    static DirectX::XMFLOAT4 PlayerColor(PlayerId playerId, float alpha = 1.0f) {
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

        const float panelWidth = 780.0f;
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
    }

    void DrawScoreRow(const std::vector<int>& scores) const {
        MiniGameRuntimeUi ui(GetEntityRef().GetScene());
        if (!ui.IsAvailable() || scores.empty()) {
            return;
        }

        const float panelWidth = 600.0f;
        const float panelX = (ui.Width() - panelWidth) * 0.5f;
        ui.FillPanel(
            panelX,
            116.0f,
            panelWidth,
            48.0f,
            D2D1::ColorF(0.025f, 0.035f, 0.06f, 0.80f)
        );

        const float cellWidth = panelWidth / static_cast<float>(scores.size());
        for (std::size_t index = 0; index < scores.size(); ++index) {
            const DirectX::XMFLOAT4 playerColor = PlayerColor(
                static_cast<PlayerId>(index)
            );
            ui.DrawText(
                "P" + std::to_string(index + 1) + "  " +
                    std::to_string(scores[index]),
                panelX + cellWidth * static_cast<float>(index) + 22.0f,
                127.0f,
                21.0f,
                D2D1::ColorF(
                    playerColor.x,
                    playerColor.y,
                    playerColor.z,
                    1.0f
                )
            );
        }
    }

    void DrawResultPanel(
        const MiniGameResult& result,
        const char* retryText = "R: RETRY   B/BACKSPACE: SELECT   N: NEXT"
    ) const {
        MiniGameRuntimeUi ui(GetEntityRef().GetScene());
        if (!ui.IsAvailable()) {
            return;
        }

        const float panelWidth = 620.0f;
        const float panelHeight = 168.0f +
            static_cast<float>(result.players.size()) * 34.0f;
        const float panelX = (ui.Width() - panelWidth) * 0.5f;
        const float panelY = (ui.Height() - panelHeight) * 0.5f;
        ui.FillPanel(
            panelX,
            panelY,
            panelWidth,
            panelHeight,
            D2D1::ColorF(0.018f, 0.025f, 0.045f, 0.94f)
        );
        ui.DrawTextCentered("RESULT", ui.Width() * 0.5f, panelY + 18.0f, 34.0f);

        if (result.isTie) {
            ui.DrawTextCentered(
                "DRAW",
                ui.Width() * 0.5f,
                panelY + 60.0f,
                25.0f,
                D2D1::ColorF(1.0f, 0.86f, 0.28f, 1.0f)
            );
        } else if (!result.players.empty()) {
            ui.DrawTextCentered(
                "WINNER  P" + std::to_string(result.players.front().playerId + 1),
                ui.Width() * 0.5f,
                panelY + 60.0f,
                25.0f,
                D2D1::ColorF(1.0f, 0.86f, 0.28f, 1.0f)
            );
        }

        float rowY = panelY + 98.0f;
        for (const PlayerResult& player : result.players) {
            const DirectX::XMFLOAT4 playerColor = PlayerColor(player.playerId);
            std::string row = std::to_string(player.rank) + ".  P" +
                std::to_string(player.playerId + 1) + "   SCORE " +
                std::to_string(player.score);
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

        ui.DrawTextCentered(
            retryText,
            ui.Width() * 0.5f,
            panelY + panelHeight - 42.0f,
            16.0f,
            D2D1::ColorF(0.72f, 0.8f, 0.92f, 1.0f)
        );
    }
};

} // namespace MiniGameCollection::Runtime