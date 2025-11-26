#include "RenderableContext.h"


#include "Component/cameraComponent.h"
#include "Component/transformComponent.h"

RenderableContext::RenderableContext(const RenderPhase& renderPass, bool* renderLayer, std::shared_ptr<PixelShaderData> setPixelShader, std::shared_ptr<VertexShaderData> setVertexShader, const CameraEntityData& data, const Vector2& setScreenSize){

	passPhase = renderPass;

	renderLayerVisibility = renderLayer;

	pixelShader = setPixelShader;
	vertexShader = setVertexShader;

	cameraData = data;

	screenSize = setScreenSize;

	CameraComponent* cameraComponent = cameraData.cameraComponent;
	TransformComponent* transformComponent = cameraData.transformComponent;

	if(cameraComponent == nullptr || transformComponent == nullptr){
		return;
	}

	// プロジェクションマトリクス設定
	projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(cameraComponent->FOV, screenSize.x / screenSize.y, cameraComponent->NearClip, cameraComponent->FarClip);

	cameraPosition = DirectX::XMFLOAT4(transformComponent->position.x, transformComponent->position.y, transformComponent->position.z, 0.0f);

	viewMatrix = cameraComponent->viewMatrix;
}
