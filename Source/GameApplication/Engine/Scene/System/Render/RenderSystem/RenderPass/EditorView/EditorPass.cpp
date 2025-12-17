#include "EditorPass.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderPhase.h"

#include "../ShadowMap/ShadowMapPass.h"
#include "../Effekseer/EffectPass.h"

#include "scene.h"
#include "System/Render/RenderSystem/Renderable/IRenderable.h"
#include "System/Render/RenderSystem/RenderTarget/renderTarget.h"
#include "Component/transformComponent.h"
#include "Registry/componentRegistry.h"

#include "Shader/commonDefine.h"

#include "Graphics/graphicsContext.h"
#include "Registry/systemRegistry.h"

#include "Component/cameraComponent.h"
#include "Component/LightComponent.h"

#include "System/Render/RenderSystem/Renderable/Model/RenderableModel.h"
#include "System/Render/RenderSystem/Renderable/BillBoard/RenderableBillBoard.h"
#include "System/Render/RenderSystem/Renderable/Mesh/RenderableMesh.h"
#include "System/Render/RenderSystem/Renderable/Particle/RenderableParticle.h"
#include "System/Render/RenderSystem/Renderable/Sprite/RenderableSprite.h"
#include "System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.h"
#include "System/Render/RenderSystem/Renderable/Wave/RenderableWave.h"

#include <queue>
#include <Component/outlineComponent.h>
#include <Component/bumpMapComponent.h>
#include <PhysX/PxScene.h>
#include <System/Physic/physicSystem.h>
#include <Component/textureComponent.h>


#include "Backends/convertMatrix.h"


void EditorPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

	m_renderSystem = renderSystem;
	m_context = context;

	renderables.clear();
	renderables.push_back(renderSystem->GetRenderable<RenderableModel>());
	renderables.push_back(renderSystem->GetRenderable<RenderableBillBoard>());
	renderables.push_back(renderSystem->GetRenderable<RenderableMesh>());
	renderables.push_back(renderSystem->GetRenderable<RenderableParticle>());
	renderables.push_back(renderSystem->GetRenderable<RenderableSprite>());
	renderables.push_back(renderSystem->GetRenderable<RenderableTerrain>());
	//renderables.push_back(renderSystem->GetRenderable<RenderableWave>());

	playerRenderTarget = new RenderTarget(
		context->PlayerScreenSize,
		context->graphics,
		RenderTargetType::RENDERTARGET_TYPE_COLOR
	);

	shadowMapPass = new ShadowMapPass();
	shadowMapPass->Initialize(
		renderSystem,
		context
	);

	effectPass = new EffectPass();
	effectPass->Initialize(
		renderSystem,
		context
	);
}

void EditorPass::Finalize() {

	shadowMapPass->Finalize();
	delete shadowMapPass;
	shadowMapPass = nullptr;

	effectPass->Finalize();
	delete effectPass;
	effectPass = nullptr;

	delete playerRenderTarget;
	playerRenderTarget = nullptr;
}

void EditorPass::Execute(const RenderPassContext& ctx) {

	shadowMapPass->Execute(ctx);

	float clearCol[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	playerRenderTarget->Resize(ctx.screenSize, m_context->graphics);
	playerRenderTarget->Clear(m_context->graphics->GetDeviceContext(), clearCol);
	m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, playerRenderTarget->rtv.GetAddressOf(), playerRenderTarget->dsv.Get());

	m_context->graphics->GetDeviceContext()->PSSetShaderResources(TextureSlot_ShadowMap, 1, shadowMapPass->shadowRenderTarget->srv.GetAddressOf());
	m_context->graphics->GetDeviceContext()->PSSetSamplers(1, 1, &shadowMapPass->shadowSampler);

	ID3D11ShaderResourceView* initialSRV = playerRenderTarget->srv.Get();

	GraphicsContext* graphics = m_context->graphics;

	bool* pRenderLayer = ctx.renderLayerVisibility;

	// コンテキストの取得
	GraphicsContext* graphicsContext = m_context->renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	CAMERA camera{};
	camera.CameraPosition = ctx.cameraPosition;
	graphicsContext->SetCamera(camera);

	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);

	D3D11_VIEWPORT vp = {};
	vp.Width = ctx.screenSize.x;
	vp.Height = ctx.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	deviceContext->RSSetViewports(1, &vp);

	m_context->imgui->SetViewProjectionMatrix(ctx.viewMatrix, ctx.projectionMatrix);

	for (int i = 0; i < (int)RenderLayer::MaxRenderLayer; i++) {

		if (ctx.renderLayerVisibility[i] == false) {
			continue;
		}

		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();

			// コンポーネントを持つエンティティの検索
			std::vector<Entity> entities = context->component->FindEntitiesWithComponent<TransformComponent>();

			if (entities.empty()) {

				continue;

			} else {

				for (Entity entity : entities) {

					RenderLayer layer = scene->GetRenderLayerFromEntity(entity);

					if ((int)layer != i) {
						continue;
					}

					for (auto renderable : renderables) {
						renderable->Execute(ctx, context, entity);
					}
				}
			}
		}
	}
	//Effekseer
	effectPass->Execute(ctx);

	//PhysX

	std::vector<PostProcessNode> postNodes;
	std::unordered_map<int, int> effectIndexToPostNodeIndex; // camera->postEffects idx → postNodes idx

	DirectX::XMFLOAT4 clearColor = { 0,0,0,1 };

	CameraComponent* cameraComponent = ctx.cameraData.cameraComponent;

	if (cameraComponent) {
		auto sortedIndices = cameraComponent->TopologicalSortPostEffects();

		for (int idx : sortedIndices) {
			if (idx < 0) continue; // -1/-2 は描画対象としてノード作らない

			auto& e = cameraComponent->postEffects[idx];
			if (!e.enabled || !e.ps || !e.vs) continue;

			PostProcessNode node{};
			node.id = idx;
			node.shader.m_VS = e.vs->m_VertexShader;
			node.shader.m_PS = e.ps->m_PixelShader;
			node.shader.m_InputLayout = e.vs->m_VertexLayout;
			node.param = e.Param;

			e.ResizeTexture(graphics->GetDevice(), ctx.screenSize);
			e.Clear(graphics->GetDeviceContext(), &clearColor.x);
			node.rtv = e.rtv.GetAddressOf();
			node.srv = e.srv.Get();
			node.tex = e.tex.Get();

			// リンクから入力ノードを決定
			node.inputs.clear();
			// ノード追加前にインデックスを記録しておく
			int postNodeIndex = static_cast<int>(postNodes.size());
			effectIndexToPostNodeIndex[idx] = postNodeIndex;

			postNodes.push_back(std::move(node));
		}

		// リンクを後から追加（マッピングが揃った後に行う）
		for (auto& node : postNodes) {
			int effectIdx = node.id;
			for (auto& link : cameraComponent->postEffectLinks) {
				if (link.endNode == effectIdx) {
					if (link.startNode < 0) {
						node.inputs.push_back(-2); // 初期SRV
					} else {
						auto it = effectIndexToPostNodeIndex.find(link.startNode);
						if (it != effectIndexToPostNodeIndex.end()) {
							node.inputs.push_back(it->second);
						}
						// else: 無効な startNode は無視（スキップされたノードの可能性あり）
					}
				}
			}
		}
	}

	// 3. ApplyPostProcessChain 側で -1 を初期SRVに置き換えるようにする（念のための処理）
	if (!postNodes.empty()) {
		for (auto& node : postNodes) {
			for (size_t i = 0; i < node.inputs.size(); ++i) {
				if (node.inputs[i] == -1) {
					node.inputs[i] = -2; // 特殊値 -2 を初期SRV扱いとして ApplyPostProcessChain で処理
				}
			}
		}

		graphics->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
		graphics->ApplyPostProcessChain(postNodes, initialSRV);
		graphics->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);

		result = graphics->m_CurrentSRV;
	} else {
		result = initialSRV;
	}

	//ImGui::Image((ImTextureRef)result, ImVec2(ctx.screenSize.x, ctx.screenSize.y));
}

