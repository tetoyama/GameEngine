#pragma once
#include <memory>


enum RenderPhase;

struct PixelShaderData;
struct VertexShaderData;

struct RenderPassContext {

	RenderPassContext(
		const RenderPhase& renderPass,
		bool* renderLayer,
		std::shared_ptr<PixelShaderData> setPixelShader,
		std::shared_ptr<VertexShaderData> setVertexShader,
		const CameraEntityData& data,
		const Vector2& setScreenSize
	);

	bool* renderLayerVisibility = nullptr;
	RenderPhase passPhase = PHASE_SHADOW;

	DirectX::XMFLOAT4 cameraPosition = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixIdentity();

	std::shared_ptr<PixelShaderData> pixelShader;
	std::shared_ptr<VertexShaderData> vertexShader;

	CameraEntityData cameraData;
	Vector2 screenSize = Vector2(1280.0f, 720.0f);
};
