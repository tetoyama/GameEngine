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

// パーティクルの描画を行うレンダラブル
class RenderableParticle: public IRenderable {
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
