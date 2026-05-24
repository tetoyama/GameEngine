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
		: m_pContext(context){}

	void Initialize() override{
		m_pAudioContext = m_pContext->pAudio;
	}

	void Finalize() override{
		auto pManager = m_pContext->pGraphics->GetEffectManager();

		for(auto& [name, scene] : m_pContext->pSceneManager->GetActiveScenes()){
			auto pContext = scene->GetSceneContext();

			auto entities = pContext->pComponent->FindEntitiesWithComponent<EffectComponent>();
			for(auto entity : entities){
				if(auto* comp = context->pComponent->GetComponent<EffectComponent>(entity)){
					comp->Stop(context);
				}
			}
		}

		manager->StopAllEffects();
		manager->Update(0.0f);
	}

	void Start() override{
		for(auto& [name, scene] : m_pContext->pSceneManager->GetActiveScenes()){
			auto pContext = scene->GetSceneContext();
			auto entities = pContext->pComponent->FindEntitiesWithComponent<EffectComponent>();

			for(auto entity : entities){
				auto* comp = context->pComponent->GetComponent<EffectComponent>(entity);
				if(!comp) continue;

				if(!comp->m_EffectData && !comp->FilePath.empty()){
					comp->m_EffectData =
						m_pContext->pResource->Load<EffectData>(comp->FilePath);
				}

				if(comp->PlayOnStart){
					comp->Play(context);
				}
			}
		}
	}

	void Update(float dt) override{

		auto pManager = m_pContext->pGraphics->GetEffectManager();

		manager->BeginUpdate();

		for(auto& [name, scene] : m_pContext->pSceneManager->GetActiveScenes()){
			auto pContext = scene->GetSceneContext();
			auto entities = pContext->pComponent->FindEntitiesWithComponent<EffectComponent>();

			for(auto entity : entities){
				auto* comp = context->pComponent->GetComponent<EffectComponent>(entity);
				if(!comp) continue;

				// --------------------
				// 再生状態更新（時間・TimeScale・停止）
				// --------------------
				comp->Update(context, dt);
				
			}
		}

		manager->EndUpdate();
	}

	void EditorUpdate(float dt) override{
		auto pManager = m_pContext->pGraphics->GetEffectManager();

		manager->BeginUpdate();

		for(auto& [name, scene] : m_pContext->pSceneManager->GetActiveScenes()){
			auto pContext = scene->GetSceneContext();
			auto entities = pContext->pComponent->FindEntitiesWithComponent<EffectComponent>();

			for(auto entity : entities){
				auto* comp = context->pComponent->GetComponent<EffectComponent>(entity);
				if(!comp) continue;
				comp->Update(context, 0.0f);

				if(!comp->Playing)
					continue;

				if(!manager->Exists(comp->m_Handle)){
					comp->Playing = false;
					continue;
				}

				// --------------------
				// Transform 反映
				// --------------------
				if(auto* transform =
				   context->pComponent->GetComponent<TransformComponent>(entity)){

					const auto& pos = transform->position;
					const auto& scale = transform->scale;
					const auto rotQuat = transform->GetRotation();

					Effekseer::Matrix43 matRot;
					matRot.Indentity();

					DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&rotQuat);
					DirectX::XMMATRIX rotMat = DirectX::XMMatrixRotationQuaternion(q);

					DirectX::XMFLOAT4X4 rotF;
					DirectX::XMStoreFloat4x4(&rotF, rotMat);

					for(int i = 0; i < 3; ++i)
						for(int j = 0; j < 3; ++j)
							matRot.Value[i][j] = rotF.m[i][j];

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
	SceneManagerContext* m_pContext = nullptr;
	AudioContext* m_pAudioContext = nullptr;
};
