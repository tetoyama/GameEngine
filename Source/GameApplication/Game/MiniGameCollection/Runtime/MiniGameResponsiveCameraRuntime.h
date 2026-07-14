#pragma once

#include "Game/MiniGameCollection/Core/MiniGameResponsiveCamera.h"
#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeScriptBase.h"

#include "Scene/Component/cameraComponent.h"
#include "Scene/Component/entityNameComponent.h"
#include "Scene/Reference/ComponentRef.h"

#include <algorithm>
#include <cmath>

namespace MiniGameCollection::Runtime {

class MiniGameResponsiveCameraRuntime final : public MiniGameRuntimeScriptBase {
public:
    MiniGameResponsiveCameraRuntime()
        : MiniGameRuntimeScriptBase("MiniGameResponsiveCameraRuntime") {
        SetExecutionOrder(SystemTaskDomain::Frame, SystemPhase::Early, -300);
    }

    YAML::Node encode() override {
        YAML::Node node = MiniGameRuntimeScriptBase::encode();
        node["ReferenceAspect"] = m_settings.referenceAspect;
        node["ReferenceVerticalFov"] = m_settings.referenceVerticalFov;
        node["MaximumVerticalFov"] = m_settings.maximumVerticalFov;
        return node;
    }

    bool decode(SceneContext* context, const YAML::Node& node) override {
        MiniGameRuntimeScriptBase::decode(context, node);
        if (node["ReferenceAspect"]) {
            m_settings.referenceAspect = node["ReferenceAspect"].as<float>();
        }
        if (node["ReferenceVerticalFov"]) {
            m_settings.referenceVerticalFov =
                node["ReferenceVerticalFov"].as<float>();
        }
        if (node["MaximumVerticalFov"]) {
            m_settings.maximumVerticalFov =
                node["MaximumVerticalFov"].as<float>();
        }
        NormalizeSettings();
        return true;
    }

private:
    static constexpr float ResizeEpsilon = 0.5f;

    void OnStart() override {
        BindCamera();
        ApplyProjection(true);
    }

    void OnUpdate(float dt) override {
        (void)dt;
        if (!m_camera.IsValid()) {
            BindCamera();
        }
        ApplyProjection(false);
    }

    void OnFixedUpdate(float dt) override { (void)dt; }
    void OnDraw() override {}
    void OnEditorUpdate(float dt) override { (void)dt; }

    void OnStop() override {
        if (CameraComponent* camera = m_camera.TryGet()) {
            camera->FOV = m_settings.referenceVerticalFov;
        }
        m_camera = {};
        m_lastPhysicalWidth = -1.0f;
        m_lastPhysicalHeight = -1.0f;
    }

    void NormalizeSettings() noexcept {
        m_settings.referenceAspect = (std::max)(
            0.25f,
            m_settings.referenceAspect
        );
        m_settings.referenceVerticalFov = (std::clamp)(
            m_settings.referenceVerticalFov,
            0.1f,
            2.8f
        );
        m_settings.maximumVerticalFov = (std::clamp)(
            m_settings.maximumVerticalFov,
            m_settings.referenceVerticalFov,
            3.0f
        );
    }

    void BindCamera() {
        SceneContext* context = GetEntityRef().GetScene();
        if (!context || !context->component) {
            return;
        }

        const auto cameras =
            context->component->FindEntitiesWithComponent<CameraComponent>();
        if (cameras.empty()) {
            return;
        }

        Entity selected = cameras.front();
        for (Entity entity : cameras) {
            const NameComponent* name =
                context->component->GetComponent<NameComponent>(entity);
            if (name && name->name == "MiniGameCamera") {
                selected = entity;
                break;
            }
        }

        m_camera = ComponentRef<CameraComponent>(selected, context);
        if (CameraComponent* camera = m_camera.TryGet()) {
            // Scene側の値を基準として扱えるよう、明示値が不正な場合だけ補完する。
            if (m_settings.referenceVerticalFov <= 0.1f) {
                m_settings.referenceVerticalFov = camera->FOV;
            }
            NormalizeSettings();
        }
        m_lastPhysicalWidth = -1.0f;
        m_lastPhysicalHeight = -1.0f;
    }

    void ApplyProjection(bool force) {
        CameraComponent* camera = m_camera.TryGet();
        SceneContext* context = GetEntityRef().GetScene();
        if (!camera || !context || !context->manager) {
            return;
        }

        const float physicalWidth = (std::max)(
            1.0f,
            context->manager->PlayerScreenSize.x
        );
        const float physicalHeight = (std::max)(
            1.0f,
            context->manager->PlayerScreenSize.y
        );
        if (!force &&
            std::abs(physicalWidth - m_lastPhysicalWidth) <= ResizeEpsilon &&
            std::abs(physicalHeight - m_lastPhysicalHeight) <= ResizeEpsilon) {
            return;
        }

        const MiniGameResponsiveCameraProjection projection =
            MiniGameResponsiveCamera::Build(
                physicalWidth,
                physicalHeight,
                m_settings
            );
        camera->FOV = projection.verticalFov;
        m_lastPhysicalWidth = physicalWidth;
        m_lastPhysicalHeight = physicalHeight;
    }

    MiniGameResponsiveCameraSettings m_settings{};
    ComponentRef<CameraComponent> m_camera;
    float m_lastPhysicalWidth = -1.0f;
    float m_lastPhysicalHeight = -1.0f;
};

} // namespace MiniGameCollection::Runtime
