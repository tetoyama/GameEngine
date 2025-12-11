#pragma once
#include "Interface/IComponent.h"
#include "Scene/scene.h"

#include "BackEnds/YAMLConverters.h"

#include "Resources/resourceService.h"
#include "Resources/Data/vertexShaderData.h"
#include "Resources/Data/pixelShaderData.h"
#include "Resources/Data/modelData.h"
#include "Resources/Loader/modelLoader.h"
#include "Resources/Loader/shaderLoader.h"

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
		node["AnimationTime"] = animationTime;
		for (const auto& [name, animData] : model->m_Animation) {
			node["Animations"][name] = animData.FilePath;
		}
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		return true;
	}

	void inspector(SceneContext* context) override {

		ImGui::Text("Model File Path");
		ImGui::SameLine(100.0f);
		if (ImGui::Checkbox("##isBlender", &isBlender)) {
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
		if (model) {
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), model->FilePath.c_str(), _TRUNCATE);
		} else {
			filepathBuffer[0] = '\0'; // 初期化
		}
		if (ImGui::InputText("##Model File Path", filepathBuffer, sizeof(filepathBuffer))) {
			// 編集されたら std::string に反映
			model = context->manager->resource->Load<ModelData>(filepathBuffer, isBlender);
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
		if (pixelShader) {
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), pixelShader->FilePath.c_str(), _TRUNCATE);
		} else {
			filepathBuffer[0] = '\0'; // 初期化
		}
		if (ImGui::InputText("##PixelShader", filepathBuffer, sizeof(filepathBuffer))) {
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
		if (vertexShader) {
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), vertexShader->FilePath.c_str(), _TRUNCATE);
		} else {
			filepathBuffer[0] = '\0'; // 初期化
		}
		if (ImGui::InputText("##VertexShader", filepathBuffer, sizeof(filepathBuffer))) {
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
		if (model) {
		// --- モーションブレンド編集UI ---
			ImGui::Separator();
			ImGui::Text("Motion Blend");

			if (!model->m_Animation.empty()) {
				// ブレンド用アニメーションリストの表示
				for (int i = 0; i < (int)model->blendedAnimations.size(); ++i) {
					auto& blendEntry = model->blendedAnimations[i];
					ImGui::PushID(i);

					// 削除ボタン
					if (ImGui::Button("Remove")) {
						model->blendedAnimations.erase(model->blendedAnimations.begin() + i);
						ImGui::PopID();
						break;
					}
					ImGui::SameLine();

					// アニメーション名取得
					int currentIdx = 0;
					std::vector<std::string> animNames;
					for (const auto& pair : model->m_Animation) {
						animNames.push_back(pair.first);
					}
					for (int idx = 0; idx < (int)animNames.size(); ++idx) {
						if (animNames[idx] == blendEntry.name) {
							currentIdx = idx;
							break;
						}
					}
					blendEntry.name = animNames[currentIdx];

					// レイアウト基準点
					float baseX = ImGui::GetCursorPosX();
					float baseY = ImGui::GetCursorPosY();

					// 指定開始位置（自由に変えてOK）
					float totalWidth = ImGui::GetContentRegionAvail().x;
					float textWidth = totalWidth * 0.3f; // テキスト部分の幅
					float weightStartX = baseX + textWidth; // ここを調整すれば開始位置を制御できる

					// アニメーション名表示
					ImGui::Text("%s", blendEntry.name.c_str());

					// 重みスライダー
					ImGui::SetCursorPosX(weightStartX); // ここで開始位置を明示的に指定
					ImGui::SetCursorPosY(baseY); // ここで開始位置を明示的に指定
					ImGui::Dummy(ImVec2(0.0f, 0.0f));
					ImGui::SameLine();

					// 幅の内訳
					totalWidth = ImGui::GetContentRegionAvail().x;
					float spacing = ImGui::GetStyle().ItemSpacing.x;
					float nameWidth = 60.0f;
					float sliderWidth = (totalWidth - nameWidth * 2.0f - spacing * 4.0f) / 2.0f;

					baseX = ImGui::GetCursorPosX();
					ImGui::Text("Weight");
					ImGui::SameLine(baseX + nameWidth);
					ImGui::PushItemWidth(sliderWidth);
					ImGui::DragFloat("##Weight", &blendEntry.weight, 0.01f, 0.0f, 1.0f);
					ImGui::PopItemWidth();

					// 開始時間スライダー
					ImGui::SameLine();
					baseX = ImGui::GetCursorPosX();
					ImGui::Text("StartTime");
					ImGui::SameLine(baseX + nameWidth);
					ImGui::PushItemWidth(sliderWidth);
					ImGui::DragFloat("##StartTime", &blendEntry.animationStartTime, 0.0f, 0.0f,
									 (float)model->m_Animation[blendEntry.name].Scene->mAnimations[0]->mDuration);
					ImGui::PopItemWidth();

					ImGui::PopID();


				}

				// 新規追加用UI
				static int newBlendAnimIndex = 0;

				if (!model->m_Animation.empty()) {
					std::vector<std::string> animNames;
					for (const auto& pair : model->m_Animation) {
						animNames.push_back(pair.first);
					}

					ImGui::PushItemWidth(150);
					ImGui::Combo("##Add Animation", &newBlendAnimIndex,
								 [](void* data, int idx, const char** out_text) {
						auto& names = *static_cast<std::vector<std::string>*>(data);
						*out_text = names[idx].c_str();
						return true;
					}, &animNames, (int)animNames.size());
					ImGui::PopItemWidth();

					ImGui::SameLine();
					if (ImGui::Button("Add")) {
						const std::string& newName = animNames[newBlendAnimIndex];
						// すでに登録済みかチェック
						bool exists = false;
						for (const auto& entry : model->blendedAnimations) {
							if (entry.name == newName) {
								exists = true;
								break;
							}
						}
						if (!exists) {
							model->blendedAnimations.push_back({ newName, 0.0f });
						}
					}
				}
			} else {
				ImGui::TextDisabled("no animations loaded.");
			}



			static char newAnimFilePath[256] = "";
			static char newAnimName[128] = "";

			// アニメーション一覧 + 削除ボタン
			ImGui::Separator();

			inputWidth = ImGui::GetContentRegionAvail().x;
			ImGui::SetNextItemWidth(inputWidth);
			if (ImGui::TreeNodeEx(("LoadedAnimation(" + std::to_string(model->m_Animation.size()) + ")").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {


				// --- Add Animation Section ---
				ImGui::BeginGroup();

				// Add Animation ボタン（ポップアップ開く用）
				if (ImGui::Button("Add Animation")) {
					ImGui::OpenPopup("AddAnimationPopup");
				}
				ImGui::SameLine();
				inputWidth = ImGui::GetContentRegionAvail().x;
				ImGui::SetNextItemWidth(inputWidth - 20.0f);

				ImGui::InputText("##AddAnimation", newAnimFilePath, sizeof(newAnimFilePath));

				ImGui::EndGroup();

				// ボタンに対するドラッグ&ドロップ処理
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
						const char* droppedPath = (const char*)payload->Data;
						strncpy_s(newAnimFilePath, sizeof(newAnimFilePath), droppedPath, _TRUNCATE);
						ImGui::OpenPopup("AddAnimationPopup");
					}
					ImGui::EndDragDropTarget();
				}

				std::vector<std::string> toDelete;
				if (model->m_Animation.empty()) {

				} else {

					ImGui::Separator();

					for (const auto& [name, anim] : model->m_Animation) {
						ImGui::PushID(name.c_str());
						if (anim.isImported) {
							if (ImGui::Button("Delete")) {
								toDelete.push_back(name);
							}
						} else {
							ImGui::BeginDisabled();
							ImGui::Button("Delete");
							ImGui::EndDisabled();
						}
						ImGui::SameLine();

						ImGui::Text("%s", name.c_str());



						ImGui::PopID();
					}
					for (auto _delete : toDelete) {
						model->RemoveAnimation(_delete);
					}
				}


				// ポップアップ内容
				if (ImGui::BeginPopupModal("AddAnimationPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

					ImGui::InputText("File Path", newAnimFilePath, sizeof(newAnimFilePath));

					// FilePath に対するドラッグ&ドロップ（ポップアップ内でも対応）
					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
							const char* droppedPath = (const char*)payload->Data;
							strncpy_s(newAnimFilePath, sizeof(newAnimFilePath), droppedPath, _TRUNCATE);
						}
						ImGui::EndDragDropTarget();
					}

					ImGui::InputText("Name", newAnimName, sizeof(newAnimName));

					if (ImGui::Button("Add")) {
						std::string filePathStr(newAnimFilePath);
						std::string animNameStr(newAnimName);
						if (!filePathStr.empty() && !animNameStr.empty() &&
							model && model->m_Animation.find(animNameStr) == model->m_Animation.end()) {

							model->LoadAnimation(filePathStr.c_str(), animNameStr.c_str());

							// 入力欄をクリア
							newAnimFilePath[0] = '\0';
							newAnimName[0] = '\0';
							ImGui::CloseCurrentPopup();
						}
						ImGui::SameLine();
					}
					ImGui::SameLine();

					if (ImGui::Button("Cancel")) {
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}

				ImGui::TreePop();
			}

		}
	}

	std::shared_ptr<ModelData>  model = nullptr;
	bool isBlender = false;
	std::shared_ptr<PixelShaderData>  pixelShader = nullptr;
	std::shared_ptr<VertexShaderData>  vertexShader = nullptr;
	float animationTime = 0.0f;
};
