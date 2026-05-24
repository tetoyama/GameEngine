// =======================================================================
// 
// EditorPass.h
// 
// =======================================================================
#pragma once
#include "../IRenderPass.h"
#include <d3d11.h>
#include <memory>

struct RenderTarget;

class shadowMapPass;
class gbufferPass;
class lightingPass;
class forwardPass;
class postEffectPass;
class overlayUipass;
class physXdebugPass;

// エディタビュー用の統合レンダリングパス
class EditorPass : public irenderPass{
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	ID3D11ShaderResourceView* pResult = nullptr;

	ShadowMapPass*  pShadowMapPass  = nullptr;
	GBufferPass*    pGBufferPass    = nullptr;
	LightingPass*   pLightingPass   = nullptr;
	ForwardPass*    pForwardPass    = nullptr;
	PostEffectPass* pPostEffectPass = nullptr;
	OverlayUIPass*  pOverlayUIPass  = nullptr;
	PhysXDebugPass* pPhysXDebugPass = nullptr;
};
