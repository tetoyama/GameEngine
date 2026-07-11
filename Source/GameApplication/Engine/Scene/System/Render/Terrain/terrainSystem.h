// =======================================================================
//
// terrainSystem.h
//
// =======================================================================
#pragma once
#include "Interface/ISystem.h"
#include "Resources/resourceService.h"
#include "Graphics/graphicsContext.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/registry/ComponentRegistry.h"
#include "Component/EffectComponent.h"
#include "Audio/audioContext.h"
#include "Scene/component/transformComponent.h"
#include <Component/terrainComponent.h>
#include <Component/ColliderComponent.h>
#include "System/Render/Terrain/TerrainMeshBuilder.h"
#include "System/Render/Terrain/TerrainMeshUpload.h"
#include "System/Render/Terrain/TerrainTaskRegistrar.h"
#include <cstdint>
#include <vector>


// 地形メッシュの生成・更新を管理するシステム
class TerrainSystem : public ISystem {
public:
	const char* GetSystemName() const override{
		return "TerrainSystem";
	}

	TerrainSystem(SceneManagerContext* context)
		: m_context(context) {}

	~TerrainSystem() {}

	void Initialize() override {
		m_graphicContext = m_context->graphics;
		// Step 17-D: 起動時同期CreateMeshは撤去。
		// 初回メッシュ生成はTask経路（Build=Earliest → Upload=Early）へ委ねる。
		// CurrentScale の初期値は -1 のため、最初のRenderフレームで
		// BuildTerrainMeshes が未生成として検出し再構築する。
		// 【挙動変更】初回メッシュは Initialize 完了時点ではなく、
		// 最初のRenderフレームのUploadタスク完了後に有効になる。
	}

	void Finalize() override {
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto entities = context->component->FindEntitiesWithComponent<TerrainComponent>();
			for (auto entity : entities) {
				auto* comp = context->component->GetComponent<TerrainComponent>(entity);
				if (comp && comp->meshRenderer) {

					comp->meshRenderer->mesh.m_IndexBuffer.Reset();
					comp->meshRenderer->mesh.m_VertexBuffer.Reset();
					comp->meshRenderer->mesh.m_PixelShader.Reset();
					comp->meshRenderer->mesh.m_VertexShader.Reset();
					comp->meshRenderer->mesh.m_VertexLayout.Reset();
					delete comp->meshRenderer->mesh.m_TextureData;

					delete comp->meshRenderer;
					comp->meshRenderer = nullptr;
				}
			}
		}
	}

	void RegisterTasks(SystemScheduleBuilder& builder) override {
		// Step 17-D: Terrainメッシュ生成を2段Task化。
		//   Build  : Render / Earliest / AnyWorker（純CPU staging生成）
		//   Upload : Render / Early    / MainThread（GPUバッファ確保）
		// 旧 LegacyExclusive 一体Task（Draw→CreateMesh）は撤去済み。
		TerrainTaskRegistrar::Register(*this, builder);
	}

	// -------------------------------------------------------------------
	// Build（AnyWorker / 純CPU）: staging頂点/インデックスを生成する。
	// 再構築トリガは旧CreateMeshと同じ「未生成 or Scale != CurrentScale」。
	// D3D11には一切触れない。
	// -------------------------------------------------------------------
	void BuildTerrainMeshes() {
		if (!m_context || !m_context->sceneManager) return;
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			if (!scene) continue;
			auto context = scene->GetSceneContext();
			if (!context || !context->component) continue;
			auto entities = context->component->FindEntitiesWithComponent<TerrainComponent>();
			for (auto entity : entities) {
				auto* comp = context->component->GetComponent<TerrainComponent>(entity);
				if (!comp) continue;

				// 再構築判定（未生成 or Scale変更。HeightMap編集時は
				// inspectorがCurrentScaleを0にするためここで検出される）。
				const bool needsRebuild =
					(!comp->meshRenderer) || (comp->Scale != comp->CurrentScale);
				if (!needsRebuild) {
					continue;
				}

				std::vector<VERTEX_3D> vertices;
				std::vector<std::uint32_t> indices;
				if (!TerrainMeshBuilder::Build(
						comp->Scale, comp->HeightMap, vertices, indices)) {
					comp->meshBuildReady = false;
					continue;
				}

				comp->stagingVertices = std::move(vertices);
				comp->stagingIndices = std::move(indices);
				comp->stagingSignature =
					TerrainMeshBuilder::ComputeSignature(comp->Scale, comp->HeightMap);
				comp->meshBuildReady = true;
			}
		}
	}

	// -------------------------------------------------------------------
	// Upload（MainThread / GPU）: meshBuildReadyなEntityのみGPU確保する。
	// meshRendererの確保/CreateBuffer/解放はMainThreadのみ。
	// Build後にScale/HeightMapが変わった場合はsignature照合で古いstagingを拒否。
	// -------------------------------------------------------------------
	void UploadTerrainMeshes() {
		if (!m_context || !m_context->sceneManager || !m_context->graphics) return;
		GraphicsContext* graphics = m_context->graphics;
		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			if (!scene) continue;
			auto context = scene->GetSceneContext();
			if (!context || !context->component) continue;
			auto entities = context->component->FindEntitiesWithComponent<TerrainComponent>();
			for (auto entity : entities) {
				auto* comp = context->component->GetComponent<TerrainComponent>(entity);
				if (!comp) continue;
				if (!comp->meshBuildReady) continue;

				// signature再照合（17-CのRevision照合と同型）。
				// Build後にScale/HeightMapが変化していたら古いstagingを破棄。
				if (comp->stagingSignature !=
					TerrainMeshBuilder::ComputeSignature(comp->Scale, comp->HeightMap)) {
					comp->meshBuildReady = false;
					comp->stagingVertices.clear();
					comp->stagingIndices.clear();
					continue;
				}

				// meshRenderer確保（MainThreadのみ）
				if (!comp->meshRenderer) {
					comp->meshRenderer = new MeshRendererComponent();
				}

				if (TerrainMeshUpload::Upload(
						*graphics,
						*comp->meshRenderer,
						comp->stagingVertices,
						comp->stagingIndices)) {
					comp->CurrentScale = comp->Scale;
					comp->meshBuildReady = false;
					comp->stagingVertices.clear();
					comp->stagingIndices.clear();

					auto* col = context->component->GetComponent<ColliderComponent>(entity);
					if (col) col->needsUpdate = true;
				}
			}
		}
	}

private:
	SceneManagerContext* m_context = nullptr;
	GraphicsContext* m_graphicContext = nullptr;
};
