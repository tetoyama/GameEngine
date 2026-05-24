// =======================================================================
// 
// GBufferPass.h
// 
// =======================================================================
#pragma once
#include "../IRenderPass.h"

#include <d3d11.h>
#include <vector>
#include <memory>

class IRenderable;
struct RenderTarget;

struct PixelShaderData;
struct VertexShaderData;

// ジオメトリバッファ (GBuffer) への書き込みパス
class GBufferPass : public IRenderPass{
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	// Renderables
	std::vector<IRenderable*> renderables;

	std::shared_ptr<PixelShaderData> gbufferPixelShader;
	std::shared_ptr<VertexShaderData> gbufferVertexShader;

	// GBuffer targets
	std::vector<RenderTarget*> pRenderTargets;
	RenderTarget* pDepthTarget = nullptr;

	ID3D11SamplerState* pSampler = nullptr;
};
