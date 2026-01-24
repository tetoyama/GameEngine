#pragma once
#include "../IRenderPass.h"
#include <d3d11.h>
#include <memory>
#include <vector>

class IRenderable;
struct RenderTarget;
struct PixelShaderData;

class ShadowMapPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	std::shared_ptr<PixelShaderData> m_RenderablePixelShader;

	std::vector<IRenderable*> renderables;
	RenderTarget* shadowRenderTarget = nullptr;
	ID3D11SamplerState* shadowSampler = nullptr;

private:
	void RenderShadowScene(const RenderPassContext& ctx);
};
