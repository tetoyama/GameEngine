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
		if(model)
		node["FilePath"] = model->FilePath;
		if(pixelShader)
		node["PixelShader"] = pixelShader->FilePath;
		if(vertexShader)
		node["VertexShader"] = vertexShader->FilePath;
		node["isBlender"] = isBlender;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text("Model File Path");
		ImGui::SameLine(100.0f);
		if(ImGui::Checkbox("##isBlender", &isBlender)){
			std::string path = model->FilePath;
			model = context->manager->resource->GetModelLoader()->LoadModel(path, isBlender);

		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("isBlenderModel?");

		ImGui::SameLine();
		float inputWidth = ImGui::GetContentRegionAvail().x;
		ImGui::SetNextItemWidth(inputWidth);

		char filepathBuffer[256] = ""; // 適当な最大長
		// バッファに現在の文字列をコピー（初回か変更時だけにすると効率的）
		if(model){
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), model->FilePath.c_str(), _TRUNCATE);
		} else{
			filepathBuffer[0] = '\0'; // 初期化
		}
		if(ImGui::InputText("##Model File Path", filepathBuffer, sizeof(filepathBuffer))){
			// 編集されたら std::string に反映
			model = context->manager->resource->GetModelLoader()->LoadModel(filepathBuffer,isBlender);
		}
		// ドロップ対象の処理
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
				const char* droppedPath = (const char*)payload->Data;
				std::string _filePath = std::string(droppedPath);

				// TODO: 実際のリソースロード処理に差し替えて
				model = context->manager->resource->GetModelLoader()->LoadModel(_filePath, isBlender);
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::Text("PixelShader");
		ImGui::SameLine(100.0f);
		inputWidth = ImGui::GetContentRegionAvail().x;
		ImGui::SetNextItemWidth(inputWidth);
		if(pixelShader){
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), pixelShader->FilePath.c_str(), _TRUNCATE);
		} else{
			filepathBuffer[0] = '\0'; // 初期化
		}
		if(ImGui::InputText("##PixelShader", filepathBuffer, sizeof(filepathBuffer))){
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


		ImGui::Text("VertexShader");
		ImGui::SameLine(100.0f);
		inputWidth = ImGui::GetContentRegionAvail().x;
		ImGui::SetNextItemWidth(inputWidth);
		if(vertexShader){
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), vertexShader->FilePath.c_str(), _TRUNCATE);
		} else{
			filepathBuffer[0] = '\0'; // 初期化
		}
		if(ImGui::InputText("##VertexShader", filepathBuffer, sizeof(filepathBuffer))){
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
	bool isBlender = false;
	PixelShaderData* pixelShader = nullptr;
	VertexShaderData* vertexShader = nullptr;
};
