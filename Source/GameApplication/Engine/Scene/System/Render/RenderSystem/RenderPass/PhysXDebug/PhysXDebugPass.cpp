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

	m_context = context;

	m_LineVertexShader = m_context->resource->Load<VertexShaderData>("Asset\\Shader\\DebugLineVS.cso");
	m_LinePixelShader = m_context->resource->Load<PixelShaderData>("Asset\\Shader\\DebugLinePS.cso");

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX_3D) * maxLineCount * 2;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = context->graphics->GetDevice()->CreateBuffer(&bd, nullptr, &pPhysicsDebugLineVB);
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
	// プレイ中のみデバッグ描画を実行する
	if(m_context->sceneManager->State != SceneManagerState::Playing){
		return;
	}

	if(!ctx.renderLayerVisibility[(int)RenderLayer::Debug]){
		return;
	}

	auto physics = m_context->systemRegistry->GetSystem<PhysicSystem>();
	if(!physics) return;

	const physx::PxRenderBuffer& rb = physics->GetRenderBuffer();

	physx::PxU32 nbLines = (std::min)(rb.getNbLines(), static_cast<physx::PxU32>(maxLineCount));
	if(nbLines == 0) return;

	// 色変換関数（PhysX の ARGB 形式を XMFLOAT4 に変換）
	auto ConvertColor = [](physx::PxU32 c) -> DirectX::XMFLOAT4 {
		float a = ((c >> 24) & 0xFF) / 255.0f;
		float r = ((c >> 16) & 0xFF) / 255.0f;
		float g = ((c >>  8) & 0xFF) / 255.0f;
		float b = ((c >>  0) & 0xFF) / 255.0f;
		// アルファが 0 の場合は不透明として扱う
		if(a == 0.0f) a = 1.0f;
		return DirectX::XMFLOAT4(r, g, b, a);
	};

	// Normal/Tangent/TexCoord はデバッグライン描画では不使用のため 0 初期化のまま
	std::vector<VERTEX_3D> vertices;
	vertices.reserve(nbLines * 2);
	for(physx::PxU32 i = 0; i < nbLines; i++){
		const physx::PxDebugLine& line = rb.getLines()[i];

		VERTEX_3D v0{};
		v0.Position = DirectX::XMFLOAT3(line.pos0.x, line.pos0.y, line.pos0.z);
		v0.Diffuse  = ConvertColor(line.color0);

		VERTEX_3D v1{};
		v1.Position = DirectX::XMFLOAT3(line.pos1.x, line.pos1.y, line.pos1.z);
		v1.Diffuse  = ConvertColor(line.color1);

		vertices.push_back(v0);
		vertices.push_back(v1);
	}

	auto graphicsContext = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	// 頂点バッファ更新
	D3D11_MAPPED_SUBRESOURCE mapped;
	if(FAILED(deviceContext->Map(pPhysicsDebugLineVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))){
		return;
	}
	memcpy(mapped.pData, vertices.data(), sizeof(VERTEX_3D) * vertices.size());
	deviceContext->Unmap(pPhysicsDebugLineVB, 0);

	// レンダーステートを設定
	graphicsContext->SetDepthMode(DepthMode::Disable);
	graphicsContext->SetBlendMode(BlendMode::None);

	graphicsContext->SetWorldMatrix(DirectX::XMMatrixIdentity());
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &pPhysicsDebugLineVB, &stride, &offset);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// シェーダーをセット
	deviceContext->VSSetShader(m_LineVertexShader->m_VertexShader.Get(), nullptr, 0);
	deviceContext->IASetInputLayout(m_LineVertexShader->m_VertexLayout.Get());
	deviceContext->PSSetShader(m_LinePixelShader->m_PixelShader.Get(), nullptr, 0);

	// 描画
	deviceContext->Draw(static_cast<UINT>(vertices.size()), 0);

	// レンダーステートを元に戻す
	graphicsContext->SetDepthMode(DepthMode::Write);
}
