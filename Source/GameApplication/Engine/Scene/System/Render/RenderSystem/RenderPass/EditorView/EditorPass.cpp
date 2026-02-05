#include "EditorPass.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderPhase.h"

#include "../ShadowMap/ShadowMapPass.h"
#include "../PhysXDebug/PhysXDebugPass.h"

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
#include <Component/materialComponent.h>
#include <System/Render/RenderSystem/Renderable/Effect/RenderableEffect.h>


void EditorPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

	m_renderSystem = renderSystem;
	m_context = context;

	m_RenderableVertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\commonVS.cso");

	renderables.clear();
	renderables.push_back(renderSystem->GetRenderable<RenderableModel>());
	renderables.push_back(renderSystem->GetRenderable<RenderableBillBoard>());
	renderables.push_back(renderSystem->GetRenderable<RenderableMesh>());
	renderables.push_back(renderSystem->GetRenderable<RenderableParticle>());
	renderables.push_back(renderSystem->GetRenderable<RenderableSprite>());
	renderables.push_back(renderSystem->GetRenderable<RenderableTerrain>());
	renderables.push_back(renderSystem->GetRenderable<RenderableWave>());
	renderables.push_back(renderSystem->GetRenderable<RenderableEffect>());

	editorRenderTarget = new RenderTarget(
		context->PlayerScreenSize,
		context->graphics,
		RenderTargetType::RENDERTARGET_TYPE_COLOR_UNORM
	);

	shadowMapPass = new ShadowMapPass();
	shadowMapPass->Initialize(
		renderSystem,
		context
	);

	physXDebugPass = new PhysXDebugPass();
	physXDebugPass->Initialize(
		renderSystem,
		context
	);
}

void EditorPass::Finalize() {

	m_RenderableVertexShader.reset();

	shadowMapPass->Finalize();
	delete shadowMapPass;
	shadowMapPass = nullptr;

	physXDebugPass->Finalize();
	delete physXDebugPass;
	physXDebugPass = nullptr;

	delete editorRenderTarget;
	editorRenderTarget = nullptr;
}

void EditorPass::Execute(const RenderPassContext& ctx) {

	// コンテキストの取得
	GraphicsContext* graphics = m_context->graphics;
	GraphicsContext* graphicsContext = m_context->renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	deviceContext->VSSetShader(m_RenderableVertexShader->m_VertexShader.Get(), nullptr, 0);
	deviceContext->IASetInputLayout(m_RenderableVertexShader->m_VertexLayout.Get());

	shadowMapPass->Execute(ctx);

	float clearCol[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	editorRenderTarget->Resize(ctx.screenSize, m_context->graphics);
	editorRenderTarget->Clear(m_context->graphics->GetDeviceContext(), clearCol);
	m_context->graphics->GetDeviceContext()->OMSetRenderTargets(1, editorRenderTarget->rtv.GetAddressOf(), editorRenderTarget->dsv.Get());

	m_context->graphics->GetDeviceContext()->PSSetShaderResources(TextureSlot_ShadowMap, 1, shadowMapPass->shadowRenderTarget->srv.GetAddressOf());
	m_context->graphics->GetDeviceContext()->PSSetSamplers(1, 1, &shadowMapPass->shadowSampler);

	ID3D11ShaderResourceView* initialSRV = editorRenderTarget->srv.Get();

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

	// シェーダーセット
	PixelShaderData* ps = m_renderSystem->GetForwardPS();
	deviceContext->PSSetShader(ps ? ps->m_PixelShader.Get() : nullptr, nullptr, 0);

	for(int i = 0; i < (int)RenderLayer::MaxRenderLayer; i++){

		if(!ctx.renderLayerVisibility[i]){
			continue;
		}
		// --- Transparent 用の一時バッファ ---
		std::vector<TransparentDrawItem> transparentList;

		for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){

			auto context = scene->GetSceneContext();
			std::vector<Entity> entities =
				context->component->FindEntitiesWithComponent<TransformComponent>();

			if(entities.empty()){
				continue;
			}

			for(Entity entity : entities){

				RenderLayer layer = scene->GetRenderLayerFromEntity(entity);
				if((int)layer != i){
					continue;
				}

				// SortTransparent3D の場合は一旦貯める
				if(layer == RenderLayer::SortTransparent3D){

					auto transform =
						context->component->GetComponent<TransformComponent>(entity);
					if(!transform){
						continue;
					}

					Vector3 worldPos = transform->GetWorldPosition(context->component);
					Vector3 diff = worldPos - Vector3(ctx.cameraPosition.x, ctx.cameraPosition.y, ctx.cameraPosition.z);

					TransparentDrawItem item;
					item.entity = entity;
					item.distanceSq = diff.dot(diff);
					item.context = context;
					transparentList.push_back(item);

				} else{
					// 通常描画（不透明など）
					for(auto renderable : renderables){

						int materialID = 0;
						auto material =
							context->component->GetComponent<MaterialComponent>(entity);
						if(material){
							materialID = material->ShaderID;
						}

						ObjectInfo info;
						info.SceneID = (unsigned int)context;
						info.ObjectID = entity;
						info.MaterialID = materialID;
						m_context->graphics->SetObjectInfo(info);

						renderable->Execute(ctx, context, entity);
					}
				}
			}

		}

		// --- Z ソートして描画 ---
		if(!transparentList.empty()){

			m_context->graphics->SetDepthMode(DepthMode::ReadOnly);

			std::sort(
				transparentList.begin(),
				transparentList.end(),
				[](const TransparentDrawItem& a,
				   const TransparentDrawItem& b){
					   return a.distanceSq > b.distanceSq; // 遠い→近い
				});

			for(auto& item : transparentList){

				Entity entity = item.entity;

				for(auto renderable : renderables){

					int materialID = 0;
					auto material =
						item.context->component->GetComponent<MaterialComponent>(entity);
					if(material){
						materialID = material->ShaderID;
					}

					ObjectInfo info;
					info.SceneID = (unsigned int)item.context;
					info.ObjectID = entity;
					info.MaterialID = materialID;
					m_context->graphics->SetObjectInfo(info);

					renderable->Execute(ctx, item.context, entity);
				}
			}

			m_context->graphics->SetDepthMode(DepthMode::Write);
		}
	}

	//PhysX
	physXDebugPass->Execute(ctx);

	result = initialSRV;
}

