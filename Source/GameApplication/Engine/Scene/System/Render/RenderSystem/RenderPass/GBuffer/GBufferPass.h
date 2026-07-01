// =======================================================================
// 
// GBufferPass.h
// 
// =======================================================================
#pragma once
#include "../IRenderPass.h"

#include <d3d11.h>
#include <memory>
#include <vector>
#include <wrl/client.h>

#include "System/Render/RenderSystem/RenderPacket/StaticBatchGpuInstanceBuffer.h"
#include "System/Render/StaticBatch/StaticBatchGBufferSubmissionTelemetry.h"
#include "System/Render/StaticBatch/StaticBatchVisibleInstanceBuffer.h"

class IRenderable;
struct RenderTarget;

struct PixelShaderData;
struct VertexShaderData;

// ジオメトリバッファ (GBuffer) への書き込みパス
class GBufferPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	const StaticBatchGBufferSubmissionTelemetry&
	GetStaticBatchTelemetry() const noexcept {
		return m_staticBatchTelemetry;
	}

	// Renderables
	std::vector<IRenderable*> renderables;

	std::shared_ptr<PixelShaderData> m_GBufferPixelShader;
	std::shared_ptr<VertexShaderData> m_GBufferVertexShader;

	// GBuffer targets
	std::vector<RenderTarget*> pRenderTargets;
	RenderTarget* pDepthTarget = nullptr;

	ID3D11SamplerState* sampler = nullptr;

private:
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_materialStencilWriteState;
	StaticBatchVisibleInstanceBuffer m_staticBatchVisibleInstances;
	StaticBatchGpuInstanceBuffer m_staticBatchVisibleGpuInstances;
	StaticBatchGBufferSubmissionTelemetry m_staticBatchTelemetry;
};
