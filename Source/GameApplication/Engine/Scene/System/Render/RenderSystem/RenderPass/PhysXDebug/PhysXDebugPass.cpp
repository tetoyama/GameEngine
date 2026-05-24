// =======================================================================
// 
// PhysXDebugPass.cpp
// 
// =======================================================================
#include "PhysXDebugPass.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "sceneManager.h"
#include "../RenderPassContext.h"
#include "../../renderPhase.h"
#include "Component/CameraComponent.h"
#include "System/Render/RenderSystem/renderLayer.h"
#include "Registry/systemRegistry.h"
#include <System/Physic/physicSystem.h>

void PhysXDebugPass::Initialize(RenderSystem* renderSystem, SceneManagerContext* context){

	m_pContext = pContext;

	m_LineVertexShader = m_pContext->pResource->Load<VertexShaderData>("Asset\\Shader\\DebugLineVS.cso");
	m_LinePixelShader = m_pContext->pResource->Load<PixelShaderData>("Asset\\Shader\\DebugLinePS.cso");

	D3D11_BUFFER_DESC m_Bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX_3D) * maxLineCount * 2;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT m_Hr= pContext->pGraphics->GetDevice()->CreateBuffer(&bd, nullptr, &pPhysicsDebugLineVB);
	if(FAILED(hr)){
		throw std::runtime_error("Failed to create physics debug line vertex buffer.");
	}
}

void PhysXDebugPass::Finalize(){

	pPhysicsDebugLineVB->Release();
	pPhysicsDebugLineVB = nullptr;

	m_LinePixelShader.reset();
	m_LineVertexShader.reset();
}

void PhysXDebugPass::Execute(const RenderPassContext& ctx){
	if (!m_LineVertexShader || !m_LinePixelShader) return;

	auto m_GraphicsContext= m_pContext->pGraphics;
	//graphicsContext->SetDepthMode(DepthMode::Disable);

	if(ctx.renderLayerVisibility[(int)RenderLayer::Debug]){

		//return;

		auto m_Physics= m_pContext->pSystemRegistry->GetSystem<PhysicSystem>();
		const physx::PxRenderBuffer& rb = physics->GetRenderBuffer();

		// 色変換関数
		auto m_ConvertColor= [](physx::PxU32 c) {
			float m_A= ((c >> 24) & 0xFF) / 255.0f;
			float m_R= ((c >> 16) & 0xFF) / 255.0f;
			float m_G= ((c >> 8) & 0xFF) / 255.0f;
			float m_B= ((c >> 0) & 0xFF) / 255.0f;
			return DirectX::XMFLOAT4(r, g, b, a);
		};

		std::vector<VERTEX_3D> m_Vertices;
		for (physx::PxU32 i = 0; i < rb.getNbLines(); i++) {
			const physx::PxDebugLine& line = rb.getLines()[i];

			VERTEX_3D m_V0;
			v0.Position = DirectX::XMFLOAT3(line.pos0.x, line.pos0.y, line.pos0.z);
			v0.Diffuse = ConvertColor(line.color0);

			VERTEX_3D m_V1;
			v1.Position = DirectX::XMFLOAT3(line.pos1.x, line.pos1.y, line.pos1.z);
			v1.Diffuse = ConvertColor(line.color1);

			vertices.push_back(v0);
			vertices.push_back(v1);
		}

		if (vertices.empty() || vertices.size() >= maxLineCount) return;

		ID3D11Device* device = graphicsContext->GetDevice();
		ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

		// 頂点バッファ更新
		D3D11_MAPPED_SUBRESOURCE m_Mapped;
		deviceContext->Map(pPhysicsDebugLineVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, vertices.data(), sizeof(VERTEX_3D) * vertices.size());
		deviceContext->Unmap(pPhysicsDebugLineVB, 0);

		graphicsContext->SetWorldMatrix(DirectX::XMMatrixIdentity());

		UINT m_Stride= sizeof(VERTEX_3D);
		UINT m_Offset= 0;
		deviceContext->IASetVertexBuffers(0, 1, &pPhysicsDebugLineVB, &stride, &offset);
		deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

		// シェーダーをセット（通常のカラー付き頂点用のものを使用）
		deviceContext->VSSetShader(m_LineVertexShader->m_VertexShader.Get(), nullptr, 0);
		deviceContext->PSSetShader(m_LinePixelShader->m_PixelShader.Get(), nullptr, 0);

		// 描画
		deviceContext->Draw(static_cast<UINT>(vertices.size()), 0);
	}

	//graphicsContext->SetDepthMode(DepthMode::Write);
}
