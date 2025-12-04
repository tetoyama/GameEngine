#include "PlayerPass.h"
#include "System/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderPhase.h"

#include "../ShadowMap/ShadowMapPass.h"

#include "scene.h"
#include "System/RenderSystem/Renderable/IRenderable.h"
#include "System/RenderSystem/RenderTarget/renderTarget.h"
#include "Component/transformComponent.h"
#include "Registry/componentRegistry.h"

#include "Shader/commonDefine.h"

#include "GameApplication/Engine/Graphics/graphicsContext.h"
#include "Registry/systemRegistry.h"

#include "Component/cameraComponent.h"
#include "Component/LightComponent.h"

#include "System/RenderSystem/Renderable/Model/RenderableModel.h"
#include "System/RenderSystem/Renderable/BillBoard/RenderableBillBoard.h"
#include "System/RenderSystem/Renderable/Mesh/RenderableMesh.h"
#include "System/RenderSystem/Renderable/Particle/RenderableParticle.h"
#include "System/RenderSystem/Renderable/Sprite/RenderableSprite.h"
#include "System/RenderSystem/Renderable/Terrain/RenderableTerrain.h"
#include "System/RenderSystem/Renderable/Wave/RenderableWave.h"
#include <queue>
#include <Component/outlineComponent.h>
#include <Component/bumpMapComponent.h>
#include <PhysX/PxScene.h>
#include <System/physicSystem.h>
#include <Component/textureComponent.h>

constexpr int maxLineCount = 99999;

Effekseer::Matrix44 ConvertXMMATRIXToMatrix44(const DirectX::XMMATRIX& matrix) {
	Effekseer::Matrix44 result;
	DirectX::XMFLOAT4X4 tempMatrix;
	DirectX::XMStoreFloat4x4(&tempMatrix, matrix);

	//行列の各成分をEffekseerの行列にコピー
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			result.Values[row][col] = tempMatrix.m[row][col];
		}
	}
	return result;
}

void PlayerPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

	m_renderSystem = renderSystem;
	m_context = context;

	m_LineVertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\DebugLineVS.cso");
	m_LinePixelShader = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\DebugLinePS.cso");

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX_3D) * maxLineCount * 2;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = context->graphics->GetDevice()->CreateBuffer(&bd, nullptr, &pPhysicsDebugLineVB);
	if (FAILED(hr)) {
		throw std::runtime_error("Failed to create physics debug line vertex buffer.");
	}

	shadowMapPass = new ShadowMapPass();
	shadowMapPass->Initialize(
		renderSystem,
		context
	);

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
}

void PlayerPass::Finalize() {


	pPhysicsDebugLineVB->Release();
	pPhysicsDebugLineVB = nullptr;

	m_LinePixelShader.reset();
	m_LineVertexShader.reset();

	shadowMapPass->Finalize();
	delete shadowMapPass;
	shadowMapPass = nullptr;

	delete playerRenderTarget;
	playerRenderTarget = nullptr;
}

void PlayerPass::Execute(const RenderPassContext& ctx) {

	shadowMapPass->Execute(ctx);

	float clearCol[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	playerRenderTarget->Resize(ctx.screenSize, m_context->graphics);
	playerRenderTarget->Clear(m_context->graphics->GetDeviceContext(), clearCol);
	m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, playerRenderTarget->rtv.GetAddressOf(), playerRenderTarget->dsv.Get());

	m_context->graphics->GetDeviceContext()->PSSetShaderResources(2, 1, shadowMapPass->shadowRenderTarget->srv.GetAddressOf());
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
	Effekseer::Matrix44 effekseerProjectionMatrix = ConvertXMMATRIXToMatrix44(ctx.projectionMatrix);
	Effekseer::Matrix44 effekseerViewMatrix = ConvertXMMATRIXToMatrix44(ctx.viewMatrix);

	m_context->graphics->GetEffectRenderer()->SetProjectionMatrix(effekseerProjectionMatrix);
	m_context->graphics->GetEffectRenderer()->SetCameraMatrix(effekseerViewMatrix);

	m_context->graphics->GetEffectRenderer()->BeginRendering();
	m_context->graphics->GetEffectManager()->Draw();
	m_context->graphics->GetEffectRenderer()->EndRendering();

	//PhysX
	if (pRenderLayer[(int)RenderLayer::Debug] && ctx.passPhase != RenderPhase::PHASE_SHADOW) {

		for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
			auto context = scene->GetSceneContext();
			auto physics = m_context->systemRegistry->GetSystem<PhysicSystem>();
			const physx::PxRenderBuffer& rb = physics->GetRenderBuffer();

			// 色変換関数
			auto ConvertColor = [](physx::PxU32 c) {
				float a = ((c >> 24) & 0xFF) / 255.0f;
				float r = ((c >> 16) & 0xFF) / 255.0f;
				float g = ((c >> 8) & 0xFF) / 255.0f;
				float b = ((c >> 0) & 0xFF) / 255.0f;
				return DirectX::XMFLOAT4(r, g, b, a);
			};

			std::vector<VERTEX_3D> vertices;
			for (physx::PxU32 i = 0; i < rb.getNbLines(); i++) {
				const physx::PxDebugLine& line = rb.getLines()[i];

				VERTEX_3D v0;
				v0.Position = DirectX::XMFLOAT3(line.pos0.x, line.pos0.y, line.pos0.z);
				v0.Diffuse = ConvertColor(line.color0);

				VERTEX_3D v1;
				v1.Position = DirectX::XMFLOAT3(line.pos1.x, line.pos1.y, line.pos1.z);
				v1.Diffuse = ConvertColor(line.color1);

				vertices.push_back(v0);
				vertices.push_back(v1);
			}

			if (vertices.empty() || vertices.size() >= maxLineCount) continue;

			ID3D11Device* device = graphicsContext->GetDevice();
			ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

			// 頂点バッファ更新
			D3D11_MAPPED_SUBRESOURCE mapped;
			deviceContext->Map(pPhysicsDebugLineVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			memcpy(mapped.pData, vertices.data(), sizeof(VERTEX_3D) * vertices.size());
			deviceContext->Unmap(pPhysicsDebugLineVB, 0);

			graphicsContext->SetWorldMatrix(DirectX::XMMatrixIdentity());

			UINT stride = sizeof(VERTEX_3D);
			UINT offset = 0;
			deviceContext->IASetVertexBuffers(0, 1, &pPhysicsDebugLineVB, &stride, &offset);
			deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

			// シェーダーをセット（通常のカラー付き頂点用のものを使用）
			deviceContext->VSSetShader(m_LineVertexShader->m_VertexShader.Get(), nullptr, 0);
			deviceContext->PSSetShader(m_LinePixelShader->m_PixelShader.Get(), nullptr, 0);

			// 描画
			deviceContext->Draw(static_cast<UINT>(vertices.size()), 0);
		}
	}

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

	ImGui::Image((ImTextureRef)result, ImVec2(ctx.screenSize.x, ctx.screenSize.y));


	ImGui::Begin("Shadow Map View", nullptr, 0);
	ImVec2 avail = ImGui::GetContentRegionAvail(); // ウィンドウ内の利用可能サイズ
	float availMin;

	if (avail.x < avail.y) {
		availMin = avail.x;
	} else {
		availMin = avail.y;
	}
	ImGui::Image((ImTextureID)shadowMapPass->shadowRenderTarget->srv.Get(), ImVec2(availMin, availMin), ImVec2(0, 0), ImVec2(1, 1));
	ImGui::End();
}

