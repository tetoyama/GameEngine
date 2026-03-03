// =======================================================================
// 
// PostEffectPass.h
// 
// =======================================================================
#pragma once
#include "../IRenderPass.h"
#include <d3d11.h>

class GBufferPass;

class PostEffectPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	void SetInputs(ID3D11ShaderResourceView* initialSRV,ID3D11RenderTargetView** initialRTV,  GBufferPass* gBufferPass);

	ID3D11ShaderResourceView* resultSrv = nullptr;
	ID3D11RenderTargetView** resultRtv = nullptr;

private:
	ID3D11ShaderResourceView* m_initialSRV  = nullptr;
	ID3D11RenderTargetView** m_initialRTV = nullptr;
	GBufferPass*              m_gBufferPass = nullptr;
};
