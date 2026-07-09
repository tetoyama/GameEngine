// =======================================================================
// 
// PlayerPass.h
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

class ShadowMapPass;
class GBufferPass;
class LightingPass;
class ForwardPass;
class PostEffectPass;
class OverlayUIPass;

// プレイヤービュー用の統合レンダリングパス
class PlayerPass : public IRenderPass {
public:
	PlayerPass();
	~PlayerPass() override;

	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	std::unique_ptr<RenderTarget> playerRenderTarget;

	ID3D11ShaderResourceView* result = nullptr;

	std::unique_ptr<ShadowMapPass> shadowMapPass;
	std::unique_ptr<GBufferPass> gBufferPass;
	std::unique_ptr<LightingPass> lightingPass;
	std::unique_ptr<ForwardPass> forwardPass;
	std::unique_ptr<PostEffectPass> postEffectPass;
	std::unique_ptr<OverlayUIPass> overlayUIPass;
};