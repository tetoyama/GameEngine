#pragma once
#include <d3d11.h>

struct RenderPassContext;
class ComponentRegistry;

class IRenderPass {
public:
	virtual ~IRenderPass(){}
	virtual void Execute(
		const RenderPassContext& ctx,
		ComponentRegistry* registry,
		ID3D11DeviceContext* deviceContext
	) = 0;
};