#pragma once
#include "Interface/IComponent.h"
#include "GameApplication/Engine/Scene/scene.h"

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
		if (!currentAnimationName.empty())
			node["CurrentAnimationName"] = currentAnimationName;

		node["isBlender"] = isBlender;
		node["AnimationTime"] = animationTime;
		for (const auto& [name, animData] : model->m_Animation) {
			node["Animations"][name] = animData.FilePath;
		}
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
			model = context->manager->resource->Load<ModelData>(path, isBlender);

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
			model = context->manager->resource->Load<ModelData>(filepathBuffer,isBlender);
		}
		// ドロップ対象の処理
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
				const char* droppedPath = (const char*)payload->Data;
				std::string _filePath = std::string(droppedPath);

				// TODO: 実際のリソースロード処理に差し替えて
				model = context->manager->resource->Load<ModelData>(_filePath, isBlender);
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
			pixelShader = context->manager->resource->Load<PixelShaderData>(filepathBuffer);
		}
		// ドロップ対象の処理
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
				const char* droppedPath = (const char*)payload->Data;
				std::string _filePath = std::string(droppedPath);

				// TODO: 実際のリソースロード処理に差し替えて
				pixelShader = context->manager->resource->Load<PixelShaderData>(_filePath);
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
			vertexShader = context->manager->resource->Load<VertexShaderData>(filepathBuffer);
		}
		// ドロップ対象の処理
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
				const char* droppedPath = (const char*)payload->Data;
				std::string _filePath = std::string(droppedPath);

				// TODO: 実際のリソースロード処理に差し替えて
				vertexShader = context->manager->resource->Load<VertexShaderData>(_filePath);
			}
			ImGui::EndDragDropTarget();
		}

		if (model && !model->m_Animation.empty()) {
			ImGui::Text("Current Animation");
			ImGui::SameLine(130.0f);
			inputWidth = ImGui::GetContentRegionAvail().x;
			ImGui::SetNextItemWidth(inputWidth);

			// アニメーション名一覧取得
			std::vector<std::string> animNames;
			for (const auto& pair : model->m_Animation) {
				animNames.push_back(pair.first);
			}

			// 現在選択されているアニメーションのインデックスを探す
			int currentIndex = 0;
			for (int i = 0; i < (int)animNames.size(); ++i) {
				if (animNames[i] == currentAnimationName) {
					currentIndex = i;
					break;
				}
			}

			if (ImGui::Combo("##CurrentAnimation", &currentIndex,
							 [](void* data, int idx, const char** out_text) {
				auto& names = *static_cast<std::vector<std::string>*>(data);
				*out_text = names[idx].c_str();
				return true;
			}, &animNames, (int)animNames.size())) {
				currentAnimationName = animNames[currentIndex];
				animationTime = 0.0f; // アニメーション切り替えたらフレームをリセット
			}

			ImGui::Text("Frame");
			ImGui::SameLine(100.0f);
			inputWidth = ImGui::GetContentRegionAvail().x;
			ImGui::SetNextItemWidth(inputWidth);

			ImGui::DragFloat("##Frame", &animationTime, 0.1f, 0.0f, 120.0f);
		}

		// --- Add Animation Section ---
		if (ImGui::CollapsingHeader("Add Animation")) {
			static char newAnimFilePath[256] = "";
			static char newAnimName[128] = "";

			ImGui::InputText("Animation File Path", newAnimFilePath, sizeof(newAnimFilePath));

			// ドラッグ＆ドロップ受け付け
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
					const char* droppedPath = (const char*)payload->Data;
					strncpy_s(newAnimFilePath, sizeof(newAnimFilePath), droppedPath, _TRUNCATE);
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::InputText("Animation Name", newAnimName, sizeof(newAnimName));

			if (ImGui::Button("Add")) {
				std::string filePathStr(newAnimFilePath);
				std::string animNameStr(newAnimName);
				if (!filePathStr.empty() && !animNameStr.empty() &&
					model->m_Animation.find(animNameStr) == model->m_Animation.end()) {
					model->LoadAnimation(filePathStr.c_str(), animNameStr.c_str());
					currentAnimationName = animNameStr;
					animationTime = 0.0f;

					newAnimFilePath[0] = '\0';
					newAnimName[0] = '\0';
				}
			}
		}

	}

	std::shared_ptr<ModelData>  model = nullptr;
	bool isBlender = false;
	std::shared_ptr<PixelShaderData>  pixelShader = nullptr;
	std::shared_ptr<VertexShaderData>  vertexShader = nullptr;
	std::string currentAnimationName;
	float animationTime = 0.0f;
};
