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

//======================================================================
// WaveReflectionPass
// Waveコンポーネントの水面反射用レンダーターゲットを生成するパス。
// 水平面 Y = waterY でカメラを反転させてシーンを描画し、
// 結果の SRV を RenderableWave に渡す。
//======================================================================
class WaveReflectionPass : public IRenderPass {
public:
	void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) override;
	void Finalize() override;
	void Execute(const RenderPassContext& ctx) override;

	void SetInputs(ShadowMapPass* shadowMapPass);

	// 反射テクスチャ（PlayerPass/EditorPass から参照可能）
	RenderTarget* reflectionRT = nullptr;

private:
	ShadowMapPass* m_shadowMapPass = nullptr;

	std::shared_ptr<VertexShaderData> m_vertexShader;
	std::shared_ptr<PixelShaderData>  m_pixelShader;

	// 反射対象の Renderable（Waveは含めない）
	std::vector<IRenderable*> m_renderables;

	// 水面の Y 座標を WaveComponent エンティティから取得
	float GetWaterPlaneY(const RenderPassContext& ctx) const;
};
