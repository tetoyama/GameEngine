#pragma once
#include "../IRenderPass.h"
#include <d3d11.h>
#include <vector>
#include <memory>

class IRenderable;
struct RenderTarget;

class ShadowMapPass;
class EffectPass;

class PlayerPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	std::vector<IRenderable*> renderables;
	RenderTarget* playerRenderTarget = nullptr;

	ID3D11ShaderResourceView* result = nullptr;

	ShadowMapPass* shadowMapPass;
	EffectPass* effectPass;
};
