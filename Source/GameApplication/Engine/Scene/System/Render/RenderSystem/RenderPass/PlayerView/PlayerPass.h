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
class PlayerPass : public IRenderPass{
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	RenderTarget* pPlayerRenderTarget = nullptr;

	ID3D11ShaderResourceView* pResult = nullptr;

	ShadowMapPass*  pShadowMapPass  = nullptr;
	GBufferPass*    pGBufferPass    = nullptr;
	LightingPass*   pLightingPass   = nullptr;
	ForwardPass*    pForwardPass    = nullptr;
	PostEffectPass* pPostEffectPass = nullptr;
	OverlayUIPass*  pOverlayUIPass = nullptr;
};
