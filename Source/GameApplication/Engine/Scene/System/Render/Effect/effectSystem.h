// =======================================================================
// 
// effectSystem.h
// 
// =======================================================================
#pragma once
#include "Interface/ISystem.h"
#include "Resources/resourceService.h"
#include "Graphics/GraphicsContext.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/registry/ComponentRegistry.h"
#include "Component/EffectComponent.h"
#include "Audio/audioContext.h"
#include "Scene/component/transformComponent.h"

// Effekseerエフェクトの更新・管理を行うシステム
class EffectSystem: public ISystem {
public:

	const char* GetSystemName() const override{
		return "EffectSystem";
	}
	EffectSystem(SceneManagerContext* context)
		: m_context(context){}

	void Initialize() override{
		m_audioContext = m_context->audio;
	}

	void Finalize() override{
		StopAllEffects();
	}

	void Start() override{
		for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<EffectComponent>();

			for(auto entity : entities){
				auto* comp = context->component->GetComponent<EffectComponent>(entity);
				if(!comp) continue;

				if(!comp->m_EffectData && !comp->FilePath.empty()){
					comp->m_EffectData =
						m_context->resource->Load<EffectData>(comp->FilePath);
				}

				if(comp->PlayOnStart){
					comp->Play(context);
				}
			}
		}
	}

	// Scene停止時はTempLoadや未使用Resource解放より先に、
	// Effekseer側が保持するEffect / Texture参照を必ず切る。
	// これを行わないと、旧SceneのEffectが解放済みSRVを次のDrawで参照する。
	void Stop() override{
		StopAllEffects();
	}

	void RegisterTasks(SystemScheduleBuilder& builder) override{

		using EffectUpdateQuery = ECSQuery::ComponentQueryView<
			ECSQuery::Read<TransformComponent>,
			ECSQuery::Write<EffectComponent>
		>;

		builder.AddQueryTask<EffectUpdateQuery>(
			"EffectSystem.Runtime.Simulate",
			SystemTaskDomain::Frame,
			SystemPhase::Late,
			0,
			StructuralAccess::None,
			ThreadAffinity::AnyWorker,
			[this](const SystemTaskContext& context){
				Update(context.deltaTime);
			}
		);

		builder.AddQueryTask<EffectUpdateQuery>(
			"EffectSystem.Render.Commit",
			SystemTaskDomain::Render,
			SystemPhase::Early,
			0,
			StructuralAccess::None,
			ThreadAffinity::AnyWorker,
			[this](const SystemTaskContext& context){
				EditorUpdate(context.deltaTime);
			}
		);
	}

	void Update(float dt){

		auto manager = m_context->graphics->GetEffectManager();

		manager->BeginUpdate();

		for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
			auto context = scene->GetSceneContext();
			const auto& entities = context->component->FindEntitiesWithComponent<EffectComponent>();

			for(auto& entity : entities){
				auto* comp = context->component->GetComponent<EffectComponent>(entity);
				if(!comp) continue;
				comp->Update(context, dt);
			}
		}

		manager->EndUpdate();
	}

	void EditorUpdate(float dt){
		(void)dt;
		auto manager = m_context->graphics->GetEffectManager();

		manager->BeginUpdate();

		for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<EffectComponent>();

			for(auto entity : entities){
				auto* comp = context->component->GetComponent<EffectComponent>(entity);
				if(!comp) continue;
				comp->Update(context, 0.0f);

				if(!comp->Playing){
					continue;
				}

				if(!manager->Exists(comp->m_Handle)){
					comp->Playing = false;
					continue;
				}

				if(auto* transform = context->component->GetComponent<TransformComponent>(entity)){
					const auto& pos = transform->position;
					const auto& scale = transform->scale;
					const auto rotQuat = transform->GetRotation();

					Effekseer::Matrix43 matRot;
					matRot.Indentity();

					DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&rotQuat);
					DirectX::XMMATRIX rotMat = DirectX::XMMatrixRotationQuaternion(q);

					DirectX::XMFLOAT4X4 rotF;
					DirectX::XMStoreFloat4x4(&rotF, rotMat);

					for(int i = 0; i < 3; ++i){
						for(int j = 0; j < 3; ++j){
							matRot.Value[i][j] = rotF.m[i][j];
						}
					}

					Effekseer::Matrix43 mat;
					mat.SetSRT(
						Effekseer::Vector3D(scale.x, scale.y, scale.z),
						matRot,
						Effekseer::Vector3D(pos.x, pos.y, pos.z)
					);

					manager->SetBaseMatrix(comp->m_Handle, mat);
				}
			}
		}

		manager->EndUpdate();
	}

private:
	void StopAllEffects(){
		if(!m_context || !m_context->graphics || !m_context->sceneManager){
			return;
		}

		auto manager = m_context->graphics->GetEffectManager();
		if(!manager){
			return;
		}

		for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
			if(!scene) continue;
			auto context = scene->GetSceneContext();
			if(!context || !context->component) continue;

			auto entities = context->component->FindEntitiesWithComponent<EffectComponent>();
			for(auto entity : entities){
				if(auto* comp = context->component->GetComponent<EffectComponent>(entity)){
					comp->Stop(context);
					comp->m_EffectData.reset();
				}
			}
		}

		manager->StopAllEffects();
		// Stop要求をEffekseer内部へ反映し、Rendererが保持しているTexture/SRV参照を解放する。
		manager->Update(0.0f);
	}

private:
	SceneManagerContext* m_context = nullptr;
	AudioContext* m_audioContext = nullptr;
};
