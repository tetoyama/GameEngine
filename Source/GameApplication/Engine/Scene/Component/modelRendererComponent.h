#pragma once
#include "IComponent.h"
#include <memory>

struct ModelData;
struct PixelShaderData;
struct VertexShaderData;

class ModelRendererComponent: public IComponent {
public:
	ModelRendererComponent() = default;
	~ModelRendererComponent() = default;

	std::shared_ptr<ModelData> model;
	PixelShaderData* pixelShader = nullptr;
	VertexShaderData* vertexShader = nullptr;
};
