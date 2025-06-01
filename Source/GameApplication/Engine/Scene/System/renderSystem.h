// Engine/Scene/System/renderSystem.h
#pragma once
#include "ISystem.h"
#include "d3d11.h"

class EntityRegistry;
class MainRenderer;

class RenderSystem : public ISystem{
public:
	RenderSystem(EntityRegistry* registry, MainRenderer* renderer)
		: m_registry(registry), m_renderer(renderer){
		Initialize();
	}
	~RenderSystem(){}

	// •`‰æˆ—
	void Initialize() override;
	void Draw() override;
	void Finalize()override;

private:
	EntityRegistry* m_registry = nullptr;
	MainRenderer* m_renderer = nullptr;

	int m_TextureID = -1;
	ID3D11Buffer* m_VertexBuffer = nullptr;
	ID3D11VertexShader* m_VertexShader = nullptr;
	ID3D11PixelShader* m_PixelShader = nullptr;
	ID3D11InputLayout* m_VertexLayout = nullptr;
};
