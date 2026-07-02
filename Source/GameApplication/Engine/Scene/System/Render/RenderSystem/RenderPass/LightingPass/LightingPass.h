// =======================================================================
// 
// LightingPass.h
// 
// =======================================================================
#pragma once
#include "../IRenderPass.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

class IRenderable;
struct RenderTarget;
struct TextureData;

class GBufferPass;
class ShadowMapPass;

class GraphicsContext;

struct PixelShaderData;
struct VertexShaderData;

// ディファードライティング計算パス
class LightingPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	void SetTextureSlot(GBufferPass* gBufferPass, ShadowMapPass* shadowMapPass, GraphicsContext* gc);

	std::shared_ptr<VertexShaderData> m_LightingVertexShader;

	ID3D11SamplerState* m_LinearSampler = nullptr;
	ID3D11SamplerState* m_EnvMapSampler = nullptr;

	std::shared_ptr<TextureData> m_EnvironmentMap;

	RenderTarget* pRenderTarget = nullptr;

private:
	GBufferPass* m_activeGBufferPass = nullptr;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_lightingDebugBuffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_materialStencilTestState;
};
