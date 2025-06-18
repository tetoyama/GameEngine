#pragma once
#include "Interface/IComponent.h"

struct ModelData;
struct PixelShaderData;
struct VertexShaderData;

class ModelRendererComponent: public IComponent {
public:

	ModelRendererComponent() = default;
	~ModelRendererComponent() = default;
	YAML::Node encode() override{
		YAML::Node node;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}
	ModelData* model = nullptr;
	PixelShaderData* pixelShader = nullptr;
	VertexShaderData* vertexShader = nullptr;
};
