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
	m_context = context;
}

void PostEffectPass::Finalize() {
	m_cameraRuntime.Reset();
	resultSrv = nullptr;
	resultRtv = nullptr;
	m_initialSRV = nullptr;
	m_initialRTV = nullptr;
	m_gBufferPass = nullptr;
}

void PostEffectPass::SetInputs(
	ID3D11ShaderResourceView* initialSRV,
	ID3D11RenderTargetView** initialRTV,
	GBufferPass* gBufferPass
){
	m_initialSRV = initialSRV;
	m_initialRTV = initialRTV;
	m_gBufferPass = gBufferPass;
}

void PostEffectPass::Execute(const RenderPassContext& ctx) {
	GraphicsContext* graphics = m_context ? m_context->graphics : nullptr;
	if(!graphics){
		resultSrv = m_initialSRV;
		resultRtv = m_initialRTV;
		return;
	}

	std::vector<PostProcessNode> postNodes;
	std::unordered_map<int, int> effectIndexToPostNodeIndex;
	const DirectX::XMFLOAT4 clearColor = {0, 0, 0, 1};

	CameraComponent* camera = ctx.cameraData.cameraComponent;
	std::uint64_t cameraRuntimeGeneration = 0;
	if(camera){
		const std::uint32_t sceneContextID = ctx.cameraData.ref.GetContextID();
		const std::uint64_t cameraEntity =
			ctx.cameraData.ref.GetEntityID().GetPackedValue();
		cameraRuntimeGeneration = m_cameraRuntime.BeginCamera(
			sceneContextID,
			cameraEntity
		);

		const auto& sortedIndices = camera->TopologicalSortPostEffects();
		postNodes.reserve(sortedIndices.size());
		effectIndexToPostNodeIndex.reserve(sortedIndices.size());

		for(int idx : sortedIndices){
			if(idx < 0){
				continue; // -1/-2 は ScreenInput/Output ノード
			}
			if(static_cast<std::size_t>(idx) >= camera->postEffects.size()){
				continue;
			}

			auto& effect = camera->postEffects[static_cast<std::size_t>(idx)];
			if(!effect.enabled || !effect.ps || !effect.vs){
				continue;
			}

			CameraPostEffectRuntimeKey runtimeKey;
			runtimeKey.sceneContextID = sceneContextID;
			runtimeKey.cameraEntity = cameraEntity;
			runtimeKey.effectIndex = idx;
			CameraPostEffectRuntime& runtime = m_cameraRuntime.Acquire(
				runtimeKey,
				cameraRuntimeGeneration
			);

			const float scale = (std::max)(
				0.1f,
				(std::min)(1.0f, effect.resolutionScale)
			);
			const Vector2 scaledSize{
				ctx.screenSize.x * scale,
				ctx.screenSize.y * scale
			};
			const bool runtimeReady = runtime.Ensure(
				graphics->GetDevice(),
				scaledSize,
				effect.mipLevels
			);
			if(!runtimeReady && !runtime.IsValid()){
				continue;
			}
			runtime.Clear(graphics->GetDeviceContext(), &clearColor.x);

			PostProcessNode node{};
			node.id = idx;
			node.shader.m_VS = effect.vs->m_VertexShader;
			node.shader.m_PS = effect.ps->m_PixelShader;
			node.shader.m_InputLayout = effect.vs->m_VertexLayout;
			node.param = effect.Param;
			node.resolutionScale = effect.resolutionScale;
			// Resize失敗で旧Runtimeを継続使用する場合も、実資源のMip数へ合わせる。
			node.mipLevels = runtime.mipLevels;
			node.outputWidth = static_cast<UINT>(runtime.resolution.x);
			node.outputHeight = static_cast<UINT>(runtime.resolution.y);
			node.rtv = runtime.renderTargetView.GetAddressOf();
			node.srv = runtime.shaderResourceView.Get();
			node.tex = runtime.texture.Get();

			node.inputs.clear();
			const int postNodeIndex = static_cast<int>(postNodes.size());
			effectIndexToPostNodeIndex[idx] = postNodeIndex;
			postNodes.push_back(std::move(node));
		}

		// リンクを後から解決する。
		for(auto& node : postNodes){
			const int effectIndex = node.id;
			const auto& resolvedInputs =
				camera->GetResolvedPostEffectInputs(effectIndex);
			node.inputs.assign(resolvedInputs.size(), -1);

			for(std::size_t slotIndex = 0;
				slotIndex < resolvedInputs.size();
				++slotIndex){
				const int inputSource = resolvedInputs[slotIndex];
				if(inputSource == -2){
					node.inputs[slotIndex] = -2;
				}else{
					auto it = effectIndexToPostNodeIndex.find(inputSource);
					if(it != effectIndexToPostNodeIndex.end()){
						node.inputs[slotIndex] = it->second;
					}
				}
			}
		}

		m_cameraRuntime.EndCamera(cameraRuntimeGeneration);
	}else{
		// Cameraが存在しないPassでは以前のCamera Runtimeを保持し続けない。
		m_cameraRuntime.Reset();
	}

	if(!postNodes.empty()){
		// 未接続スロット(-1)を初期SRV扱い(-2)へ統一する。
		for(auto& node : postNodes){
			for(int& input : node.inputs){
				if(input == -1){
					input = -2;
				}
			}
		}

		ID3D11ShaderResourceView* gbufferSRVs[PostEffectGBufferSlot_Count] = {};
		if(m_gBufferPass){
			for(int g = 0; g < GBufferSlot_Max; ++g){
				if(m_gBufferPass->pRenderTargets[g]){
					gbufferSRVs[g] =
						m_gBufferPass->pRenderTargets[g]->srv.Get();
				}
			}
			if(m_gBufferPass->pDepthTarget){
				gbufferSRVs[
					PostEffectGBufferSlot_Depth - PostEffectGBufferSlot_Start
				] = m_gBufferPass->pDepthTarget->srv.Get();
			}
		}

		graphics->ApplyPostProcessChain(
			postNodes,
			m_initialSRV,
			gbufferSRVs,
			PostEffectGBufferSlot_Count
		);
		resultSrv = graphics->m_CurrentSRV;
		resultRtv = graphics->m_CurrentRTV;
	}else{
		resultSrv = m_initialSRV;
		resultRtv = m_initialRTV;
	}
}
