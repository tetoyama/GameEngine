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
		// ドロップ対象の処理
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
				const char* droppedPath = (const char*)payload->Data;
				std::string _filePath = std::string(droppedPath);

				// TODO: 実際のリソースロード処理に差し替えて
				model = context->manager->resource->GetModelLoader()->LoadModel(_filePath);
			}
			ImGui::EndDragDropTarget();
		}


		if(pixelShader){
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), pixelShader->FilePath.c_str(), _TRUNCATE);
		}
		if(ImGui::InputText("PixelShader", filepathBuffer, sizeof(filepathBuffer))){
			// 編集されたら std::string に反映
			pixelShader = context->manager->resource->GetShaderLoader()->LoadPixelShader(filepathBuffer);
		}
		// ドロップ対象の処理
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
				const char* droppedPath = (const char*)payload->Data;
				std::string _filePath = std::string(droppedPath);

				// TODO: 実際のリソースロード処理に差し替えて
				pixelShader = context->manager->resource->GetShaderLoader()->LoadPixelShader(_filePath);
			}
			ImGui::EndDragDropTarget();
		}



		if(vertexShader){
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), vertexShader->FilePath.c_str(), _TRUNCATE);
		}
		if(ImGui::InputText("VertexShader", filepathBuffer, sizeof(filepathBuffer))){
			// 編集されたら std::string に反映
			vertexShader = context->manager->resource->GetShaderLoader()->LoadVertexShader(filepathBuffer);
		}
		// ドロップ対象の処理
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
				const char* droppedPath = (const char*)payload->Data;
				std::string _filePath = std::string(droppedPath);

				// TODO: 実際のリソースロード処理に差し替えて
				vertexShader = context->manager->resource->GetShaderLoader()->LoadVertexShader(_filePath);
			}
			ImGui::EndDragDropTarget();
		}
	}

	ModelData* model = nullptr;
	PixelShaderData* pixelShader = nullptr;
	VertexShaderData* vertexShader = nullptr;
};
