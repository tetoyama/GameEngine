// =======================================================================
// 
// RenderableWave.h
// 
// =======================================================================
#pragma once
#include "../IRenderable.h"
class ComponentRegistry;

struct RenderableContext;
struct SceneManagerContext;
// 波メッシュの描画を行うレンダラブル
class RenderableWave : public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override {}
	void Finalize() override {}

	void Execute(
		const RenderPassContext& ctx,
		const RenderPacket& packet
	) override;
};
