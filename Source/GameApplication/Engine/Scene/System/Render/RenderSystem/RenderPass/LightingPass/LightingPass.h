#pragma once
#include "../IRenderPass.h"

#include <d3d11.h>
#include <vector>
#include <memory>

class IRenderable;
struct RenderTarget;

class GBufferPass;
class ShadowMapPass;

class GraphicsContext;

struct PixelShaderData;
struct VertexShaderData;

class LightingPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	void ReloadShader();

	void SetTextureSlot(GBufferPass* gBufferPass, ShadowMapPass* shadowMapPass, GraphicsContext* gc);

	std::shared_ptr<PixelShaderData> m_LightingPixelShader;
	std::shared_ptr<VertexShaderData> m_LightingVertexShader;

	ID3D11SamplerState* m_LinearSampler;

	RenderTarget* pRenderTarget;
};
