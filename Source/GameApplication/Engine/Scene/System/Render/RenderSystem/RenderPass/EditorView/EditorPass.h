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

class ShadowMapPass;
class GBufferPass;
class LightingPass;
class ForwardPass;
class PostEffectPass;
class OverlayUIPass;
class PhysXDebugPass;

// エディタビュー用の統合レンダリングパス
class EditorPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	ID3D11ShaderResourceView* result = nullptr;

	ShadowMapPass*  shadowMapPass  = nullptr;
	GBufferPass*    gBufferPass    = nullptr;
	LightingPass*   lightingPass   = nullptr;
	ForwardPass*    forwardPass    = nullptr;
	PostEffectPass* postEffectPass = nullptr;
	OverlayUIPass*  overlayUIPass  = nullptr;
	PhysXDebugPass* physXDebugPass = nullptr;
};
