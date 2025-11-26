#pragma once
#include "../IRenderable.h"
class ComponentRegistry;

struct RenderableContext;
struct SceneManagerContext;
class MeshRendererComponent;

class RenderableBillBoard: public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override;
	void Finalize() override;

	void Execute(
		const RenderableContext& ctx,
		SceneContext* sceneContext,
		const Entity& entity
	)override;

private:
	MeshRendererComponent* m_billBoardMesh = nullptr;
};
