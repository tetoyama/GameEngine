#pragma once
#include "../IRenderable.h"
#include <memory>

class ComponentRegistry;
struct PixelShaderData;

struct RenderableContext;
struct SceneManagerContext;
class RenderableWave : public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override;
	void Finalize() override;

	void Execute(
		const RenderPassContext& ctx,
		SceneContext* sceneContext,
		const Entity& entity
	)override;

private:
	SceneManagerContext* m_context = nullptr;
	std::shared_ptr<PixelShaderData> m_wavePS;
};
