#pragma once
#include "Interface/IComponent.h"

#include "Service/YAMLConverters.h"
#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"
#include "Engine/Resources/Data/modelData.h"
#include "Engine/Resources/Loader/modelLoader.h"
#include "Engine/Resources/Loader/shaderLoader.h"

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

	void inspector(SceneContext* context) override{
		ImGui::Text("ModelRendererComponent");

		static char filepathBuffer[256] = {0}; // 適当な最大長
		// バッファに現在の文字列をコピー（初回か変更時だけにすると効率的）
		if(model){
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), model->FilePath.c_str(), _TRUNCATE);
		}
		if(ImGui::InputText("Model File Path", filepathBuffer, sizeof(filepathBuffer))){
			// 編集されたら std::string に反映
			model = context->manager->resource->GetModelLoader()->LoadModel(filepathBuffer);
		}
		if(pixelShader){
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), pixelShader->FilePath.c_str(), _TRUNCATE);
		}
		if(ImGui::InputText("PixelShader File Path", filepathBuffer, sizeof(filepathBuffer))){
			// 編集されたら std::string に反映
			pixelShader = context->manager->resource->GetShaderLoader()->LoadPixelShader(filepathBuffer);
		}
		if(vertexShader){
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), vertexShader->FilePath.c_str(), _TRUNCATE);
		}
		if(ImGui::InputText("VertexShader File Path", filepathBuffer, sizeof(filepathBuffer))){
			// 編集されたら std::string に反映
			vertexShader = context->manager->resource->GetShaderLoader()->LoadVertexShader(filepathBuffer);
		}
	}

	ModelData* model = nullptr;
	PixelShaderData* pixelShader = nullptr;
	VertexShaderData* vertexShader = nullptr;
};
