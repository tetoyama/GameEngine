#pragma once
#include "../IRenderPass.h"

class GBufferPass : public IRenderPass {
public:
	void Initialize(SceneManagerContext* context) override;
	void Finalize() override;
	void Execute() override;
};
