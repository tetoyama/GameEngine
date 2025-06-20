#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

#include "Backends/myVector3.h"
#include <DirectXMath.h>

class CameraComponent : public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		node["Component"] = "CameraComponent";

		node["isLock"] = isLock;
		node["Target"] = Target;
		node["NearClip"] = NearClip;
		node["FarClip"] = FarClip;
		node["FOV"] = FOV;
		node["viewMatrix"] = viewMatrix;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}

	bool isLock = false;
	Vector3 Target = Vector3(0.0f,0.0f,0.0f);

	float NearClip = 0.01f;
	float FarClip = 256.0f;

	float FOV = 1.0f;

	DirectX::XMMATRIX viewMatrix{};
};
