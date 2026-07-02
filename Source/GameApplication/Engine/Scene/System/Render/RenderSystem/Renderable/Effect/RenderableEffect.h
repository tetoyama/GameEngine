#pragma once
#include "../IRenderable.h"

struct RenderPassContext;
struct SceneManagerContext;

class RenderableEffect : public IRenderable {
public:
	void Initialize(SceneManagerContext* context) override { m_context = context; }
	void Finalize() override {}
	void Execute(const RenderPassContext& ctx, const RenderPacket& packet) override;

private:
	SceneManagerContext* m_context = nullptr;
};
