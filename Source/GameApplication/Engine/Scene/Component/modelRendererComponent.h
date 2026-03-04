// =======================================================================
// 
// modelRendererComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include "Scene/scene.h"

#include <string>
#include <memory>
#include <d3d11.h>
#include <vector>
#include <algorithm>

#include "Backends/YAMLConverters.h"

#include "Resources/resourceService.h"

#include "Resources/Data/modelData.h"
#include "Resources/Loader/modelLoader.h"

#include "DebugTools/DebugSystem.h"

// 3Dモデルの描画を管理するコンポーネント
class ModelRendererComponent: public IComponent {
public:
	
	std::shared_ptr<ModelData>model = nullptr;

	std::string modelFilePath;
	std::vector<std::pair<std::string,std::string>> animations;
	std::vector<AnimationBlend> blendedAnimations;

	// 描画用（Dynamic）
	std::vector<ID3D11Buffer*> dynamicVertexBuffers;

	bool isBlender = false;
	float animationTime = 0.0f;

	ModelRendererComponent() = default;

	~ModelRendererComponent(){
		ReleaseBuffers();
		model = nullptr;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(node["FilePath"]){
			modelFilePath = node["FilePath"].as<std::string>();
		}
		if (node["isBlender"]) {
			isBlender = node["isBlender"].as<bool>();
		}
		if (node["AnimationTime"]) {
			animationTime = node["AnimationTime"].as<float>();
		}
		animations.clear();
		for(const auto& animNode : node["Animations"]){
			animations.emplace_back(animNode.first.as<std::string>(), animNode.second.as<std::string>());
		}
		CreateModel(context);
		return true;
	}

	YAML::Node encode() override{
		YAML::Node node;
		if(model){
			node["FilePath"] = model->FilePath;
		}

		node["isBlender"] = isBlender;
		node["AnimationTime"] = animationTime;
		for (const auto& [animName, animFile] : animations) {
			node["Animations"][animName] = animFile;
		}
		return node;
	}

	void CreateModel(SceneContext* context){

		if(modelFilePath.empty()){
			return;
		}
		if(model){
			ReleaseBuffers();
			model.reset();
		}

		model = context->manager->resource->Load<ModelData>(modelFilePath, isBlender);

		for(const auto& animNode : animations){
			std::string animName = animNode.first;
			std::string animFile = animNode.second;
			model->LoadAnimation(animFile.c_str(), animName.c_str());
		}

		dynamicVertexBuffers.clear();
		dynamicVertexBuffers.resize(model->AiScene->mNumMeshes);

		for(uint32_t i = 0; i < model->AiScene->mNumMeshes; i++){
			aiMesh* mesh = model->AiScene->mMeshes[i];

			D3D11_BUFFER_DESC desc{};
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = sizeof(VERTEX_3D) * mesh->mNumVertices;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			HRESULT hr = context->manager->graphics->GetDevice()->CreateBuffer(
				&desc, nullptr, &dynamicVertexBuffers[i]
			);
			assert(SUCCEEDED(hr));
		}

	}

	void ReleaseBuffers(){
		for(auto* vb : dynamicVertexBuffers){
			if(vb) vb->Release();
		}
		dynamicVertexBuffers.clear();
	}

	void inspector(SceneContext* context) override {

		ImGui::Text("Model File Path");
		ImGui::SameLine(100.0f);
		if (ImGui::UndoCheckbox("##isBlender", &isBlender)) {
			std::string path = model->FilePath;

			ReleaseBuffers();
			model.reset();
			animations.clear();
			blendedAnimations.clear();
			CreateModel(context);
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
			ReleaseBuffers();
			model.reset();
			animations.clear();
			blendedAnimations.clear();

			modelFilePath = std::string(filepathBuffer);
			CreateModel(context);
		}
		// ドロップ対象の処理
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
				ReleaseBuffers();
				model.reset();
				animations.clear();
				blendedAnimations.clear();

				const char* droppedPath = (const char*)payload->Data;
				std::string _filePath = std::string(droppedPath);
				modelFilePath = _filePath;
				CreateModel(context);
			}
			ImGui::EndDragDropTarget();
		}

		if (model) {
			// アニメーション一覧 + 削除ボタン
			ImGui::Separator();

			ImGui::UndoDragFloat("Animation Time", &animationTime, 1.0f, 0, 0);

			static char newAnimFilePath[256] = "";
			static char newAnimName[128] = "";
			inputWidth = ImGui::GetContentRegionAvail().x;
			ImGui::SetNextItemWidth(inputWidth);
			if(ImGui::TreeNodeEx(("LoadedAnimation(" + std::to_string(model->m_Animation.size()) + ")").c_str(), ImGuiTreeNodeFlags_DefaultOpen)){


				// --- Add Animation Section ---
				ImGui::BeginGroup();

				// Add Animation ボタン（ポップアップ開く用）
				if(ImGui::Button("Add Animation")){
					ImGui::OpenPopup("AddAnimationPopup");
				}
				ImGui::SameLine();
				inputWidth = ImGui::GetContentRegionAvail().x;
				ImGui::SetNextItemWidth(inputWidth - 20.0f);

				ImGui::InputText("##AddAnimation", newAnimFilePath, sizeof(newAnimFilePath));

				ImGui::EndGroup();

				// ボタンに対するドラッグ&ドロップ処理
				if(ImGui::BeginDragDropTarget()){
					if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
						const char* droppedPath = (const char*)payload->Data;
						strncpy_s(newAnimFilePath, sizeof(newAnimFilePath), droppedPath, _TRUNCATE);
						ImGui::OpenPopup("AddAnimationPopup");
					}
					ImGui::EndDragDropTarget();
				}

				std::vector<std::string> toDelete;
				if(model->m_Animation.empty()){

				} else{

					ImGui::Separator();

					for(const auto& [name, anim] : model->m_Animation){
						ImGui::PushID(name.c_str());
						if(anim.isImported){
							if(ImGui::Button("Delete")){
								toDelete.push_back(name);
							}
						} else{
							ImGui::BeginDisabled();
							ImGui::Button("Delete");
							ImGui::EndDisabled();
						}
						ImGui::SameLine();

						ImGui::Text("%s", name.c_str());



						ImGui::PopID();
					}
					for(const auto& delName : toDelete){

						// ModelData 側の削除
						model->RemoveAnimation(delName);

						// Component 側の animations からも削除
						animations.erase(
							std::remove_if(
								animations.begin(),
								animations.end(),
								[&](const std::pair<std::string, std::string>& anim){
									return anim.first == delName;
								}
							),
							animations.end()
						);

						// ブレンドリストからも削除
						blendedAnimations.erase(
							std::remove_if(
								blendedAnimations.begin(),
								blendedAnimations.end(),
								[&](const AnimationBlend& blend){
									return blend.name == delName;
								}
							),
							blendedAnimations.end()
						);
					}
				}

				// ポップアップ内容
				if(ImGui::BeginPopupModal("AddAnimationPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){

					ImGui::InputText("File Path", newAnimFilePath, sizeof(newAnimFilePath));

					// FilePath に対するドラッグ&ドロップ（ポップアップ内でも対応）
					if(ImGui::BeginDragDropTarget()){
						if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
							const char* droppedPath = (const char*)payload->Data;
							strncpy_s(newAnimFilePath, sizeof(newAnimFilePath), droppedPath, _TRUNCATE);
						}
						ImGui::EndDragDropTarget();
					}

					ImGui::InputText("Name", newAnimName, sizeof(newAnimName));

					if(ImGui::Button("Add")){
						std::string filePathStr(newAnimFilePath);
						std::string animNameStr(newAnimName);
						if(!filePathStr.empty() && !animNameStr.empty() &&
						   model && model->m_Animation.find(animNameStr) == model->m_Animation.end()){

							model->LoadAnimation(filePathStr.c_str(), animNameStr.c_str());
							animations.push_back(std::make_pair(animNameStr, filePathStr));
							// 入力欄をクリア
							newAnimFilePath[0] = '\0';
							newAnimName[0] = '\0';
							ImGui::CloseCurrentPopup();
						}
						ImGui::SameLine();
					}
					ImGui::SameLine();

					if(ImGui::Button("Cancel")){
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}

				ImGui::TreePop();
			}
			// --- モーションブレンド編集UI ---
			ImGui::Separator();
			if(ImGui::TreeNodeEx(("Motion Blend(" + std::to_string(blendedAnimations.size()) + ")").c_str(), ImGuiTreeNodeFlags_DefaultOpen)){

				if(!model->m_Animation.empty()){

					// 新規追加用UI
					static int newBlendAnimIndex = 0;

					if(!model->m_Animation.empty()){
						std::vector<std::string> animNames;
						for(const auto& pair : model->m_Animation){
							animNames.push_back(pair.first);
						}

						if(ImGui::Button("Add Animation")){
							const std::string& newName = animNames[newBlendAnimIndex];
							// すでに登録済みかチェック
							bool exists = false;
							for(const auto& entry : blendedAnimations){
								if(entry.name == newName){
									exists = true;
									break;
								}
							}
							if(!exists){
								blendedAnimations.push_back({newName, 0.0f});
							}
						}
						ImGui::SameLine();
						ImGui::Combo("##Add Animation", &newBlendAnimIndex,
									 [](void* data, int idx, const char** out_text){
										 auto& names = *static_cast<std::vector<std::string>*>(data);
										 *out_text = names[idx].c_str();
										 return true;
									 }, &animNames, (int)animNames.size());
					}

					// ブレンド用アニメーションリストの表示
					for(int i = 0; i < (int)blendedAnimations.size(); ++i){
						auto& blendEntry = blendedAnimations[i];
						ImGui::PushID(i);

						// 削除ボタン
						if(ImGui::Button("Delete")){
							blendedAnimations.erase(blendedAnimations.begin() + i);
							ImGui::PopID();
							break;
						}
						ImGui::SameLine();

						// アニメーション名取得
						int currentIdx = 0;
						std::vector<std::string> animNames;
						for(const auto& pair : model->m_Animation){
							animNames.push_back(pair.first);
						}
						for(int idx = 0; idx < (int)animNames.size(); ++idx){
							if(animNames[idx] == blendEntry.name){
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
						ImGui::UndoDragFloat("##Weight", &blendEntry.weight, 0.01f, 0.0f, 1.0f);
						ImGui::PopItemWidth();

						// 開始時間スライダー
						ImGui::SameLine();
						baseX = ImGui::GetCursorPosX();
						ImGui::Text("StartTime");
						ImGui::SameLine(baseX + nameWidth);
						ImGui::PushItemWidth(sliderWidth);
						ImGui::UndoDragFloat("##StartTime", &blendEntry.animationStartTime, 0.0f, 0.0f,
										 (float)model->m_Animation[blendEntry.name].Scene->mAnimations[0]->mDuration);
						ImGui::PopItemWidth();

						ImGui::PopID();
					}


				} else{
					ImGui::TextDisabled("no animations loaded.");
				}

				ImGui::TreePop();

			}
		}
	}
};
