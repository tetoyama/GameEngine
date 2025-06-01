// Engine/Scene/Component/meshRendererComponent.h
#pragma once
#include "Component.h"
#include <string>
#include <memory>
#include <wrl/client.h>
#include <d3d11.h>

struct MeshData {  
	int m_TextureID = -1;
	ID3D11Buffer* m_VertexBuffer = nullptr;
	ID3D11VertexShader* m_VertexShader = nullptr;
	ID3D11PixelShader* m_PixelShader = nullptr;
	ID3D11InputLayout* m_VertexLayout = nullptr;
};

class MeshRendererComponent: public Component {
public:
	MeshRendererComponent() = default;
	virtual ~MeshRendererComponent() = default;

	std::shared_ptr<MeshData> mesh;
};
