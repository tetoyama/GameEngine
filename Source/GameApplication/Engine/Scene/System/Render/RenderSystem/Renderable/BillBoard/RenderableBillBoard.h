// =======================================================================
// 
// RenderableBillBoard.h
// 
// =======================================================================
#pragma once
#include "../IRenderable.h"

struct RenderPassContext;
struct SceneManagerContext;
class MeshRendererComponent;

// ビルボードの描画を行うレンダラブル
class RenderableBillBoard: public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override;
	void Finalize() override;

	void Execute(
		const RenderPassContext& ctx,
		const RenderPacket& packet
	) override;

private:
	MeshRendererComponent* m_billBoardMesh = nullptr;
};
