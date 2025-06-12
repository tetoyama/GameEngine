#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"
#include <DirectXMath.h>

class CameraComponent : public IComponent {
public:
	bool isLock = false;
	Vector3 Target = Vector3(0.0f,0.0f,0.0f);

	DirectX::XMMATRIX viewMatrix{};
};
