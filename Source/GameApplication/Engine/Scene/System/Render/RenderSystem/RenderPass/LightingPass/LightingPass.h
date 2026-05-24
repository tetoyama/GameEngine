// =======================================================================
// 
// LightingPass.h
// 
// =======================================================================
#pragma once
#include "../IRenderPass.h"

#include <d3d11.h>
#include <vector>
#include <memory>

class IRenderable;
struct RenderTarget;
struct TextureData;

class gbufferPass;
class shadowMapPass;

class graphicsContext;

struct PixelShaderData;
struct VertexShaderData;

// ディファードライティング計算パス
class LightingPass : public irenderPass{
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	void SetTextureSlot(GBufferPass* gBufferPass, ShadowMapPass* shadowMapPass, GraphicsContext* gc);

	std::shared_ptr<VertexShaderData> lightingVertexShader;

	ID3D11SamplerState* m_LinearSampler;
	ID3D11SamplerState* m_EnvMapSampler = nullptr;

	std::shared_ptr<TextureData> environmentMap;

	RenderTarget* pRenderTarget;
};
