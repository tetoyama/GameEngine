// Engine/Scene/System/renderSystem.h
#pragma once
#include "ISystem.h"

class EntityRegistry;
class MainRenderer;

class RenderSystem : public ISystem{
public:
	RenderSystem(EntityRegistry* registry, MainRenderer* renderer)
		: m_registry(registry), m_renderer(renderer){}
	~RenderSystem(){}

	// •`‰æˆ—
	void Initialize() override;
	void Draw() override;
	void Finalize()override;

private:
	EntityRegistry* m_registry;
	MainRenderer* m_renderer;

	int m_TextureID = -1;
	ID3D11Buffer* m_VertexBuffer;
	ID3D11VertexShader* m_VertexShader;
	ID3D11PixelShader* m_PixelShader;
	ID3D11InputLayout* m_VertexLayout;
};
