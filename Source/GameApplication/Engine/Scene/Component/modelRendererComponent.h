#pragma once
#include "Interface/IComponent.h"

struct ModelData;
struct PixelShaderData;
struct VertexShaderData;

class ModelRendererComponent: public IComponent {
public:
	ModelRendererComponent() = default;
	~ModelRendererComponent() = default;

	ModelData* model = nullptr;
	PixelShaderData* pixelShader = nullptr;
	VertexShaderData* vertexShader = nullptr;
};
