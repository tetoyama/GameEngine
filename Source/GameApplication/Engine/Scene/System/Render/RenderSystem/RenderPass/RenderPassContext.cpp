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
	if (data.pCameraComponent && data.pTransformComponent) {
		DirectX::XMVECTOR m_CamPos= data.pTransformComponent->position.ToXMVECTOR();
		DirectX::XMStoreFloat4(&CameraPosition, camPos);
		projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(data.pCameraComponent->FOV, screenSize.x / screenSize.y, data.pCameraComponent->NearClip, data.pCameraComponent->FarClip);
		viewMatrix = data.pCameraComponent->viewMatrix;
	}
}
