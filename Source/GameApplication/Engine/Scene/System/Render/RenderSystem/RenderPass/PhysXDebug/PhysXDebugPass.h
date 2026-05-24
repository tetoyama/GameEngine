// =======================================================================
// 
// PhysXDebugPass.h
// 
// =======================================================================
#pragma once
#include "../IRenderPass.h"
#include <d3d11.h>
#include <vector>
#include <memory>

struct PixelShaderData;
struct VertexShaderData;

class cameraComponent;

// PhysXデバッグライン描画パス
class PhysXDebugPass: public irenderPass{
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	ID3D11Buffer* pPhysicsDebugLineVB = nullptr;
	std::shared_ptr<PixelShaderData> linePixelShader;
	std::shared_ptr<VertexShaderData> lineVertexShader;

	ID3D11ShaderResourceView* pResult = nullptr;

	int maxLineCount = 999999;
};
