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
class meshRendererComponent;

// パーティクルの描画を行うレンダラブル
class RenderableParticle: public irenderable{
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
