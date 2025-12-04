#pragma once

struct SceneManagerContext;
struct RenderableContext;

class IRenderPass {
public:
	virtual ~IRenderPass() = default;

	virtual void Initialize(SceneManagerContext* context) = 0;
	virtual void Finalize() = 0;

	virtual void Execute() = 0;

	SceneManagerContext* m_context = nullptr;
};
