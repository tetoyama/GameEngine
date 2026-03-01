#pragma once
#include "../IRenderPass.h"
#include <d3d11.h>

class GBufferPass;

class PostEffectPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	void SetInputs(ID3D11ShaderResourceView* initialSRV, GBufferPass* gBufferPass);

	ID3D11ShaderResourceView* result = nullptr;

private:
	ID3D11ShaderResourceView* m_initialSRV  = nullptr;
	GBufferPass*              m_gBufferPass = nullptr;
};
