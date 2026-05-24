// =======================================================================
// 
// RenderableMesh.h
// 
// =======================================================================
#pragma once
#include "../IRenderable.h"
class ComponentRegistry;

struct RenderPassContext;
struct SceneManagerContext;
// メッシュの描画を行うレンダラブル
class RenderableMesh: public IRenderable{
public:
	void Initialize(SceneManagerContext* context) override{}
	void Finalize() override{}

	void Execute(
		const RenderPassContext& ctx,
		SceneContext* sceneContext,
		const Entity& entity
	)override;
};
