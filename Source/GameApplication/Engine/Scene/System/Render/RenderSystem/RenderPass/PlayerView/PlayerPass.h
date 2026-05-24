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

class shadowMapPass;
class gbufferPass;
class lightingPass;
class forwardPass;
class postEffectPass;
class overlayUipass;

// プレイヤービュー用の統合レンダリングパス
class PlayerPass : public irenderPass{
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	RenderTarget* playerRenderTarget = nullptr;

	ID3D11ShaderResourceView* result = nullptr;

	ShadowMapPass*  shadowMapPass  = nullptr;
	GBufferPass*    gBufferPass    = nullptr;
	LightingPass*   lightingPass   = nullptr;
	ForwardPass*    forwardPass    = nullptr;
	PostEffectPass* postEffectPass = nullptr;
	OverlayUIPass*  overlayUIPass = nullptr;
};
