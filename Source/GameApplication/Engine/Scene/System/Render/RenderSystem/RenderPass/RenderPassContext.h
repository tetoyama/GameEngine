#pragma once
#include <memory>
#include <d3d11.h>

#include "Backends/myVector2.h"
#include "Resources/Data/shaderData.h"
#include "../renderPhase.h"
#include "../renderLayer.h"
#include "../cameraEntityData.h"

struct RenderPassContext {

	RenderPassContext(
		const RenderPhase& renderPhase,
		bool* renderLayer,
		const CameraEntityData& data,
		const Vector2& setScreenSize
	);

	bool renderLayerVisibility[RenderLayer::MaxRenderLayer];
	RenderPhase passPhase = RenderPhase::PHASE_SHADOW;

	DirectX::XMFLOAT4 cameraPosition = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixIdentity();

	CameraEntityData cameraData;
	Vector2 screenSize = Vector2(1280.0f, 720.0f);
};

struct TransparentDrawItem {
	Entity entity;
	float distanceSq;
	SceneContext* context;
};

struct SpriteDrawItem {
	Entity entity;
	int orderInLayer;
	SceneContext* context;
};
