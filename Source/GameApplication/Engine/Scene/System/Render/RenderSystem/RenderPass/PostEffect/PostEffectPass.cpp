// =======================================================================
// 
// PostEffectPass.cpp
// 
// =======================================================================
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

	std::vector<PostProcessNode>      postNodes;
	std::unordered_map<int, int>      effectIndexToPostNodeIndex;
	const DirectX::XMFLOAT4          clearColor = {0, 0, 0, 1};

	CameraComponent* camera = ctx.cameraData.cameraComponent;

	if (camera) {
		const auto& sortedIndices = camera->TopologicalSortPostEffects();
		postNodes.reserve(sortedIndices.size());
		effectIndexToPostNodeIndex.reserve(sortedIndices.size());

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
			node.resolutionScale     = e.resolutionScale;

			float scale = max(0.1f, min(1.0f, e.resolutionScale));
			Vector2 scaledSize{ ctx.screenSize.x * scale, ctx.screenSize.y * scale };
			e.ResizeTexture(graphics->GetDevice(), scaledSize);
			e.Clear(graphics->GetDeviceContext(), &clearColor.x);
			node.outputWidth = static_cast<UINT>(e.resolution.x);
			node.outputHeight = static_cast<UINT>(e.resolution.y);
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
			const auto& resolvedInputs = camera->GetResolvedPostEffectInputs(effectIdx);

			node.inputs.assign(resolvedInputs.size(), -1);

			for (size_t slotIndex = 0; slotIndex < resolvedInputs.size(); ++slotIndex) {
				int inputSource = resolvedInputs[slotIndex];
				if (inputSource == -2) {
					node.inputs[slotIndex] = -2;
				} else {
					auto it = effectIndexToPostNodeIndex.find(inputSource);
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

}
