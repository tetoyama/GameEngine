// =======================================================================
// 
// LightingPass.cpp
// 
// =======================================================================
#include "LightingPass.h"
#include "Shader/commonDefine.h"

#include <algorithm>

#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderPhase.h"

#include "../GBuffer/GBufferPass.h"
#include "../ShadowMap/ShadowMapPass.h"

#include "scene.h"
#include "System/Render/RenderSystem/Renderable/IRenderable.h"
#include "System/Render/RenderSystem/RenderTarget/renderTarget.h"
#include "Component/transformComponent.h"
#include "Registry/componentRegistry.h"

#include "Graphics/graphicsContext.h"
#include "Graphics/mainRenderer.h"

#include <System/Render/RenderSystem/Renderable/Model/RenderableModel.h>
#include <System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.h>
#include <System/Render/RenderSystem/Renderable/Wave/RenderableWave.h>

#include "Resources/resourceService.h"
#include "Resources/Data/shaderData.h"
#include "Resources/Data/textureData.h"
#include <ImGui/imgui_impl_dx11.h>

void LightingPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

	m_renderSystem = renderSystem;
	m_context = context;

	// Linear
	D3D11_SAMPLER_DESC desc{};
	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.MinLOD = 0;
	desc.MaxLOD = D3D11_FLOAT32_MAX;
	m_context->graphics->GetDevice()->CreateSamplerState(&desc, &m_LinearSampler);

	// Environment map sampler (trilinear + wrap for cubemap)
	D3D11_SAMPLER_DESC envDesc{};
	envDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	envDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	envDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	envDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	envDesc.MinLOD = 0;
	envDesc.MaxLOD = D3D11_FLOAT32_MAX;
	m_context->graphics->GetDevice()->CreateSamplerState(&envDesc, &m_EnvMapSampler);

	D3D11_BUFFER_DESC debugBufferDesc{};
	debugBufferDesc.ByteWidth = sizeof(CbLightingDebug);
	debugBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	debugBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	debugBufferDesc.CPUAccessFlags = 0;
	debugBufferDesc.MiscFlags = 0;
	m_context->graphics->GetDevice()->CreateBuffer(
		&debugBufferDesc,
		nullptr,
		m_lightingDebugBuffer.ReleaseAndGetAddressOf()
	);

	Vector2 size = Vector2((float)context->graphics->m_width, (float)context->graphics->m_height);

	// ----- RenderTargets -----
	// HDR float16 バッファ: エミッシブ HDR 値を保持しブルーム入力に使用する
	pRenderTarget = new RenderTarget(size, context->graphics, RENDERTARGET_TYPE_COLOR);

	m_LightingVertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\LightingVS.cso");
}

void LightingPass::Finalize() {

	m_LightingVertexShader.reset();
	m_lightingDebugBuffer.Reset();

	delete pRenderTarget;
	pRenderTarget = nullptr;

	if(m_LinearSampler){
		m_LinearSampler->Release();
		m_LinearSampler = nullptr;
	}

	if (m_EnvMapSampler) {
		m_EnvMapSampler->Release();
		m_EnvMapSampler = nullptr;
	}

	m_EnvironmentMap.reset();

}

void LightingPass::SetTextureSlot(GBufferPass* gBufferPass, ShadowMapPass* shadowMapPass, GraphicsContext* gc) {

	ID3D11DeviceContext* dc = gc->GetDeviceContext();

	ID3D11ShaderResourceView* nullSRV[LightingSlot_Max] = {};
	dc->PSSetShaderResources(0, LightingSlot_Max, nullSRV);

	dc->PSSetShaderResources(LightingSlot_GAlbedo, 1, gBufferPass->pRenderTargets[GBufferSlot_Albedo]->srv.GetAddressOf());
	dc->PSSetShaderResources(LightingSlot_GNormal, 1, gBufferPass->pRenderTargets[GBufferSlot_Normal]->srv.GetAddressOf());
	dc->PSSetShaderResources(LightingSlot_GPosition, 1, gBufferPass->pRenderTargets[GBufferSlot_Position]->srv.GetAddressOf());
	dc->PSSetShaderResources(LightingSlot_GMaterial, 1, gBufferPass->pRenderTargets[GBufferSlot_Material]->srv.GetAddressOf());
	dc->PSSetShaderResources(LightingSlot_GEmissive, 1, gBufferPass->pRenderTargets[GBufferSlot_Emissive]->srv.GetAddressOf());
	dc->PSSetShaderResources(LightingSlot_GParam, 1, gBufferPass->pRenderTargets[GBufferSlot_Param]->srv.GetAddressOf());
	dc->PSSetShaderResources(LightingSlot_ShadowMap, 1, shadowMapPass->shadowRenderTarget->srv.GetAddressOf());

	if (m_EnvironmentMap && m_EnvironmentMap->pTexture) {
		dc->PSSetShaderResources(LightingSlot_EnvironmentMap, 1, m_EnvironmentMap->pTexture.GetAddressOf());
	}

	ID3D11SamplerState* samplers[] =
	{
		m_LinearSampler					// s2
	};
	dc->PSSetSamplers(2, 1, samplers);

	if (m_EnvMapSampler) {
		dc->PSSetSamplers(3, 1, &m_EnvMapSampler);	// s3
	}
}


void LightingPass::Execute(const RenderPassContext& ctx){

	pRenderTarget->Resize(ctx.screenSize, m_context->graphics);

	float clearColor[4] = {0,0,0,0};
	pRenderTarget->Clear(m_context->graphics->GetDeviceContext(), clearColor);

	ID3D11DeviceContext* dc = m_context->graphics->GetDeviceContext();
	GraphicsContext* gc = m_context->renderer->GetGraphicsContext();

	dc->OMSetRenderTargets(1, pRenderTarget->rtv.GetAddressOf(), nullptr);

	dc->VSSetShader(m_LightingVertexShader->m_VertexShader.Get(), nullptr, 0);
	dc->IASetInputLayout(m_LightingVertexShader->m_VertexLayout.Get());

	D3D11_VIEWPORT vp = {};
	vp.Width = ctx.screenSize.x;
	vp.Height = ctx.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	dc->RSSetViewports(1, &vp);

	const CbLightingDebug settings = m_renderSystem
		? m_renderSystem->GetLightingDebugSettings()
		: CbLightingDebug{};
	if(m_lightingDebugBuffer){
		dc->UpdateSubresource(
			m_lightingDebugBuffer.Get(),
			0,
			nullptr,
			&settings,
			0,
			0
		);
		ID3D11Buffer* debugBuffer = m_lightingDebugBuffer.Get();
		dc->PSSetConstantBuffers(3, 1, &debugBuffer);
	}

	// Shadow無効とLight数制限は既存Material Shaderを変更せず、
	// CbPerFrameの一時コピーで診断する。描画後に必ず元データへ戻す。
	LightBuffer originalLights{};
	bool lightBufferOverridden = false;
	if(gc && gc->GetLight()){
		originalLights = *gc->GetLight();
		LightBuffer diagnosticLights = originalLights;

		if(settings.LightingDebugMaxActiveLights > 0){
			const int sourceCount = (std::clamp)(
				diagnosticLights.ActiveLightCount,
				0,
				LIGHT_MAX_COUNT
			);
			const int limitedCount = (std::min)(
				sourceCount,
				settings.LightingDebugMaxActiveLights
			);
			diagnosticLights.ActiveLightCount = limitedCount;

			// CSM CascadeとPoint Shadow Faceは先頭EntryのPosition.wから
			// 後続Entryを直接参照する。ActiveLightCountだけを切り詰めると
			// 診断範囲外のEntryまで評価されるため、展開数も残数へ制限する。
			for(int index = 0; index < limitedCount; ++index){
				LIGHT& light = diagnosticLights.Lights[index];
				const int remainingEntries = limitedCount - index;
				if(light.LightType == LIGHT_TYPE_DIRECTIONAL_CSM &&
					light.Dummy == 1){
					const int cascadeCount = (std::clamp)(
						static_cast<int>(light.Position.w + 0.5f),
						1,
						remainingEntries
					);
					light.Position.w = static_cast<float>(cascadeCount);
				}else if(light.LightType == LIGHT_TYPE_POINT &&
					light.Dummy == -1){
					const int faceCount = (std::clamp)(
						static_cast<int>(light.Position.w + 0.5f),
						1,
						remainingEntries
					);
					light.Position.w = static_cast<float>(faceCount);
				}
			}
			lightBufferOverridden = true;
		}

		if((settings.LightingDebugFlags &
			LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS) != 0u){
			for(int index = 0; index < LIGHT_MAX_COUNT; ++index){
				diagnosticLights.Lights[index].CastShadow = 0;
			}
			lightBufferOverridden = true;
		}

		if(lightBufferOverridden){
			gc->SetLight(&diagnosticLights);
		}
	}

	// 登録マテリアル数 + デバッグ分だけ描画。
	// 各PSは自分の担当materialID以外をGetMaterialIDで早期discardする。
	// Material dispatchをswitchへ統合せず、診断設定はDraw全体で均一な値だけを使用する。
	for(const auto& ps : m_renderSystem->GetDeferredPSList()){
		if(!ps) continue;
		dc->PSSetShader(ps->m_PixelShader.Get(), nullptr, 0);
		gc->DrawQuad();
	}

	if(lightBufferOverridden){
		gc->SetLight(&originalLights);
	}

	ID3D11Buffer* nullConstantBuffer = nullptr;
	dc->PSSetConstantBuffers(3, 1, &nullConstantBuffer);

	ID3D11ShaderResourceView* nullSRV[LightingSlot_Max] = {};
	dc->PSSetShaderResources(0, LightingSlot_Max, nullSRV);

	ID3D11SamplerState* nullSampler[2] = {nullptr, nullptr};
	dc->PSSetSamplers(2, 2, nullSampler);

	{
		return;
		// ...以下dead codeはそのまま...
	}
}
