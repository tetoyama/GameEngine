#pragma once
#include "../IRenderPass.h"
#include <d3d11.h>
#include <vector>
#include <memory>

struct PixelShaderData;
struct VertexShaderData;

class CameraComponent;

class PhysXDebugPass: public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	ID3D11Buffer* pPhysicsDebugLineVB = nullptr;
	std::shared_ptr<PixelShaderData> m_LinePixelShader;
	std::shared_ptr<VertexShaderData> m_LineVertexShader;

	ID3D11ShaderResourceView* result = nullptr;

	int maxLineCount = 999999;
};
