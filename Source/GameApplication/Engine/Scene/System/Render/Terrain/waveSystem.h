// =======================================================================
//
// waveSystem.h
//
// =======================================================================
#pragma once

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "Interface/ISystem.h"
#include "Graphics/GraphicsContext.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/registry/ComponentRegistry.h"
#include "Component/meshRendererComponent.h"
#include "Component/waveComponent.h"
#include "WaveMeshBuilder.h"
#include "WaveMeshUpload.h"
#include "WaveTaskRegistrar.h"

// 波メッシュの生成・更新を管理するシステム
class WaveSystem: public ISystem {
public:
	const char* GetSystemName() const override{
		return "WaveSystem";
	}

	WaveSystem(SceneManagerContext* context)
		: m_context(context){}

	~WaveSystem(){}

	void Initialize() override{
		m_graphicContext = m_context ? m_context->graphics : nullptr;

		// 旧契約どおり、初回TopologyはInitialize中に同期生成する。
		// 毎Frameの波形頂点だけをSchedulerのBuild/Uploadへ分離する。
		InitializeWaveMeshes();
	}

	void Finalize() override{
		if(!m_context || !m_context->sceneManager) return;
		for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
			(void)name;
			if(!scene) continue;
			SceneContext* context = scene->GetSceneContext();
			if(!context || !context->component) continue;
			const auto entities =
				context->component->FindEntitiesWithComponent<WaveComponent>();
			for(Entity entity : entities){
				WaveComponent* component =
					context->component->GetComponent<WaveComponent>(entity);
				if(!component) continue;
				if(component->meshRenderer){
					component->meshRenderer->mesh.m_IndexBuffer.Reset();
					component->meshRenderer->mesh.m_VertexBuffer.Reset();
					delete component->meshRenderer;
					component->meshRenderer = nullptr;
				}
				ClearStaging(*component);
				component->CurrentResolution = -1;
			}
		}
	}

	void RegisterTasks(SystemScheduleBuilder& builder) override{
		WaveTaskRegistrar::Register(*this, builder);
	}

	// AnyWorker: D3D11へ触れず、次回Upload用の頂点列を生成する。
	void BuildWaveVertices(){
		if(!m_context || !m_context->sceneManager) return;
		for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
			(void)name;
			if(!scene) continue;
			SceneContext* context = scene->GetSceneContext();
			if(!context || !context->component) continue;
			const auto entities =
				context->component->FindEntitiesWithComponent<WaveComponent>();
			for(Entity entity : entities){
				WaveComponent* component =
					context->component->GetComponent<WaveComponent>(entity);
				if(!component) continue;

				const std::uint64_t inputSignature =
					WaveMeshBuilder::ComputeInputSignature(
						component->Resolution,
						component->Amplitude,
						component->Wavelength,
						component->Time
					);

				if(component->topologyBuildReady || component->vertexBuildReady){
					if(component->stagingSignature == inputSignature){
						// 前回Upload失敗時のstagingをそのまま再試行する。
						continue;
					}
					ClearStaging(*component);
				}

				const bool needsTopology =
					!component->meshRenderer ||
					component->Resolution != component->CurrentResolution;

				if(needsTopology){
					if(!WaveMeshBuilder::BuildTopology(
							component->Resolution,
							component->stagingVertices,
							component->stagingIndices) ||
						!WaveMeshBuilder::BuildAnimatedVertices(
							component->Resolution,
							component->Amplitude,
							component->Wavelength,
							component->Time,
							component->stagingVertices)){
						ClearStaging(*component);
						continue;
					}
					component->topologyBuildReady = true;
				}else{
					if(!WaveMeshBuilder::BuildAnimatedVertices(
							component->Resolution,
							component->Amplitude,
							component->Wavelength,
							component->Time,
							component->stagingVertices)){
						ClearStaging(*component);
						continue;
					}
					component->vertexBuildReady = true;
				}

				component->stagingSignature = inputSignature;
			}
		}
	}

	// MainThread: stagingをDYNAMIC Vertex Bufferへ反映する。
	void UploadWaveVertices(){
		if(!m_context || !m_context->sceneManager || !m_graphicContext) return;
		for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
			(void)name;
			if(!scene) continue;
			SceneContext* context = scene->GetSceneContext();
			if(!context || !context->component) continue;
			const auto entities =
				context->component->FindEntitiesWithComponent<WaveComponent>();
			for(Entity entity : entities){
				WaveComponent* component =
					context->component->GetComponent<WaveComponent>(entity);
				if(!component ||
					(!component->topologyBuildReady && !component->vertexBuildReady)){
					continue;
				}

				const std::uint64_t currentSignature =
					WaveMeshBuilder::ComputeInputSignature(
						component->Resolution,
						component->Amplitude,
						component->Wavelength,
						component->Time
					);
				if(component->stagingSignature != currentSignature){
					ClearStaging(*component);
					continue;
				}

				bool uploaded = false;
				if(component->topologyBuildReady){
					const bool allocatedRenderer = component->meshRenderer == nullptr;
					if(allocatedRenderer){
						component->meshRenderer = new MeshRendererComponent();
					}
					uploaded = WaveMeshUpload::ReplaceMesh(
						*m_graphicContext,
						*component->meshRenderer,
						component->stagingVertices,
						component->stagingIndices
					);
					if(uploaded){
						component->CurrentResolution = component->Resolution;
					}else if(allocatedRenderer){
						delete component->meshRenderer;
						component->meshRenderer = nullptr;
					}
				}else if(component->meshRenderer &&
					component->CurrentResolution == component->Resolution){
					uploaded = WaveMeshUpload::UploadVertices(
						*m_graphicContext,
						*component->meshRenderer,
						component->stagingVertices
					);
				}else{
					// Topologyが失われた場合は次Frameに再構築へ戻す。
					ClearStaging(*component);
					continue;
				}

				if(!uploaded){
					// stagingと旧GPUメッシュを維持し、次Frameに同じ入力で再試行する。
					continue;
				}

				if(std::isfinite(component->Speed)){
					component->Time += 0.02f * component->Speed;
				}
				ClearStaging(*component);
			}
		}
	}

	// 旧呼び出し互換。Scheduler外から呼ばれた場合も同一二段処理を行う。
	void Draw(float deltaTime){
		(void)deltaTime;
		BuildWaveVertices();
		UploadWaveVertices();
	}

private:
	static void ClearStaging(WaveComponent& component){
		component.stagingVertices.clear();
		component.stagingIndices.clear();
		component.stagingSignature = 0;
		component.topologyBuildReady = false;
		component.vertexBuildReady = false;
	}

	void InitializeWaveMeshes(){
		if(!m_context || !m_context->sceneManager || !m_graphicContext) return;
		for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
			(void)name;
			if(!scene) continue;
			SceneContext* context = scene->GetSceneContext();
			if(!context || !context->component) continue;
			const auto entities =
				context->component->FindEntitiesWithComponent<WaveComponent>();
			for(Entity entity : entities){
				WaveComponent* component =
					context->component->GetComponent<WaveComponent>(entity);
				if(!component) continue;

				std::vector<VERTEX_3D> vertices;
				std::vector<std::uint32_t> indices;
				if(!WaveMeshBuilder::BuildTopology(
						component->Resolution,
						vertices,
						indices)){
					continue;
				}

				const bool allocatedRenderer = component->meshRenderer == nullptr;
				if(allocatedRenderer){
					component->meshRenderer = new MeshRendererComponent();
				}
				if(WaveMeshUpload::ReplaceMesh(
						*m_graphicContext,
						*component->meshRenderer,
						vertices,
						indices)){
					component->CurrentResolution = component->Resolution;
				}else if(allocatedRenderer){
					delete component->meshRenderer;
					component->meshRenderer = nullptr;
				}
				ClearStaging(*component);
			}
		}
	}

	SceneManagerContext* m_context = nullptr;
	GraphicsContext* m_graphicContext = nullptr;
};
