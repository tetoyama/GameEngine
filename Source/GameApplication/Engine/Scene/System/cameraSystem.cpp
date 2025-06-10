#include "cameraSystem.h"

#include <DirectXMath.h>
#include "Backends/myVector3.h"
#include "Engine/DebugTools/debugSystem.h"

#include "Engine/Graphics/graphicsContext.h"
#include "Engine/Graphics/mainRenderer.h"

#include "Scene.h"
#include "sceneManager.h"

#include "Registry/entityRegistry.h"
#include "Registry/componentRegistry.h"

#include "Component/cameraComponent.h"
#include "Component/transformComponent.h"

void CameraSystem::Initialize(){
	m_context->manager->debug->LOG_DEBUG(u8"CameraSystemを初期化中...");
}

void CameraSystem::Update(float deltaTime) {

	// コンテキストの取得
	GraphicsContext* graphicsContext = m_context->manager->renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	// カメラコンポーネントを持つエンティティ取得
	auto entities = m_context->component->FindEntitiesWithComponent<CameraComponent>();
	if (entities.empty()) {
		//取得に失敗
		return;
	}

	// カメラコンポーネントの取得
	auto cameraComponent = m_context->component->GetComponent<CameraComponent>(entities[0]);
	// トランスフォームの取得
	auto transformComponent = m_context->component->GetComponent<TransformComponent>(entities[0]);

	// プロジェクションマトリクス設定
	DirectX::XMMATRIX projection;
	projection = DirectX::XMMatrixPerspectiveFovLH(1.0f, (float)graphicsContext->m_width / graphicsContext->m_height, 0.01f, 1000.0f);
	graphicsContext->SetProjectionMatrix(projection);

	// コンスタントバッファ設定
	Vector3 position = transformComponent->position;
	CAMERA camera{};
	camera.CameraPosition = { position.x,position.y,position.z,0.0f };
	graphicsContext->SetCamera(camera);

	// ビューマトリクス設定
	if (cameraComponent->isLock) {

		Vector3 target = cameraComponent->Target;
		graphicsContext->SetViewMatrix(DirectX::XMMatrixLookAtLH({ position.x,position.y,position.z }, { target.x,target.y,target.z }, { 0.0f,1.0f,0.0f }));

	} else {

		Vector3 front = transformComponent->position + transformComponent->front();
		Vector3 up = transformComponent->position + transformComponent->up();

		graphicsContext->SetViewMatrix(DirectX::XMMatrixLookAtLH({ position.x,position.y,position.z }, { front.x,front.y,front.z }, { up.x,up.y,up.z }));
	}
}
