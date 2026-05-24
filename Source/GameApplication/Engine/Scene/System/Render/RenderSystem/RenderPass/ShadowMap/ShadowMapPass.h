// =======================================================================
// 
// ShadowMapPass.h
// 
// =======================================================================
#pragma once
#include "../IRenderPass.h"
#include <d3d11.h>
#include <memory>
#include <vector>

class IRenderable;
struct RenderTarget;
struct PixelShaderData;

// シャドウマップ生成パス
class ShadowMapPass : public IRenderPass{
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	std::shared_ptr<PixelShaderData> renderablePixelShader;

	std::vector<IRenderable*> renderables;
	RenderTarget* pShadowRenderTarget = nullptr;
	ID3D11SamplerState* pShadowSampler = nullptr;
	ID3D11RasterizerState* pDepthClampRS = nullptr; // DepthClipEnable=FALSE: フラスタム外 shadow caster のクリッピングを防ぐ

private:
	void RenderShadowScene(const RenderPassContext& ctx);
};
