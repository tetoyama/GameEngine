// Engine/Scene/Component/meshRendererComponent.h
#pragma once
#include "Component.h"
#include <string>
#include <memory>
#include <wrl/client.h>
#include <d3d11.h>
#include "Engine/Resources/TextureLoader.h"


class GraphicsContext;

struct MeshData {
	int m_TextureID = -1;
	ID3D11Buffer* m_VertexBuffer = nullptr;
	ID3D11Buffer* m_IndexBuffer = nullptr;
	ID3D11VertexShader* m_VertexShader = nullptr;
	ID3D11PixelShader* m_PixelShader = nullptr;
	ID3D11InputLayout* m_VertexLayout = nullptr;
	int meshCount = 0;
	int indexCount = 0;

	~MeshData() {
		if(m_VertexBuffer){
			m_VertexBuffer->Release();
		}
		if(m_IndexBuffer){
			m_IndexBuffer->Release();
		}
		if(m_VertexShader){
			m_VertexShader->Release();
		}
		if(m_PixelShader){
			m_PixelShader->Release();
		}
		if(m_VertexLayout){
			m_VertexLayout->Release();
		}
		if(0 <= m_TextureID){
			UnloadTexture(m_TextureID);
		}
	}
};

class MeshRendererComponent: public Component {
public:
	MeshRendererComponent() = default;
	virtual ~MeshRendererComponent() = default;

	std::shared_ptr<MeshData> mesh;
};
