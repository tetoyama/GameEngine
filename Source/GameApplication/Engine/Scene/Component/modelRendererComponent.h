#pragma once
#include "Interface/IComponent.h"

#include "Service/YAMLConverters.h"
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"
#include "Engine/Resources/Data/modelData.h"

class ModelRendererComponent: public IComponent {
public:

	ModelRendererComponent() = default;
	~ModelRendererComponent() = default;
	YAML::Node encode() override{
		YAML::Node node;
		node["Component"] = "ModelRendererComponent";

		node["FilePath"] = model->FilePath;
		node["PixelShader"] = pixelShader->FilePath;
		node["VertexShader"] = vertexShader->FilePath;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}

	void inspector() override{
		ImGui::Text("ModelRendererComponent");
	}

	ModelData* model = nullptr;
	PixelShaderData* pixelShader = nullptr;
	VertexShaderData* vertexShader = nullptr;
};
