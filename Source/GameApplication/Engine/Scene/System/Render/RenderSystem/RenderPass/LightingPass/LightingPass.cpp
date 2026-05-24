// =======================================================================
// 
// LightingPass.cpp
// 
// =======================================================================
#include "LightingPass.h"
#include "Shader/commonDefine.h"

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

#include <System/Render/RenderSystem/Renderable/Model/RenderableModel.h>
#include <System/Render/RenderSystem/Renderable/Terrain/RenderableTerrain.h>
#include <System/Render/RenderSystem/Renderable/Wave/RenderableWave.h>

#include "Resources/resourceService.h"
#include "Resources/Data/shaderData.h"
#include "Resources/Data/textureData.h"
#include <ImGui/imgui_impl_dx11.h>

void LightingPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {

	m_pRenderSystem = renderSystem;
	m_pContext = pContext;

	// Linear
	D3D11_SAMPLER_DESC m_Desc{};
	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.MinLOD = 0;
	desc.MaxLOD = D3D11_FLOAT32_MAX;
	m_pContext->pGraphics->GetDevice()->CreateSamplerState(&desc, &m_LinearSampler);

	// Environment map sampler (trilinear + wrap for cubemap)
	D3D11_SAMPLER_DESC m_EnvDesc{};
	envDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	envDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	envDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	envDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	envDesc.MinLOD = 0;
	envDesc.MaxLOD = D3D11_FLOAT32_MAX;
	m_pContext->pGraphics->GetDevice()->CreateSamplerState(&envDesc, &m_EnvMapSampler);

	Vector2 m_Size= Vector2((float)pContext->pGraphics->m_width, (float)pContext->pGraphics->m_height);

	// ----- RenderTargets -----
	// HDR float16 バッファ: エミッシブ HDR 値を保持しブルーム入力に使用する
	pRenderTarget = new RenderTarget(size, pContext->pGraphics, RENDERTARGET_TYPE_COLOR);

	m_LightingVertexShader = m_pContext->pResource->Load<VertexShaderData>("Asset\\Shader\\LightingVS.cso");
}

void LightingPass::Finalize() {

	m_LightingVertexShader.reset();

	delete m_PRenderTarget;
	pRenderTarget = nullptr;

	m_LinearSampler->Release();
	m_LinearSampler = nullptr;

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
	dc->PSSetShaderResources(LightingSlot_ShadowMap, 1, shadowMapPass->pShadowRenderTarget->srv.GetAddressOf());

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


void LightingPass::Execute(const RenderPassContext& ctx) {

	pRenderTarget->Resize(ctx.screenSize, m_pContext->pGraphics);

	float m_ClearColor[4] = { 0,0,0,0 };
	pRenderTarget->Clear(m_pContext->pGraphics->GetDeviceContext(), clearColor);

	ID3D11DeviceContext* dc = m_pContext->pGraphics->GetDeviceContext();
	GraphicsContext* gc = m_pContext->pRenderer->GetGraphicsContext();

	dc->OMSetRenderTargets(1, pRenderTarget->rtv.GetAddressOf(), nullptr);

	dc->VSSetShader(m_LightingVertexShader->m_VertexShader.Get(), nullptr, 0);
	dc->IASetInputLayout(m_LightingVertexShader->m_VertexLayout.Get());
	PixelShaderData* ps = m_pRenderSystem->GetDeferredPS();
	dc->PSSetShader(ps ? ps->m_PixelShader.Get() : nullptr, nullptr, 0);
	D3D11_VIEWPORT m_Vp= {};
	vp.Width = ctx.screenSize.x;
	vp.Height = ctx.screenSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	dc->RSSetViewports(1, &vp);

	gc->DrawQuad();

	ID3D11ShaderResourceView* nullSRV[LightingSlot_Max] = {};
	dc->PSSetShaderResources(0, LightingSlot_Max, nullSRV);

	ID3D11SamplerState* nullSampler[2] = { nullptr, nullptr };
	dc->PSSetSamplers(2, 2, nullSampler);

	{
		return;
	// ================================
	// ImGui Debug Draw（暫定）
	// ================================

	// 1. バックバッファに戻す
		dc->OMSetRenderTargets(0, nullptr, nullptr);

		// 5. ImGui 描画
		ImGui::Begin("LightingPass Debug");

		ImGui::Text("LightingPass Debug View");

		// Albedo
		ImGui::Image(
			pRenderTarget->srv.Get(),
			ImVec2(256, ctx.screenSize.y / ctx.screenSize.x * 256.0f)
		);

		ImGui::End();
	}
}
