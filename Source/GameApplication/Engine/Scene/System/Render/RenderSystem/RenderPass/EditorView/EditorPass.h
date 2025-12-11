#pragma once
#include "../IRenderPass.h"
#include <d3d11.h>
#include <vector>
#include <memory>

class IRenderable;
struct RenderTarget;

class ShadowMapPass;

struct PixelShaderData;
struct VertexShaderData;

class CameraComponent;

class EditorPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;
	ShadowMapPass* shadowMapPass;

	std::vector<IRenderable*> renderables;
	RenderTarget* playerRenderTarget = nullptr;

	ID3D11Buffer* pPhysicsDebugLineVB = nullptr;

	std::shared_ptr<PixelShaderData> m_LinePixelShader;
	std::shared_ptr<VertexShaderData> m_LineVertexShader;

	ID3D11ShaderResourceView* result = nullptr;
	int maxLineCount = 99999;

};
