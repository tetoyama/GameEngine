#pragma once
#include "../IRenderPass.h"
#include <d3d11.h>
#include <vector>
#include <memory>

class IRenderable;
struct RenderTarget;

struct PixelShaderData;
struct VertexShaderData;

class ShadowMapPass;
class EffectPass;
class GBufferPass;
class LightingPass;

class PlayerPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	std::vector<IRenderable*> renderables;
	RenderTarget* playerRenderTarget = nullptr;

	std::shared_ptr<PixelShaderData> m_RenderablePixelShader;
	std::shared_ptr<VertexShaderData> m_RenderableVertexShader;

	ID3D11ShaderResourceView* result = nullptr;

	ShadowMapPass* shadowMapPass;
	EffectPass* effectPass;
	GBufferPass* gBufferPass;
	LightingPass* lightingPass;
};
