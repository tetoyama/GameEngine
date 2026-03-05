// =======================================================================
// 
// ForwardPass.h
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

class GBufferPass;
class LightingPass;
class ShadowMapPass;

// フォワードレンダリングパス（透過オブジェクト等）
class ForwardPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	void SetInputs(LightingPass* lightingPass, GBufferPass* gBufferPass, ShadowMapPass* shadowMapPass);

	std::vector<IRenderable*> renderables;
	std::shared_ptr<VertexShaderData> m_VertexShader;

private:
	LightingPass*  m_lightingPass  = nullptr;
	GBufferPass*   m_gBufferPass   = nullptr;
	ShadowMapPass* m_shadowMapPass = nullptr;
};
