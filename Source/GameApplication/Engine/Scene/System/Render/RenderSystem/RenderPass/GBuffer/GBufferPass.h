#pragma once
#include "../IRenderPass.h"

#include <d3d11.h>
#include <vector>

class IRenderable;
struct RenderTarget;

class GBufferPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	// Renderables
	std::vector<IRenderable*> renderables;

	// GBuffer targets
	std::vector<RenderTarget*> pRenderTargets;
	RenderTarget* pDepthTarget = nullptr;

	ID3D11SamplerState* sampler = nullptr;
};
