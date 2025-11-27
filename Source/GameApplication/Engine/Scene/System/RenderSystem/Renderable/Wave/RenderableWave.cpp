#include "RenderableWave.h"

#include <d3d11.h>
#include "../RenderableContext.h"

#include "GameApplication/Engine/DebugTools/DebugSystem.h"

#include "GameApplication/Engine/Scene/scene.h"
#include "GameApplication/Engine/Scene/sceneManager.h"
#include "GameApplication/Engine/Scene/Registry/componentRegistry.h"

#include "GameApplication/Engine/Scene/Component/waveComponent.h"
#include "GameApplication/Engine/Scene/Component/meshRendererComponent.h"
#include "GameApplication/Engine/Scene/Component/transformComponent.h"
#include "GameApplication/Engine/Scene/Component/textureComponent.h"


void RenderableWave::Execute(const RenderableContext& ctx, SceneContext* sceneContext, const Entity& entity) {
	auto componentRegistry = sceneContext->component;
	auto pTransform = componentRegistry->GetComponent<TransformComponent>(entity);
	auto pWave = componentRegistry->GetComponent<WaveComponent>(entity);

	if (!pWave || !pWave->meshRenderer) {
		return;
	}

	auto meshRenderer = pWave->meshRenderer;
	auto transform = pTransform;

	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, componentRegistry);

	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetIndexBuffer(*meshRenderer->mesh.m_IndexBuffer.GetAddressOf(), DXGI_FORMAT_R32_UINT, 0);

	deviceContext->DrawIndexed(meshRenderer->mesh.indexCount, 0, 0);

	graphicsContext->SetDepthEnable(true);
	graphicsContext->SetViewMatrix(ctx.viewMatrix);
	graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}
