#pragma once

#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"

#include <string>
#include <vector>

namespace MiniGameCollection::Runtime {

// Entry Sceneに配置し、Scene資産が宣言したAdditiveScenePathsから
// Multi-Scene構成を組み立てるRuntime。
class MiniGameCollectionEntryRuntime final : public MiniGameRuntimeScriptBase {
public:
    MiniGameCollectionEntryRuntime()
        : MiniGameRuntimeScriptBase("MiniGameCollectionEntryRuntime") {
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Early, -300);
        SetExecutionOrder(SystemTaskDomain::Render, SystemPhase::Late, 300);
    }

    YAML::Node encode() override {
        YAML::Node node = MiniGameRuntimeScriptBase::encode();
        YAML::Node paths(YAML::NodeType::Sequence);
        for (const std::string& path : m_additiveScenePaths) {
            paths.push_back(path);
        }
        node["AdditiveScenePaths"] = paths;
        node["DestroyEntryAfterLoaded"] = m_destroyEntryAfterLoaded;
        return node;
    }

    bool decode(SceneContext* context, const YAML::Node& node) override {
        MiniGameRuntimeScriptBase::decode(context, node);

        m_additiveScenePaths.clear();
        if (const YAML::Node paths = node["AdditiveScenePaths"];
            paths && paths.IsSequence()) {
            for (const YAML::Node& path : paths) {
                const std::string value = path.as<std::string>();
                if (!value.empty()) {
                    m_additiveScenePaths.push_back(value);
                }
            }
        }
        if (node["DestroyEntryAfterLoaded"]) {
            m_destroyEntryAfterLoaded =
                node["DestroyEntryAfterLoaded"].as<bool>();
        }
        return true;
    }

private:
    void OnStart() override {
        m_loadRequested = QueueConfiguredScenes();
    }

    void OnUpdate(float dt) override {
        (void)dt;

        SceneContext* context = GetEntityRef().GetScene();
        if (!context || !context->manager || !context->manager->sceneManager) {
            return;
        }

        SceneManager& sceneManager = *context->manager->sceneManager;
        if (!m_loadRequested) {
            m_loadRequested = QueueConfiguredScenes();
        }

        if (!AreConfiguredScenesLoaded(sceneManager) ||
            !m_destroyEntryAfterLoaded) {
            return;
        }

        // 宣言したScene群が揃った後はEntry Sceneを構成から外す。
        for (const auto& [name, scene] : sceneManager.GetActiveScenes()) {
            (void)name;
            if (scene && scene->GetSceneContext() == context) {
                scene->isDestroy = true;
                break;
            }
        }
    }

    void OnFixedUpdate(float dt) override {
        (void)dt;
    }

    void OnDraw() override {
        MiniGameRuntimeUi ui(GetEntityRef().GetScene());
        if (!ui.IsAvailable()) {
            return;
        }

        ui.FillPanel(
            0.0f,
            0.0f,
            ui.Width(),
            ui.Height(),
            D2D1::ColorF(0.012f, 0.018f, 0.035f, 1.0f)
        );
        ui.DrawTextCentered(
            "MINI GAME COLLECTION",
            ui.Width() * 0.5f,
            ui.Height() * 0.5f - 34.0f,
            34.0f
        );
        ui.DrawTextCentered(
            m_additiveScenePaths.empty()
                ? "ENTRY SCENE HAS NO ADDITIVE SCENES"
                : "LOADING MULTI-SCENE COMPOSITION...",
            ui.Width() * 0.5f,
            ui.Height() * 0.5f + 18.0f,
            18.0f,
            m_additiveScenePaths.empty()
                ? D2D1::ColorF(1.0f, 0.32f, 0.28f, 1.0f)
                : D2D1::ColorF(0.64f, 0.74f, 0.9f, 1.0f)
        );
    }

    void OnEditorUpdate(float dt) override {
        (void)dt;
    }

    void OnStop() override {
        m_loadRequested = false;
    }

    bool QueueConfiguredScenes() {
        SceneContext* context = GetEntityRef().GetScene();
        if (!context || !context->manager || !context->manager->sceneManager ||
            m_additiveScenePaths.empty()) {
            return false;
        }

        SceneManager& sceneManager = *context->manager->sceneManager;
        bool allQueued = true;
        for (const std::string& path : m_additiveScenePaths) {
            if (IsSceneLoaded(sceneManager, path)) {
                continue;
            }
            allQueued = sceneManager.QueueAdditiveSceneLoadFromFilePath(path) &&
                allQueued;
        }
        return allQueued;
    }

    bool AreConfiguredScenesLoaded(const SceneManager& sceneManager) const {
        if (m_additiveScenePaths.empty()) {
            return false;
        }
        for (const std::string& path : m_additiveScenePaths) {
            if (!IsSceneLoaded(sceneManager, path)) {
                return false;
            }
        }
        return true;
    }

    static bool IsSceneLoaded(
        const SceneManager& sceneManager,
        const std::string& scenePath
    ) {
        for (const auto& [name, scene] : sceneManager.GetActiveScenes()) {
            (void)name;
            if (scene && scene->ScenePath == scenePath) {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> m_additiveScenePaths;
    bool m_destroyEntryAfterLoaded = true;
    bool m_loadRequested = false;
};

} // namespace MiniGameCollection::Runtime
