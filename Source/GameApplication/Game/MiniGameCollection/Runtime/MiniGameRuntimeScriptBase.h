#pragma once

#include "Game/MiniGameCollection/Core/MiniGameMath.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeMailbox.h"

#include "Scene/Component/CustomScriptComponent.h"
#include "Scene/Component/entityNameComponent.h"
#include "Scene/Component/materialComponent.h"
#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Component/transformComponent.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

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
        if (!pending) {
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

    static void DrawScreenHeader(
        const char* title,
        const char* rule,
        const char* controls,
        float remainingSeconds
    ) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + 18.0f),
            ImGuiCond_Always,
            ImVec2(0.5f, 0.0f)
        );
        ImGui::SetNextWindowBgAlpha(0.78f);
        ImGui::Begin(
            "##MiniGameHeader",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoInputs
        );
        ImGui::TextUnformatted(title);
        ImGui::Separator();
        ImGui::TextUnformatted(rule);
        ImGui::TextUnformatted(controls);
        ImGui::SameLine();
        ImGui::Text("  TIME %.1f", std::max(0.0f, remainingSeconds));
        ImGui::End();
    }

    static void DrawScoreRow(const std::vector<int>& scores) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + 112.0f),
            ImGuiCond_Always,
            ImVec2(0.5f, 0.0f)
        );
        ImGui::SetNextWindowBgAlpha(0.72f);
        ImGui::Begin(
            "##MiniGameScores",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoInputs
        );
        for (std::size_t index = 0; index < scores.size(); ++index) {
            if (index > 0) ImGui::SameLine();
            ImGui::Text("P%zu: %d", index + 1, scores[index]);
        }
        ImGui::End();
    }

    static void DrawResultPanel(
        const MiniGameResult& result,
        const char* retryText = "R: RETRY   ESC: SELECT   N: NEXT"
    ) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            viewport->GetCenter(),
            ImGuiCond_Always,
            ImVec2(0.5f, 0.5f)
        );
        ImGui::SetNextWindowBgAlpha(0.92f);
        ImGui::Begin(
            "RESULT",
            nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings
        );
        if (result.isTie) {
            ImGui::TextUnformatted("DRAW");
        } else if (!result.players.empty()) {
            ImGui::Text("WINNER: P%d", result.players.front().playerId + 1);
        }
        ImGui::Separator();
        for (const PlayerResult& player : result.players) {
            ImGui::Text(
                "%u. P%d   SCORE %d%s",
                player.rank,
                player.playerId + 1,
                player.score,
                player.eliminated ? "   OUT" : ""
            );
        }
        ImGui::Separator();
        ImGui::TextUnformatted(retryText);
        ImGui::End();
    }
};

} // namespace MiniGameCollection::Runtime
