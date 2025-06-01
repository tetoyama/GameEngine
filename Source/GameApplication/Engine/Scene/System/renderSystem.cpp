// Engine/Scene/System/renderSystem.cpp
#include "renderSystem.h"
#include "Entity/entityRegistry.h"
#include "Component/transformComponent.h"
#include "Component/meshRendererComponent.h"
#include "Engine/Graphics/mainRenderer.h"
#include "DirectXMath.h"
#include "Engine/Resources/TextureLoader.h"

#define POLYGON_VERTEX (4)
#define POLYGON_SIZE (100)

void RenderSystem::Initialize(){

	VERTEX_3D vertex[POLYGON_VERTEX]{};

	vertex[0].Position = DirectX::XMFLOAT3(POLYGON_SIZE * 0.0f, POLYGON_SIZE * 0.0f, 0.0f);
	vertex[0].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
	vertex[0].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[0].TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);

	vertex[1].Position = DirectX::XMFLOAT3(POLYGON_SIZE * 1.0f, POLYGON_SIZE * 0.0f, 0.0f);
	vertex[1].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
	vertex[1].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[1].TexCoord = DirectX::XMFLOAT2(1.0f, 0.0f);

	vertex[2].Position = DirectX::XMFLOAT3(POLYGON_SIZE * 0.0f, POLYGON_SIZE * 1.0f, 0.0f);
	vertex[2].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
	vertex[2].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[2].TexCoord = DirectX::XMFLOAT2(0.0f, 1.0f);

	vertex[3].Position = DirectX::XMFLOAT3(POLYGON_SIZE * 1.0f, POLYGON_SIZE * 1.0f, 0.0f);
	vertex[3].Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
	vertex[3].Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[3].TexCoord = DirectX::XMFLOAT2(1.0f, 1.0f);

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VERTEX_3D) * POLYGON_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	m_renderer->GetGraphicsContext()->GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);
	m_renderer->GetGraphicsContext()->CreateVertexShader("Asset\\Shader\\unlitTextureVS.cso", &m_VertexShader,&m_VertexLayout);
	m_renderer->GetGraphicsContext()->CreatePixelShader("Asset\\Shader\\unlitTexturePS.cso", &m_PixelShader);
}

void RenderSystem::Draw(){
	m_renderer->GetGraphicsContext()->GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	m_renderer->GetGraphicsContext()->GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	m_renderer->GetGraphicsContext()->GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

	m_renderer->GetGraphicsContext()->SetWorldViewProjection2D();

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;

	m_renderer->GetGraphicsContext()->GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

	SetTexture(0);

	m_renderer->GetGraphicsContext()->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	m_renderer->GetGraphicsContext()->GetDeviceContext()->Draw(POLYGON_VERTEX, 0);


}

void RenderSystem::Finalize(){

	m_VertexBuffer->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
	m_VertexLayout->Release();
}
