// =======================================================================
// 
// RenderableModel.h
// 
// =======================================================================
#pragma once
#include <d3d11.h>
#include "../IRenderable.h"

struct RenderPassContext;
struct SceneManagerContext;

// 3Dモデルの描画を行うレンダラブル
class RenderableModel :public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override{}
	void Finalize() override{}

	void Execute(
		const RenderPassContext& ctx,
		const RenderPacket& packet
	) override;
};
