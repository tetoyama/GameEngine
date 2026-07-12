#pragma once

#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"

namespace MiniGameCollection::Runtime {

// MiniGameCollectionの唯一の起動Sceneに配置するRuntime。
// GameApplicationはEntry Sceneだけを開き、このRuntimeがPersistent Sceneを
// AdditiveロードすることでMulti-Scene構成を成立させる。
class MiniGameCollectionEntryRuntime final : public MiniGameRuntimeScriptBase {
public:
    MiniGameCollectionEntryRuntime()
        : MiniGameRuntimeScriptBase("MiniGameCollectionEntryRuntime") {
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Early, -300);
        SetExecutionOrder(SystemTaskDomain::Render, SystemPhase::Late, 300);
    }

private:
    static constexpr const char* PersistentScenePath =
        "Asset/Game/MiniGameCollection/Scene/Persistent/MiniGamePersistent.scene";

    void OnStart() override {
        QueuePersistentScene();
    }

    void OnUpdate(float dt) override {
        (void)dt;

        SceneContext* context = GetEntityRef().GetScene();
        if (!context || !context->manager || !context->manager->sceneManager) {
            return;
        }

        SceneManager& sceneManager = *context->manager->sceneManager;
        if (!m_loadRequested) {
            QueuePersistentScene();
        }

        if (!IsPersistentSceneLoaded(sceneManager)) {
            return;
        }

        // Persistent Sceneが起動した後はEntry Sceneを構成から外す。
        // Multi-Sceneの所有者はPersistent Runtimeへ引き継がれる。
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
            "LOADING...",
            ui.Width() * 0.5f,
            ui.Height() * 0.5f + 18.0f,
            18.0f,
            D2D1::ColorF(0.64f, 0.74f, 0.9f, 1.0f)
        );
    }

    void OnEditorUpdate(float dt) override {
        (void)dt;
    }

    void OnStop() override {
        m_loadRequested = false;
    }

    void QueuePersistentScene() {
        SceneContext* context = GetEntityRef().GetScene();
        if (!context || !context->manager || !context->manager->sceneManager) {
            return;
        }

        SceneManager& sceneManager = *context->manager->sceneManager;
        if (IsPersistentSceneLoaded(sceneManager)) {
            m_loadRequested = true;
            return;
        }

        m_loadRequested = sceneManager.QueueAdditiveSceneLoadFromFilePath(
            PersistentScenePath
        );
    }

    static bool IsPersistentSceneLoaded(const SceneManager& sceneManager) {
        for (const auto& [name, scene] : sceneManager.GetActiveScenes()) {
            (void)name;
            if (scene && scene->ScenePath == PersistentScenePath) {
                return true;
            }
        }
        return false;
    }

    bool m_loadRequested = false;
};

} // namespace MiniGameCollection::Runtime
