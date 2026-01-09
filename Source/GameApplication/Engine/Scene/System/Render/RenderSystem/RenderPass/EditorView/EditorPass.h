#pragma once
#include "../IRenderPass.h"
#include <d3d11.h>
#include <vector>
#include <memory>

class IRenderable;
struct RenderTarget;

class ShadowMapPass;
class EffectPass;
class PhysXDebugPass;


struct PixelShaderData;
struct VertexShaderData;

class EditorPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	std::vector<IRenderable*> renderables;
	RenderTarget* editorRenderTarget = nullptr;

	std::shared_ptr<PixelShaderData> m_RenderablePixelShader;
	std::shared_ptr<VertexShaderData> m_RenderableVertexShader;

	ID3D11ShaderResourceView* result = nullptr;

	ShadowMapPass* shadowMapPass;
	EffectPass* effectPass;
	PhysXDebugPass* physXDebugPass;
};
