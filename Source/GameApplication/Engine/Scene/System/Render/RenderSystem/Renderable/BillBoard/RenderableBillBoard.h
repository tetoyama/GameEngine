// =======================================================================
// 
// RenderableBillBoard.h
// 
// =======================================================================
#pragma once
#include "../IRenderable.h"
class ComponentRegistry;

struct RenderPassContext;
struct SceneManagerContext;
class meshRendererComponent;

// ビルボードの描画を行うレンダラブル
class RenderableBillBoard: public IRenderable{
public:
	void Initialize(SceneManagerContext* context) override;
	void Finalize() override;

	void Execute(
		const RenderPassContext& ctx,
		SceneContext* sceneContext,
		const Entity& entity
	)override;

private:
	MeshRendererComponent* m_pBillBoardMesh = nullptr;
};
