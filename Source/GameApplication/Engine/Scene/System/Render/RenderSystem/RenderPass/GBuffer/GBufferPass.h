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

class GBufferPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	// Renderables
	std::vector<IRenderable*> renderables;

	std::shared_ptr<PixelShaderData> m_GBufferPixelShader;
	std::shared_ptr<VertexShaderData> m_GBufferVertexShader;

	// GBuffer targets
	std::vector<RenderTarget*> pRenderTargets;
	RenderTarget* pDepthTarget = nullptr;

	ID3D11SamplerState* sampler = nullptr;
};
