#pragma once

class RenderSystem;
struct SceneManagerContext;

struct RenderPassContext;

class IRenderPass {
public:
	virtual ~IRenderPass() = default;

	virtual void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) = 0;
	virtual void Finalize() = 0;

	virtual void Execute(const RenderPassContext& ctx) = 0;

	SceneManagerContext* m_context = nullptr;
	RenderSystem* m_renderSystem = nullptr;
};
