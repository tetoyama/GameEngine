// =======================================================================
// 
// ShadowMapPass.cpp
// 
// =======================================================================
#include "ShadowMapPass.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderPhase.h"

#include "scene.h"
#include "System/Render/RenderSystem/Renderable/IRenderable.h"
#include "System/Render/RenderSystem/RenderTarget/renderTarget.h"
#include "Component/transformComponent.h"
#include "Registry/componentRegistry.h"
#include "Resources/Data/pixelShaderData.h"

#include "Shader/commonDefine.h"

#include "Graphics/graphicsContext.h"
#include "Graphics/RHI/RHIService.h"
#include "Registry/systemRegistry.h"
#include "System/Render/Culling/ShadowRenderPacketCullingView.h"
#include "System/Render/RenderSystem/Renderable/Model/RenderableModel.h"
#include "System/Render/StaticBatch/StaticBatchShadowSubmission.h"
#include <Component/LightComponent.h>
#include <Component/cameraComponent.h>
#include <Component/environmentMapComponent.h>
#include <System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.h>
#include <System/Render/RenderSystem/Renderable/Wave/RenderableWave.h>
#include <System/Render/RenderSystem/Renderable/Particle/RenderableParticle.h>
#include <System/Render/RenderSystem/Renderable/BillBoard/RenderableBillBoard.h>

struct PointFace {
	DirectX::XMVECTOR dir;
	DirectX::XMVECTOR up;
};

static const PointFace s_PointFaces[6] = {
	{ { 1, 0, 0, 0}, { 0, 1, 0, 0 }}, // +X
	{ {-1, 0, 0, 0 },{ 0, 1, 0, 0 }}, // -X
	{ { 0, 1, 0, 0 },{ 0, 0, -1, 0}}, // +Y
	{ { 0,-1, 0, 0 },{ 0, 0, 1, 0 }}, // -Y
	{ { 0, 0, 1, 0 },{ 0, 1, 0, 0 }}, // +Z
	{ { 0, 0,-1, 0 },{ 0, 1, 0, 0 }}, // -Z
};


void ShadowMapPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {
	m_renderSystem = renderSystem;
	m_context = context;

	D3D11_SAMPLER_DESC desc = {};
	desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;

	context->graphics->GetDevice()->CreateSamplerState(&desc, &shadowSampler);

	shadowRenderTarget = new RenderTarget(
		Vector2(SHADOWMAP_SIZE, SHADOWMAP_SIZE),
		m_context->graphics,
		RenderTargetType::RENDERTARGET_TYPE_DEPTH
	);

	renderables.clear();
	renderables.push_back(renderSystem->GetRenderable<RenderableModel>());
	renderables.push_back(renderSystem->GetRenderable<RenderableTerrain>());
	//renderables.push_back(renderSystem->GetRenderable<RenderableWave>());
	//renderables.push_back(renderSystem->GetRenderable<RenderableParticle>());
	renderables.push_back(renderSystem->GetRenderable<RenderableBillBoard>());

	m_RenderablePixelShader = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\shadowPS.cso");

	// DepthClipEnable=FALSE のラスタライザステートを作成
	// フラスタム外の shadow caster が light-space Z < 0 でもクリップされず、
	// near plane にクランプされることで正しく影を落とせるようにする
	D3D11_RASTERIZER_DESC rsDesc = {};
	rsDesc.FillMode = D3D11_FILL_SOLID;
	rsDesc.CullMode = D3D11_CULL_BACK;
	rsDesc.DepthClipEnable = FALSE;
	rsDesc.MultisampleEnable = FALSE;
	HRESULT hr = context->graphics->GetDevice()->CreateRasterizerState(&rsDesc, &depthClampRS);
	assert(SUCCEEDED(hr));

}

void ShadowMapPass::Finalize() {
	m_RenderablePixelShader.reset();

	shadowSampler->Release();
	shadowSampler = nullptr;

	if (depthClampRS){
		depthClampRS->Release();
		depthClampRS = nullptr;
	}

	delete shadowRenderTarget;
	shadowRenderTarget = nullptr;
}
void ShadowMapPass::Execute(const RenderPassContext& ctx){

	using namespace DirectX;

	ID3D11DeviceContext* deviceContext = m_context->graphics->GetDeviceContext();
	GraphicsContext* graphicsContext = m_context->renderer->GetGraphicsContext();

	ID3D11ShaderResourceView* nullSRV[1] = {nullptr};
	deviceContext->PSSetShaderResources(TextureSlot_ShadowMap, 1, nullSRV);
	deviceContext->PSSetShader(m_RenderablePixelShader->m_PixelShader.Get(), NULL, 0);
	deviceContext->PSSetSamplers(1, 1, &shadowSampler);

	// ======== RenderTarget 切り替え ========
	if(shadowRenderTarget->type == RenderTargetType::RENDERTARGET_TYPE_DEPTH){
		deviceContext->ClearDepthStencilView(
			shadowRenderTarget->dsv.Get(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,
			0
		);
		deviceContext->OMSetRenderTargets(0, nullptr, shadowRenderTarget->dsv.Get());
	} else{
		float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		deviceContext->ClearRenderTargetView(shadowRenderTarget->rtv.Get(), color);
		deviceContext->ClearDepthStencilView(
			shadowRenderTarget->dsv.Get(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,
			0
		);
		deviceContext->OMSetRenderTargets(1, shadowRenderTarget->rtv.GetAddressOf(), shadowRenderTarget->dsv.Get());
	}

	// DepthClipEnable=FALSE で shadow caster をレンダリング
	deviceContext->RSSetState(depthClampRS);

	RenderPassContext newContext = ctx;
	newContext.passPhase = RenderPhase::PHASE_SHADOW;
	newContext.screenSize = Vector2(SHADOWMAP_SIZE, SHADOWMAP_SIZE);

	// ======== メインカメラ取得 ========
	Vector3 mainCamPos = Vector3(ctx.CameraPosition.x, ctx.CameraPosition.y, ctx.CameraPosition.z);
	Vector3 mainCamFront = ctx.cameraData.transformComponent->front();

	// ======== Directional Light 取得 ========
	LightBuffer light;

	for(int i = 0; i < LIGHT_MAX_COUNT; i++){
		light.Lights[i].Enable = false;
	}
	int lightCount = 0;
	int shadowCount = 0;

	bool hasCsmLight = false;
	bool foundLight = false;

	const auto& activeScenes = m_context->sceneManager->GetActiveScenes();
	const int maxLayer = (int)RenderLayer::MaxRenderLayer;

	for(const auto& [name, scene] : activeScenes){
		if(lightCount >= LIGHT_MAX_COUNT){
			break;
		}

		auto sctx = scene->GetSceneContext();
		const auto& lightEntities = sctx->component->FindEntitiesWithComponent<LightComponent>();
		if(lightEntities.empty()){
			continue;
		}

		for(Entity ent : lightEntities){
			if(lightCount >= LIGHT_MAX_COUNT){
				break;
			}

			LightComponent* lightcomp = sctx->component->GetComponent<LightComponent>(ent);
			if(!lightcomp) continue;
			if(!lightcomp->light.Enable) continue;
			if(!lightcomp->light.CastShadow) continue;

			if(lightcomp->light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM || lightcomp->light.LightType == LIGHT_TYPE_DIRECTIONAL){
				lightcomp->dirty = true;
			}

			TransformComponent* transform = sctx->component->GetComponent<TransformComponent>(ent);
			if(!transform) continue;

			LIGHT& lightData = lightcomp->light;
			lightData.Position = DirectX::XMFLOAT4(transform->position.x, transform->position.y, transform->position.z, 0.0f);
			lightData.Direction = DirectX::XMFLOAT4(transform->front().x, transform->front().y, transform->front().z, 0.0f);
			lightData.Dummy = 0;

			// ======== Directional Shadow ========
			if(lightData.LightType == LIGHT_TYPE_DIRECTIONAL){
				float shadowSize = lightData.Param.x / 10.0f;
				if(shadowSize <= 50.0f){
					shadowSize = 50.0f;
				}

				XMVECTOR camPos = mainCamPos.ToXMVECTOR();
				XMVECTOR ld = transform->front().ToXMVECTOR();
				XMVECTOR center = camPos;
				float dist = shadowSize;

				XMVECTOR eyev = center - ld * dist;
				XMVECTOR upv = XMVectorSet(0, 1, 0, 0);

				XMMATRIX lightView = XMMatrixLookAtLH(eyev, center, upv);
				XMMATRIX lightProj = XMMatrixOrthographicLH(shadowSize, shadowSize, 0.1f, dist * 2.0f);

				XMFLOAT3 lightCamPos;
				XMStoreFloat3(&lightCamPos, eyev);

				newContext.CameraPosition = XMFLOAT4(lightCamPos.x, lightCamPos.y, lightCamPos.z, 0);
				newContext.viewMatrix = lightView;
				newContext.projectionMatrix = lightProj;
				lightData.Position = newContext.CameraPosition;

				XMStoreFloat4x4(&lightData.LightView, XMMatrixTranspose(newContext.viewMatrix));
				XMStoreFloat4x4(&lightData.LightProjection, XMMatrixTranspose(newContext.projectionMatrix));

				light.Lights[lightCount] = lightData;
				lightCount++;
				shadowCount++;
				foundLight = true;
				continue;
			}

			// ======== CSM ========
			if(lightData.LightType == LIGHT_TYPE_DIRECTIONAL_CSM && !hasCsmLight
			   && ctx.cameraData.cameraComponent && ctx.cameraData.cameraComponent->FarClip > ctx.cameraData.cameraComponent->NearClip){

				XMVECTOR lightDir = XMVector3Normalize(transform->front().ToXMVECTOR());

				float cameraNear = ctx.cameraData.cameraComponent->NearClip;
				float cameraFar = ctx.cameraData.cameraComponent->FarClip;
				float fov = ctx.cameraData.cameraComponent->FOV;
				float aspect = (ctx.screenSize.y > 0.0f) ? (ctx.screenSize.x / ctx.screenSize.y) : 1.0f;
				float tanHalfFovV = tanf(fov * 0.5f);
				float tanHalfFovH = tanHalfFovV * aspect;

				XMMATRIX invCameraView = XMMatrixInverse(nullptr, ctx.viewMatrix);

				const float csmNear = max(cameraNear, 0.1f);
				const float csmFar = cameraFar;
				constexpr float csmLambda = 0.85f;

				float splitDepths[DIRECTIONAL_CSM_CASCADE_COUNT];
				for(int c = 0; c < DIRECTIONAL_CSM_CASCADE_COUNT; c++){
					float p = (float)(c + 1) / (float)DIRECTIONAL_CSM_CASCADE_COUNT;
					float logSplit = csmNear * powf(csmFar / csmNear, p);
					float uniSplit = csmNear + (csmFar - csmNear) * p;
					splitDepths[c] = csmLambda * logSplit + (1.0f - csmLambda) * uniSplit;
				}

				XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
				if(fabsf(XMVectorGetX(XMVector3Dot(lightDir, worldUp))) > 0.99f){
					worldUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
				}

				float prevSplit = csmNear;
				int firstCascadeSlot = lightCount;
				int addedCascades = 0;

				for(int c = 0; c < DIRECTIONAL_CSM_CASCADE_COUNT; c++){
					if(lightCount >= LIGHT_MAX_COUNT){
						break;
					}

					float currSplit = splitDepths[c];

					float nX = prevSplit * tanHalfFovH;
					float nY = prevSplit * tanHalfFovV;
					float fX = currSplit * tanHalfFovH;
					float fY = currSplit * tanHalfFovV;

					XMVECTOR corners[8] = {
						XMVectorSet(-nX,  nY, prevSplit, 1.0f),
						XMVectorSet(nX,  nY, prevSplit, 1.0f),
						XMVectorSet(-nX, -nY, prevSplit, 1.0f),
						XMVectorSet(nX, -nY, prevSplit, 1.0f),
						XMVectorSet(-fX,  fY, currSplit, 1.0f),
						XMVectorSet(fX,  fY, currSplit, 1.0f),
						XMVectorSet(-fX, -fY, currSplit, 1.0f),
						XMVectorSet(fX, -fY, currSplit, 1.0f),
					};

					for(int k = 0; k < 8; k++){
						corners[k] = XMVector4Transform(corners[k], invCameraView);
					}

					XMVECTOR centroid = XMVectorZero();
					for(int k = 0; k < 8; k++){
						centroid = XMVectorAdd(centroid, corners[k]);
					}
					centroid = XMVectorScale(centroid, 1.0f / 8.0f);

					float radius = 0.0f;
					for(int k = 0; k < 8; k++){
						float d = XMVectorGetX(XMVector3Length(corners[k] - centroid));
						if(d > radius) radius = d;
					}

					XMVECTOR eye = centroid - lightDir * (radius + 1.0f);
					XMMATRIX cascadeView = XMMatrixLookAtLH(eye, centroid, worldUp);

					float minX = FLT_MAX, maxX = -FLT_MAX;
					float minY = FLT_MAX, maxY = -FLT_MAX;
					float minZ = FLT_MAX, maxZ = -FLT_MAX;

					for(int k = 0; k < 8; k++){
						XMVECTOR lc = XMVector4Transform(corners[k], cascadeView);
						float lx = XMVectorGetX(lc);
						float ly = XMVectorGetY(lc);
						float lz = XMVectorGetZ(lc);
						minX = min(minX, lx); maxX = max(maxX, lx);
						minY = min(minY, ly); maxY = max(maxY, ly);
						minZ = min(minZ, lz); maxZ = max(maxZ, lz);
					}

					if(maxZ <= minZ) maxZ = minZ + 1.0f;

					XMMATRIX cascadeProj = XMMatrixOrthographicOffCenterLH(
						minX, maxX, minY, maxY, 0.0f, maxZ
					);

					LIGHT cascadeEntry = lightData;
					cascadeEntry.LightType = LIGHT_TYPE_DIRECTIONAL_CSM;
					cascadeEntry.Enable = true;
					cascadeEntry.CastShadow = true;
					cascadeEntry.Dummy = c + 1;

					XMFLOAT3 eyePos;
					XMStoreFloat3(&eyePos, eye);
					cascadeEntry.Position = XMFLOAT4(eyePos.x, eyePos.y, eyePos.z, 0.0f);

					if(c > 0){
						cascadeEntry.Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
					}

					XMStoreFloat4x4(&cascadeEntry.LightView, XMMatrixTranspose(cascadeView));
					XMStoreFloat4x4(&cascadeEntry.LightProjection, XMMatrixTranspose(cascadeProj));

					light.Lights[lightCount] = cascadeEntry;
					lightCount++;
					shadowCount++;
					addedCascades++;

					prevSplit = currSplit;
				}

				if(addedCascades > 0){
					light.Lights[firstCascadeSlot].Position.w = (float)addedCascades;
				}

				hasCsmLight = true;
				foundLight = true;
				continue;
			}

			// ======== Spot Shadow ========
			if(lightData.LightType == LIGHT_TYPE_SPOT && lightData.CastShadow){

				XMVECTOR eye = transform->position.ToXMVECTOR();
				XMVECTOR forward = XMVector3Normalize(transform->front().ToXMVECTOR());

				XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

				float dp = XMVectorGetX(XMVector3Dot(forward, worldUp));
				if(fabsf(dp) > 0.99f){
					worldUp = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
				}

				XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
				XMVECTOR up = XMVector3Cross(forward, right);

				XMVECTOR at = eye + forward;

				float inner = lightData.Param.y;
				float outer = lightData.Param.z;
				if(outer <= 0.01f){
					outer = 0.01f;
				}
				float fov = XMConvertToRadians(outer) * 2.0f;

				float nearZ = 1.0f;
				float farZ = lightData.Param.x * 1000.0f;
				if(nearZ > farZ){
					farZ = nearZ + 1.0f;
				}

				XMMATRIX lightView = XMMatrixLookAtLH(eye, at, up);
				XMMATRIX lightProj = XMMatrixPerspectiveFovLH(fov, 1.0f, nearZ, farZ);

				XMStoreFloat4x4(&lightData.LightView, XMMatrixTranspose(lightView));
				XMStoreFloat4x4(&lightData.LightProjection, XMMatrixTranspose(lightProj));

				light.Lights[lightCount] = lightData;
				lightCount++;
				shadowCount++;
				foundLight = true;
				continue;
			}

			// ======== Point Shadow ========
			if(lightData.LightType == LIGHT_TYPE_POINT && lightData.CastShadow){

				XMVECTOR eye = transform->position.ToXMVECTOR();

				float nearZ = 0.1f;
				float farZ = lightData.Param.x;

				if(farZ <= nearZ){
					farZ = nearZ + 0.1f;
				}

				XMMATRIX lightProj =
					XMMatrixPerspectiveFovLH(
						XM_PIDIV2,
						1.0f,
						nearZ,
						farZ
					);

				int firstPointFaceSlot = lightCount;
				int addedPointFaces = 0;

				for(int face = 0; face < 6; face++){

					if(lightCount >= LIGHT_MAX_COUNT){
						break;
					}

					XMVECTOR at = eye + s_PointFaces[face].dir;

					XMMATRIX lightView =
						XMMatrixLookAtLH(
							eye,
							at,
							s_PointFaces[face].up
						);

					LIGHT faceLight = lightData;

					faceLight.Dummy = -(face + 1);
					faceLight.Position.w = 0.0f;

					if(face > 0){
						faceLight.Diffuse =
							XMFLOAT4(0, 0, 0, 0);

						faceLight.Ambient =
							XMFLOAT4(0, 0, 0, 0);
					}

					XMStoreFloat4x4(
						&faceLight.LightView,
						XMMatrixTranspose(lightView)
					);

					XMStoreFloat4x4(
						&faceLight.LightProjection,
						XMMatrixTranspose(lightProj)
					);

					light.Lights[lightCount] = faceLight;
					light.Lights[lightCount].Enable = true;

					lightCount++;
					shadowCount++;
					addedPointFaces++;
				}

				if(addedPointFaces > 0){
					light.Lights[firstPointFaceSlot].Position.w =
						(float)addedPointFaces;
				}

				foundLight = true;
				continue;
			}

			// ======== それ以外の shadow light ========
			if(lightData.LightType == LIGHT_TYPE_DIRECTIONAL_CSM){
				continue;
			}

			light.Lights[lightCount] = lightData;
			lightCount++;
			if(lightData.CastShadow){
				shadowCount++;
			}
			foundLight = true;
		}
	}

	light.ActiveLightCount = lightCount;
	light.ShadowAtlasCount = shadowCount;
	graphicsContext->SetLight(&light);

	if(!foundLight){
		deviceContext->RSSetState(nullptr);
		deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		return;
	}

	if(shadowCount == 0){
		deviceContext->RSSetState(nullptr);
		deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		return;
	}

	int ATLAS_GRID = (int)std::sqrt((float)shadowCount);
	if(ATLAS_GRID * ATLAS_GRID < shadowCount){
		ATLAS_GRID++;
	}
	const int ATLAS_SIZE = (int)shadowRenderTarget->size.x;
	const int TILE_SIZE = (ATLAS_GRID > 0) ? (ATLAS_SIZE / ATLAS_GRID) : ATLAS_SIZE;
	int shadowNum = 0;

	SystemRegistry* systemRegistry =
		m_context->sceneManager->GetSystemRegistry();
	StaticBatchUploadSystem* staticBatchUploadSystem = systemRegistry
		? systemRegistry->GetSystem<StaticBatchUploadSystem>()
		: nullptr;
	RHI::RenderHardwareInterfaceService* rhiService =
		m_context->graphics->GetRHIService();
	RHI::IRHIDevice* rhiDevice = rhiService
		? rhiService->GetDevice()
		: nullptr;
	const bool canSubmitStaticShadow =
		staticBatchUploadSystem && rhiDevice &&
		rhiDevice->GetBackendType() == RHI::BackendType::Direct3D11;

	for(int i = 0; i < lightCount; i++){

		if(!light.Lights[i].Enable || !light.Lights[i].CastShadow){
			continue;
		}

		int gx = shadowNum % ATLAS_GRID;
		int gy = shadowNum / ATLAS_GRID;
		int tileX = gx * TILE_SIZE;
		int tileY = gy * TILE_SIZE;

		const LIGHT& currentLight = light.Lights[i];

		newContext.CameraPosition = currentLight.Position;
		newContext.viewMatrix = DirectX::XMMatrixTranspose(XMLoadFloat4x4(&currentLight.LightView));
		newContext.projectionMatrix = DirectX::XMMatrixTranspose(XMLoadFloat4x4(&currentLight.LightProjection));

		graphicsContext->SetCameraPosition(newContext.CameraPosition);
		graphicsContext->SetViewMatrix(newContext.viewMatrix);
		graphicsContext->SetProjectionMatrix(newContext.projectionMatrix);

		D3D11_VIEWPORT vp = {};
		vp.TopLeftX = (float)tileX;
		vp.TopLeftY = (float)tileY;
		vp.Width = (float)TILE_SIZE;
		vp.Height = (float)TILE_SIZE;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		deviceContext->RSSetViewports(1, &vp);

		const RenderPacketFrameBuffer& packetBuffer =
			m_renderSystem->GetRenderPacketBuffer();
		const RenderPacketCullingView shadowCullingView =
			ShadowRenderPacketCullingView::Build(
				newContext,
				ctx.cullingViewKind,
				ctx.cullingViewInstanceID,
				static_cast<std::uint32_t>(shadowNum)
			);
		if(packetBuffer.IsReady()){
			m_renderSystem->PrepareRenderPacketView(shadowCullingView);
		}

		StaticBatchPacketReplacementSet replacements;
		if(packetBuffer.IsReady() && canSubmitStaticShadow){
			(void)StaticBatchShadowSubmission::Submit(
				*rhiDevice,
				*graphicsContext,
				packetBuffer,
				*staticBatchUploadSystem,
				m_renderSystem->GetCullingVisibility(),
				shadowCullingView,
				newContext.renderLayerVisibility,
				static_cast<std::size_t>(maxLayer),
				replacements
			);

			deviceContext->PSSetShader(
				m_RenderablePixelShader->m_PixelShader.Get(),
				nullptr,
				0
			);
			deviceContext->PSSetSamplers(1, 1, &shadowSampler);
			deviceContext->RSSetState(depthClampRS);
		}

		if(packetBuffer.IsReady()){
			const std::span<const RenderPacket> packets = packetBuffer.Packets();
			for(std::size_t packetIndex = 0;
				packetIndex < packets.size();
				++packetIndex){
				if(replacements.Contains(packetIndex)) continue;
				const RenderPacket& packet = packets[packetIndex];
				if(!HasRenderPacketPass(packet.passMask, RenderPacketPassMask::Shadow)){
					continue;
				}
				if(!m_renderSystem->ShouldRenderPacket(
					shadowCullingView,
					packet
				)){
					continue;
				}

				const int layerIndex = static_cast<int>(packet.layer);
				if(static_cast<unsigned>(layerIndex) >=
					static_cast<unsigned>(maxLayer)){
					continue;
				}
				if(!newContext.renderLayerVisibility[layerIndex]) continue;
				IRenderable* renderable =
					m_renderSystem->GetRenderableForPacketKind(packet.kind);
				if(!renderable) continue;

				renderable->Execute(newContext, packet);
			}
		}

		shadowNum++;
	}

	deviceContext->RSSetState(nullptr);
	deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}
