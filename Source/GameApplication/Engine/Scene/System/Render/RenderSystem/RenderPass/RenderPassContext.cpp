#include "RenderPassContext.h"
#include "Component/transformComponent.h"
#include "Component/cameraComponent.h"

RenderPassContext::RenderPassContext(const RenderPhase& renderPass, bool* renderLayer, const CameraEntityData& data, const Vector2& setScreenSize) {
	for(int i = 0; i < RenderLayer::MaxRenderLayer; i++){
		renderLayerVisibility[i] = renderLayer[i];
	}
	passPhase = renderPass;
	cameraData = data;
	screenSize = setScreenSize;
	if (data.cameraComponent && data.transformComponent) {
		DirectX::XMVECTOR camPos = data.transformComponent->position.ToXMVECTOR();
		DirectX::XMStoreFloat4(&cameraPosition, camPos);
		projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(data.cameraComponent->FOV, screenSize.x / screenSize.y, data.cameraComponent->NearClip, data.cameraComponent->FarClip);
		viewMatrix = data.cameraComponent->viewMatrix;
	}
}
