// AudioSystem.h
#pragma once
#include "Interface/ISystem.h"
#include "GameApplication/Engine/Resources/resourceService.h"
#include "Engine/Graphics/GraphicsContext.h"
#include "Engine/Scene/scene.h"
#include "Engine/Scene/sceneManager.h"
#include "Engine/Scene/registry/ComponentRegistry.h"
#include "Component/EffectComponent.h"
#include "Engine/Audio/audioContext.h"
#include "Engine/Scene/component/transformComponent.h"


class EffectSystem : public ISystem {
public:
	EffectSystem(SceneContext* context)
		: m_context(context) {}

	~EffectSystem() {}

	void Initialize() override {
		m_audioContext = m_context->manager->audio;
	}

	void Finalize() override {
		auto entities = m_context->component->FindEntitiesWithComponent<EffectComponent>();
		for (auto entity : entities) {
			auto* comp = m_context->component->GetComponent<EffectComponent>(entity);
			if (comp) {
				comp->Stop();
			}
		}
	}

	void Start() override {
		auto entities = m_context->component->FindEntitiesWithComponent<EffectComponent>();
		for (auto entity : entities) {
			auto* comp = m_context->component->GetComponent<EffectComponent>(entity);
			if (!comp) continue;

			// AudioDataロードはSystem側でやる
			if (!comp->m_EffectData && !comp->FilePath.empty()) {
				comp->m_EffectData = m_context->manager->resource->Load<EffectData>(comp->FilePath);
			}

			if (comp->PlayOnStart && !comp->Playing) {
				comp->Play(m_context);
			}
		}
	}

	void Update(float dt) override {
		m_context->manager->graphics->GetEffectManager()->BeginUpdate();

		auto entities = m_context->component->FindEntitiesWithComponent<EffectComponent>();
		for (auto entity : entities) {
			auto* comp = m_context->component->GetComponent<EffectComponent>(entity);
			if (!comp) continue;

			if (comp->Playing) {
				if (!m_context->manager->graphics->GetEffectManager()->Exists(comp->m_Handle)) {
					comp->Playing = false;
					continue;
				}

				auto* transform = m_context->component->GetComponent<TransformComponent>(entity);
				if(transform){

					Vector3 m_Pos = transform->position;
					Vector3 m_Rot = transform->rotation;
					Vector3 m_Scale = transform->scale;

					Effekseer::Matrix43 Matrix, MatrixRot;
					MatrixRot.Indentity(); // 単位行列取得
					MatrixRot.RotationZXY(m_Rot.x, m_Rot.z, m_Rot.y + DirectX::XM_PI * 0.5f); // 回転行列生成

					Effekseer::Vector3D vec3Ds(m_Scale.x, m_Scale.y, m_Scale.z);
					Effekseer::Vector3D vec3Dt(m_Pos.x, m_Pos.y, m_Pos.z);

					Matrix.SetSRT(
						vec3Ds,
						MatrixRot,
						vec3Dt
					); // 拡大、回転、移動行列を作成

					m_context->manager->graphics->GetEffectManager()->SetBaseMatrix(comp->m_Handle, Matrix); // その行列をエフェクトに適用
				}

				m_context->manager->graphics->GetEffectManager()->UpdateHandle(comp->m_Handle, dt / 60.0f);
			} else{
				if(comp->Loop){
					comp->Play(m_context);
				}
			}
		}
		m_context->manager->graphics->GetEffectManager()->EndUpdate();

	}

	void FixedUpdate(float dt) override {}
	void Draw() override {

	}
	void EditorUpdate(float) override {}

private:
	SceneContext* m_context = nullptr;
	AudioContext* m_audioContext = nullptr;
};
