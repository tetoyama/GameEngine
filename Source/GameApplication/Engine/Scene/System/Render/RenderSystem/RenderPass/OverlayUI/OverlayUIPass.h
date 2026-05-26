// =======================================================================
// 
// OverlayUIPass.h
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

// オーバーレイUI描画パス
class OverlayUIPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;
	void SetInputs(ID3D11RenderTargetView** setRTV, RenderTarget* setDSV);

	std::vector<IRenderable*> renderables;

private:
	std::shared_ptr<VertexShaderData> vertexShader;

	ID3D11RenderTargetView** m_rtv = nullptr;
	RenderTarget* m_dsv = nullptr;
};
