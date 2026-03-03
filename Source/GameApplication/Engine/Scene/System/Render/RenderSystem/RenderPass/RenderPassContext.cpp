// =======================================================================
// 
// RenderPassContext.cpp
// 
// =======================================================================
#include "RenderPassContext.h"
#include "Component/transformComponent.h"
#include "Component/CameraComponent.h"

RenderPassContext::RenderPassContext(const RenderPhase& renderPass, bool* renderLayer, const CameraEntityData& data, const Vector2& setScreenSize) {
	for(int i = 0; i < RenderLayer::MaxRenderLayer; i++){
		renderLayerVisibility[i] = renderLayer[i];
	}
	passPhase = renderPass;
	cameraData = data;
	screenSize = setScreenSize;
	if (data.CameraComponent && data.transformComponent) {
		DirectX::XMVECTOR camPos = data.transformComponent->position.ToXMVECTOR();
		DirectX::XMStoreFloat4(&CameraPosition, camPos);
		projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(data.CameraComponent->FOV, screenSize.x / screenSize.y, data.CameraComponent->NearClip, data.CameraComponent->FarClip);
		viewMatrix = data.CameraComponent->viewMatrix;
	}
}
