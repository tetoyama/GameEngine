#include "cameraSystem.h"

#include "Engine/Graphics/graphicsContext.h"
#include "Engine/Graphics/mainRenderer.h"
#include "Entity/entityRegistry.h"
#include "Component/cameraComponent.h"
#include "Backends/myVector3.h"

#include <DirectXMath.h>

void CameraSystem::Draw() {

	GraphicsContext* graphicsContext = m_renderer->GetGraphicsContext();
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();

	const auto& searchComponents = m_registry->GetComponents();

	for (const auto& typePair : searchComponents) {

		if (typePair.first == typeid(CameraComponent)) {
			for (const auto& entityPair : typePair.second) {

				Entity entity = entityPair.first;
				auto* cameraComponent = static_cast<CameraComponent*>(entityPair.second.get());

				if (!cameraComponent) {
					continue;
				}
				auto* transform = m_registry->GetComponent<TransformComponent>(entity);
				if (!transform) {
					continue;
				}

				DirectX::XMMATRIX projection;
				projection = DirectX::XMMatrixPerspectiveFovLH(1.0f, (float)graphicsContext->m_width / graphicsContext->m_height, 0.01f, 1000.0f);
				graphicsContext->SetProjectionMatrix(projection);

				Vector3 position = transform->position;

				graphicsContext->SetViewMatrix(DirectX::XMMatrixLookAtLH({ position.x,position.y,position.z}, { 0.0,0.0f,1.0f }, { 0.0f,1.0f,0.0f }));

				CAMERA camera{};
				camera.CameraPosition = { position.x,position.y,position.z,0.0f };
				graphicsContext->SetCamera(camera);
			}
		}
	}
}
