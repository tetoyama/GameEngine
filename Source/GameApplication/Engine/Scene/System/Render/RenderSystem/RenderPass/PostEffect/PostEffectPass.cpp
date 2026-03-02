#include "PostEffectPass.h"
#include "Shader/commonDefine.h"

#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../RenderTarget/renderTarget.h"
#include "../GBuffer/GBufferPass.h"

#include "Graphics/graphicsContext.h"
#include "Component/CameraComponent.h"

#include <algorithm>

void PostEffectPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context) {
	m_renderSystem = renderSystem;
	m_context      = context;
}

void PostEffectPass::Finalize() {
}

void PostEffectPass::SetInputs(ID3D11ShaderResourceView* initialSRV, ID3D11RenderTargetView** initialRTV, GBufferPass* gBufferPass) {
	m_initialSRV  = initialSRV;
	m_initialRTV = initialRTV;
	m_gBufferPass = gBufferPass;
}

void PostEffectPass::Execute(const RenderPassContext& ctx) {

	GraphicsContext*     graphics      = m_context->graphics;
	ID3D11DeviceContext* deviceContext  = graphics->GetDeviceContext();

	std::vector<PostProcessNode>      postNodes;
	std::unordered_map<int, int>      effectIndexToPostNodeIndex;
	const DirectX::XMFLOAT4          clearColor = {0, 0, 0, 1};

	CameraComponent* camera = ctx.cameraData.CameraComponent;

	if (camera) {
		auto sortedIndices = camera->TopologicalSortPostEffects();

		for (int idx : sortedIndices) {
			if (idx < 0) continue; // -1/-2 は ScreenInput/Output ノード

			auto& e = camera->postEffects[idx];
			if (!e.enabled || !e.ps || !e.vs) continue;

			PostProcessNode node{};
			node.id                  = idx;
			node.shader.m_VS         = e.vs->m_VertexShader;
			node.shader.m_PS         = e.ps->m_PixelShader;
			node.shader.m_InputLayout = e.vs->m_VertexLayout;
			node.param               = e.Param;

			e.ResizeTexture(graphics->GetDevice(), ctx.screenSize);
			e.Clear(graphics->GetDeviceContext(), &clearColor.x);
			node.rtv = e.rtv.GetAddressOf();
			node.srv = e.srv.Get();
			node.tex = e.tex.Get();

			node.inputs.clear();
			int postNodeIndex = static_cast<int>(postNodes.size());
			effectIndexToPostNodeIndex[idx] = postNodeIndex;

			postNodes.push_back(std::move(node));
		}

		// リンクを後から解決
		for (auto& node : postNodes) {
			int   effectIdx = node.id;
			auto& effect    = camera->postEffects[effectIdx];

			node.inputs.assign(effect.inputPins.size(), -1);

			for (auto& link : camera->postEffectLinks) {
				if (link.endNode != effectIdx) continue;

				auto pinIt = std::find(
					effect.inputPins.begin(), effect.inputPins.end(), link.endAttr
				);
				if (pinIt == effect.inputPins.end()) {
					OutputDebugStringA("PostEffectPass: link.endAttr does not match any inputPin\n");
					continue;
				}

				int slotIndex = (int)(pinIt - effect.inputPins.begin());
				if (link.startNode < 0) {
					node.inputs[slotIndex] = -2; // ScreenInput
				} else {
					auto it = effectIndexToPostNodeIndex.find(link.startNode);
					if (it != effectIndexToPostNodeIndex.end()) {
						node.inputs[slotIndex] = it->second;
					}
				}
			}
		}
	}

	if (!postNodes.empty()) {

		// 未接続スロット (-1) を初期 SRV 扱い (-2) に統一
		for (auto& node : postNodes) {
			for (size_t i = 0; i < node.inputs.size(); ++i) {
				if (node.inputs[i] == -1) {
					node.inputs[i] = -2;
				}
			}
		}

		// SRV / RTV を全解除
		ID3D11ShaderResourceView* nullSRV[TextureSlot_Max] = {nullptr};
		deviceContext->PSSetShaderResources(0, TextureSlot_Max, nullSRV);
		ID3D11RenderTargetView* nullRTV[1] = {nullptr};
		deviceContext->OMSetRenderTargets(1, nullRTV, nullptr);

		// GBuffer SRV を収集
		ID3D11ShaderResourceView* gbufferSRVs[PostEffectGBufferSlot_Count] = {};
		for (int g = 0; g < GBufferSlot_Max; ++g) {
			gbufferSRVs[g] = m_gBufferPass->pRenderTargets[g]->srv.Get();
		}
		gbufferSRVs[PostEffectGBufferSlot_Depth - PostEffectGBufferSlot_Start] =
			m_gBufferPass->pDepthTarget->srv.Get();

		graphics->ApplyPostProcessChain(
			postNodes, m_initialSRV, gbufferSRVs, PostEffectGBufferSlot_Count
		);

		resultSrv = graphics->m_CurrentSRV;
		resultRtv = graphics->m_CurrentRTV;

	} else {
		resultSrv = m_initialSRV;
		resultRtv = m_initialRTV;
	}

	// クリーンアップ
	{
		ID3D11ShaderResourceView* nullSRV[TextureSlot_Max] = {nullptr};
		deviceContext->PSSetShaderResources(0, TextureSlot_Max, nullSRV);
		ID3D11RenderTargetView* nullRTV[1] = {nullptr};
		deviceContext->OMSetRenderTargets(1, nullRTV, nullptr);
	}
}
