// =======================================================================
// 
// RenderableParticle.h
// 
// =======================================================================
#pragma once
#include "../IRenderable.h"
class ComponentRegistry;

struct RenderPassContext;
struct SceneManagerContext;
class MeshRendererComponent;

class RenderableParticle: public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override;
	void Finalize() override;

	void Execute(
		const RenderPassContext& ctx,
		SceneContext* sceneContext,
		const Entity& entity
	)override;

private:
	MeshRendererComponent* m_billBoardMesh = nullptr;
};
