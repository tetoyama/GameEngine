// =======================================================================
// 
// RenderableSprite.h
// 
// =======================================================================
#pragma once
#include "../IRenderable.h"

struct RenderPassContext;
struct SceneManagerContext;
class MeshRendererComponent;

// 2Dスプライトの描画を行うレンダラブル
class RenderableSprite: public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override;
	void Finalize() override;

	void Execute(
		const RenderPassContext& ctx,
		const RenderPacket& packet
	) override;

private:
	MeshRendererComponent* m_spriteMesh = nullptr;
};
