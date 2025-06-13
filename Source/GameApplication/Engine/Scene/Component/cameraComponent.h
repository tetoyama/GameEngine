#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"
#include <DirectXMath.h>

class CameraComponent : public IComponent {
public:
	bool isLock = false;
	Vector3 Target = Vector3(0.0f,0.0f,0.0f);

	float NearClip = 0.01f;
	float FarClip = 256.0f;

	float FOV = 1.0f;

	DirectX::XMMATRIX viewMatrix{};
};
