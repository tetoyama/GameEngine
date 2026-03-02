#pragma once
#include "../IRenderable.h"
class ComponentRegistry;

struct RenderableContext;
struct SceneManagerContext;

struct ID3D11ShaderResourceView;

class RenderableWave : public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override {}
	void Finalize() override {}

	void Execute(
		const RenderPassContext& ctx,
		SceneContext* sceneContext,
		const Entity& entity
	)override;

	// WaveReflectionPass が毎フレームセットする反射テクスチャ SRV
	ID3D11ShaderResourceView* reflectionSRV = nullptr;
};
