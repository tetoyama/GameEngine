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
#include "Registry/systemRegistry.h"
#include "System/Render/RenderSystem/Renderable/Model/RenderableModel.h"
#include <Component/LightComponent.h>
#include <Component/cameraComponent.h>
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

	// ======== RenderTarget 切り替え ========
	if(shadowRenderTarget->type == RenderTargetType::RENDERTARGET_TYPE_DEPTH){
		deviceContext->ClearDepthStencilView(shadowRenderTarget->dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		deviceContext->OMSetRenderTargets(0, nullptr, shadowRenderTarget->dsv.Get());
	} else{
		float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		deviceContext->ClearRenderTargetView(shadowRenderTarget->rtv.Get(), color);
		deviceContext->ClearDepthStencilView(shadowRenderTarget->dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		deviceContext->OMSetRenderTargets(1, shadowRenderTarget->rtv.GetAddressOf(), shadowRenderTarget->dsv.Get());
	}

	// DepthClipEnable=FALSE で shadow caster をレンダリング:
	// light-space Z < 0 の caster (視錐台より光源側にある高い物体等) を near plane にクランプして影を正しく描画する
	deviceContext->RSSetState(depthClampRS);

	RenderPassContext newContext = ctx;
	newContext.passPhase = RenderPhase::PHASE_SHADOW;
	newContext.screenSize = Vector2(SHADOWMAP_SIZE, SHADOWMAP_SIZE);

	// ======== メインカメラ取得 ========
	Vector3 mainCamPos = Vector3(ctx.CameraPosition.x, ctx.CameraPosition.y, ctx.CameraPosition.z);
	Vector3 mainCamFront = ctx.cameraData.transformComponent->front();

	// ======== Directional Light 取得 ========
	LightBuffer light;

	// 全ライトを無効化で初期化
	for(int i = 0; i < LIGHT_MAX_COUNT; i++){
		light.Lights[i].Enable = false;
	}
	int lightCount = 0;
	int shadowCount = 0;

	// CSM ライトは最大 1 つに制限 (複数の CSM 平行光を処理する場合はこのフラグを除去)
	bool hasCsmLight = false;

	bool foundLight = false;
	for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
		auto sctx = scene->GetSceneContext();
		const auto& lightEntities = sctx->component->FindEntitiesWithComponent<LightComponent>();
		if(lightEntities.empty()) continue;

		for(Entity ent : lightEntities){
			LightComponent* lightcomp = sctx->component->GetComponent<LightComponent>(ent);
			if(!lightcomp) continue;

			// 最大数を超えたら安全に打ち切る
			if(lightCount >= LIGHT_MAX_COUNT){
				break;
			}

			TransformComponent* transform = sctx->component->GetComponent<TransformComponent>(ent);
			if(transform){
				lightcomp->light.Position = DirectX::XMFLOAT4(transform->position.x, transform->position.y, transform->position.z, 0.0f);
				lightcomp->light.Direction = DirectX::XMFLOAT4(transform->front().x, transform->front().y, transform->front().z, 0.0f);
				lightcomp->light.Dummy = 0;
				if(lightcomp->light.CastShadow){
					if(lightcomp->light.LightType == LIGHT_TYPE_DIRECTIONAL){
						// ======== シャドウカメラ計算 ========
						float shadowSize = lightcomp->light.Param.x / 10.0f;
						if (shadowSize <= 50.0f) {
							shadowSize = 50.0f;
						}

						XMVECTOR camPos = mainCamPos.ToXMVECTOR();
						XMVECTOR camDir = mainCamFront.ToXMVECTOR();
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
						lightcomp->light.Position = newContext.CameraPosition;
						XMStoreFloat4x4(&lightcomp->light.LightView, DirectX::XMMatrixTranspose(newContext.viewMatrix));
						XMStoreFloat4x4(&lightcomp->light.LightProjection, DirectX::XMMatrixTranspose(newContext.projectionMatrix));

						foundLight = true;
					} else if (lightcomp->light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM && !hasCsmLight
					           && ctx.cameraData.cameraComponent && ctx.cameraData.cameraComponent->FarClip > ctx.cameraData.cameraComponent->NearClip) {
						// ======== CSM カスケード計算 ========
						// 各カスケードを個別の LIGHT エントリとしてアトラスに統合する。
						// CbCSM 専用バッファは使用しない。
						XMVECTOR lightDir = XMVector3Normalize(transform->front().ToXMVECTOR());

						float cameraNear  = ctx.cameraData.cameraComponent->NearClip;
						float cameraFar   = ctx.cameraData.cameraComponent->FarClip;
						float fov         = ctx.cameraData.cameraComponent->FOV;
						float aspect      = (ctx.screenSize.y > 0.0f) ? (ctx.screenSize.x / ctx.screenSize.y) : 1.0f;
						float tanHalfFovV = tanf(fov * 0.5f);
						float tanHalfFovH = tanHalfFovV * aspect;

						XMMATRIX invCameraView = XMMatrixInverse(nullptr, ctx.viewMatrix);

						// カスケードスプリット深度を PSSM (対数+線形ブレンド) で計算
						const float     csmNear   = max(cameraNear, 0.1f);
						const float     csmFar    = cameraFar;
						constexpr float csmLambda = 0.85f; // 0.0=完全線形, 1.0=完全対数(近距離重視)
						float splitDepths[DIRECTIONAL_CSM_CASCADE_COUNT];
						for(int c = 0; c < DIRECTIONAL_CSM_CASCADE_COUNT; c++){
							float p        = (float)(c + 1) / (float)DIRECTIONAL_CSM_CASCADE_COUNT;
							float logSplit = csmNear * powf(csmFar / csmNear, p);
							float uniSplit = csmNear + (csmFar - csmNear) * p;
							splitDepths[c] = csmLambda * logSplit + (1.0f - csmLambda) * uniSplit;
						}

						// ライト方向の up ベクトルを決定
						XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
						if(fabsf(XMVectorGetX(XMVector3Dot(lightDir, worldUp))) > 0.99f){
							worldUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
						}

						// 各カスケードの行列を計算して個別の LIGHT エントリとして登録
						float prevSplit = csmNear;
						// 先頭カスケードのスロット番号を記録 (後で実際に追加されたカスケード数を Position.w に書き込むため)
						int firstCascadeSlot = lightCount;
						int addedCascades    = 0;
						for(int c = 0; c < DIRECTIONAL_CSM_CASCADE_COUNT; c++){
							if(lightCount >= LIGHT_MAX_COUNT) break;

							float currSplit = splitDepths[c];

							// サブフラスタムの8頂点をビュー空間で計算してワールド空間に変換
							float nX = prevSplit * tanHalfFovH;
							float nY = prevSplit * tanHalfFovV;
							float fX = currSplit * tanHalfFovH;
							float fY = currSplit * tanHalfFovV;

							XMVECTOR corners[8] = {
								// Near
								XMVectorSet(-nX,  nY, prevSplit, 1.0f),
								XMVectorSet( nX,  nY, prevSplit, 1.0f),
								XMVectorSet(-nX, -nY, prevSplit, 1.0f),
								XMVectorSet( nX, -nY, prevSplit, 1.0f),
								// Far
								XMVectorSet(-fX,  fY, currSplit, 1.0f),
								XMVectorSet( fX,  fY, currSplit, 1.0f),
								XMVectorSet(-fX, -fY, currSplit, 1.0f),
								XMVectorSet( fX, -fY, currSplit, 1.0f),
							};

							// ビュー空間 → ワールド空間
							for(int k = 0; k < 8; k++){
								corners[k] = XMVector4Transform(corners[k], invCameraView);
							}

							// カスケード中心を計算
							XMVECTOR centroid = XMVectorZero();
							for(int k = 0; k < 8; k++){
								centroid = XMVectorAdd(centroid, corners[k]);
							}
							centroid = XMVectorScale(centroid, 1.0f / 8.0f);

							// ライトビュー行列 (視錐台境界より少し後退させて near クリップを避ける)
							float radius = 0.0f;
							for(int k = 0; k < 8; k++){
								float d = XMVectorGetX(XMVector3Length(corners[k] - centroid));
								if(d > radius) radius = d;
							}
							XMVECTOR eye = centroid - lightDir * (radius + 1.0f);
							XMMATRIX cascadeView = XMMatrixLookAtLH(eye, centroid, worldUp);

							// ライト空間での AABB を計算
							float minX =  FLT_MAX, maxX = -FLT_MAX;
							float minY =  FLT_MAX, maxY = -FLT_MAX;
							float minZ =  FLT_MAX, maxZ = -FLT_MAX;
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

							// nearClip を 0 に設定: フラスタム外の shadow caster も捕捉する
							XMMATRIX cascadeProj = XMMatrixOrthographicOffCenterLH(
								minX, maxX, minY, maxY, 0.0f, maxZ);

							// ======== カスケードを個別 LIGHT エントリとして登録 ========
							LIGHT cascadeEntry         = lightcomp->light;
							// GPU ライトバッファ上は LIGHT_TYPE_DIRECTIONAL として登録
							cascadeEntry.LightType     = LIGHT_TYPE_DIRECTIONAL;
							cascadeEntry.Enable        = true;
							cascadeEntry.CastShadow    = true;
							// Dummy = c+1: CSM カスケードマーカー (1=最精細, 2..N=それ以降)
							cascadeEntry.Dummy         = c + 1;

							// Position.w はループ後に実際に追加されたカスケード数で更新するため 0 で初期化
							// 以降のカスケード (c>0): アンビエントなし (最初のエントリのみ適用)
							XMFLOAT3 eyePos;
							XMStoreFloat3(&eyePos, eye);
							cascadeEntry.Position = XMFLOAT4(eyePos.x, eyePos.y, eyePos.z, 0.0f);
							if(c > 0){
								cascadeEntry.Ambient   = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
							}

							// 転置して格納 (シェーダーの mul(v, M) 規約に合わせる)
							XMStoreFloat4x4(&cascadeEntry.LightView,       XMMatrixTranspose(cascadeView));
							XMStoreFloat4x4(&cascadeEntry.LightProjection, XMMatrixTranspose(cascadeProj));

							light.Lights[lightCount] = cascadeEntry;
							lightCount++;
							shadowCount++;
							addedCascades++;

							prevSplit = currSplit;
						}

						// 先頭カスケードエントリの Position.w に実際に追加されたカスケード数を書き込む。
						// シェーダーはこの値を使って LIGHT 配列から各カスケードを参照する。
						if(addedCascades > 0){
							light.Lights[firstCascadeSlot].Position.w = (float)addedCascades;
						}

						hasCsmLight = true;
						foundLight  = true;
						continue;
					} else if (lightcomp->light.LightType == LIGHT_TYPE_SPOT && lightcomp->light.CastShadow) {
						// --- 位置と forward 取得 ---
						XMVECTOR eye = transform->position.ToXMVECTOR();
						XMVECTOR forward = XMVector3Normalize(transform->front().ToXMVECTOR());

						// --- ワールドの上方向（基準ベクトル） ---
						XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

						// --- forward と worldUp が平行に近い場合の対策 ---
						float dp = XMVectorGetX(XMVector3Dot(forward, worldUp));
						if (fabsf(dp) > 0.99f) {
							// ほぼ真上/真下を向いている場合は X 軸を使う
							worldUp = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
						}

						// --- 正確な up / right を再構築（直交化） ---
						XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
						XMVECTOR up = XMVector3Cross(forward, right);

						// --- LookAt 用のターゲット ---
						XMVECTOR at = eye + forward;

						// ------------------------------
						//      ライト行列の計算
						// ------------------------------
						float inner = lightcomp->light.Param.y;
						float outer = lightcomp->light.Param.z;
						if (outer <= 0.01f) {
							outer = 0.01f;
						}
						float fov = XMConvertToRadians(outer) * 2.0f;

						float nearZ = 1.0f;
						float farZ = lightcomp->light.Param.x * 1000.0f;
						if (nearZ > farZ) {
							farZ = nearZ + 1.0f;
						}
						XMMATRIX lightView = XMMatrixLookAtLH(eye, at, up);
						XMMATRIX lightProj = XMMatrixPerspectiveFovLH(fov, 1.0f, nearZ, farZ);

						// 転置して格納
						XMStoreFloat4x4(&lightcomp->light.LightView, XMMatrixTranspose(lightView));
						XMStoreFloat4x4(&lightcomp->light.LightProjection, XMMatrixTranspose(lightProj));


						foundLight = true;

					} else if (lightcomp->light.LightType == LIGHT_TYPE_POINT && lightcomp->light.CastShadow) {

						XMVECTOR eye = transform->position.ToXMVECTOR();

						float nearZ = 0.1f;
						float farZ = lightcomp->light.Param.x;
						if(farZ <= nearZ){
							farZ = nearZ + 0.1f;
						}

						XMMATRIX lightProj =
							XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, nearZ, farZ);

						int firstPointFaceSlot = lightCount;
						int addedPointFaces = 0;
						for (int face = 0; face < 6; face++) {

							if (lightCount >= LIGHT_MAX_COUNT)
								break;

							XMVECTOR at = eye + s_PointFaces[face].dir;

							XMMATRIX lightView =
								XMMatrixLookAtLH(eye, at, s_PointFaces[face].up);

							LIGHT faceLight = lightcomp->light;
							faceLight.Dummy = -(face + 1);
							faceLight.Position.w = 0.0f;
							if (face > 0) {
								faceLight.Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
							}

							XMStoreFloat4x4(
								&faceLight.LightView,
								XMMatrixTranspose(lightView)
							);
							XMStoreFloat4x4(
								&faceLight.LightProjection,
								XMMatrixTranspose(lightProj)
							);

							// Light配列に直接積む
							light.Lights[lightCount] = faceLight;
							light.Lights[lightCount].Enable = true;

							lightCount++;
							shadowCount++;
							addedPointFaces++;
						}
						if (addedPointFaces > 0) {
							light.Lights[firstPointFaceSlot].Position.w = (float)addedPointFaces;
						}

						foundLight = true;
						continue;
					}
				}
			}
			// 現在のライトスロットにコピー
			// LIGHT_TYPE_DIRECTIONAL_CSM は上のブロックで continue 済みのためここには来ない。
			// ただし hasCsmLight==true で 2 つ目以降の CSM が来た場合は GPU バッファに追加しない。
			if(lightcomp->light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM){
				continue;
			}
			light.Lights[lightCount] = lightcomp->light;
			lightCount++;
			if(lightcomp->light.CastShadow){
				shadowCount++;
			}
		}
	}
	light.ActiveLightCount = lightCount;
	light.ShadowAtlasCount = shadowCount;
	graphicsContext->SetLight(&light);

	if(!foundLight){
		return; // ライトが無い場合は中断
	}

	int ATLAS_GRID = (int)std::sqrt((float)shadowCount);
	if(ATLAS_GRID * ATLAS_GRID < shadowCount){
		ATLAS_GRID++;
	}
	const int ATLAS_SIZE = (int)shadowRenderTarget->size.x;
	const int TILE_SIZE = (ATLAS_GRID > 0) ? (ATLAS_SIZE / ATLAS_GRID) : ATLAS_SIZE;
	int shadowNum = 0;

	for(int i = 0; i < lightCount; i++){

		if(!light.Lights[i].Enable || !light.Lights[i].CastShadow){
			continue;
		}

		// ======== 各ライトのシャドウをアトラスにレンダリング ========
		// CSM カスケードエントリ (Dummy >= 1) も通常ライトと同じパスで処理する。
		// 各カスケードは LightView / LightProjection に自身の行列を持つため、
		// 通常の平行光と同じレンダリング処理で正しくシャドウマップが生成される。
		int gx = shadowNum % ATLAS_GRID;
		int gy = shadowNum / ATLAS_GRID;
		int tileX = gx * TILE_SIZE;
		int tileY = gy * TILE_SIZE;

		// ======== GraphicsContext に反映 ========
		newContext.CameraPosition = light.Lights[i].Position;
		newContext.viewMatrix = DirectX::XMMatrixTranspose(XMLoadFloat4x4(&light.Lights[i].LightView));
		newContext.projectionMatrix = DirectX::XMMatrixTranspose(XMLoadFloat4x4(&light.Lights[i].LightProjection));

		// GraphicsContext に反映
		graphicsContext->SetCameraPosition(newContext.CameraPosition);
		graphicsContext->SetViewMatrix(newContext.viewMatrix);
		graphicsContext->SetProjectionMatrix(newContext.projectionMatrix);

		// ======== Viewport ========
		D3D11_VIEWPORT vp = {};
		vp.TopLeftX = (float)tileX;
		vp.TopLeftY = (float)tileY;
		vp.Width = (float)TILE_SIZE;
		vp.Height = (float)TILE_SIZE;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		deviceContext->RSSetViewports(1, &vp);

		// ======== レンダリング実行 ========
		for(int j = 0; j < (int)RenderLayer::MaxRenderLayer; j++){

			if(!newContext.renderLayerVisibility[j]) continue;

			for(auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
				auto sctx = scene->GetSceneContext();

				std::vector<Entity> entities = sctx->component->FindEntitiesWithComponent<TransformComponent>();
				if(entities.empty()) continue;

				for(Entity ent : entities){

					RenderLayer layer = scene->GetRenderLayerFromEntity(ent);
					if((int)layer != j) continue;

					for(auto renderable : renderables){
						renderable->Execute(newContext, sctx, ent);
					}
				}
			}
		}

		shadowNum++;
	}

	// デフォルトのラスタライザステートに戻す (DepthClipEnable=TRUE)
	deviceContext->RSSetState(nullptr);

	deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	//deviceContext->PSSetShaderResources(TextureSlot_ShadowMap, 1, nullSRV);

	//ImGui::Image((ImTextureRef)shadowRenderTarget->srv.Get(), ImVec2(200, 200));
}
