// =======================================================================
// 
// IRenderable.h
// 
// =======================================================================
#pragma once
#include "System/Render/RenderSystem/RenderPacket/RenderPacket.h"

struct RenderPassContext;
struct SceneManagerContext;


// 描画オブジェクトの基底インターフェース
class IRenderable {
public:
	virtual ~IRenderable(){}

	virtual void Initialize(SceneManagerContext* context) = 0;
	virtual void Finalize() = 0;

	virtual void Execute(
		const RenderPassContext& ctx,
		const RenderPacket& packet
	) = 0;
};
