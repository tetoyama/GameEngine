#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include "Interface/IComponent.h"

#include "GameApplication/Engine/Graphics/graphicsContext.h"
#include "Service/YAMLConverters.h"
#include "scene.h"
#include "sceneManager.h"
#include "Engine/Resources/resourceSystem.h"
#include "Engine/Resources/Loader/textureLoader.h"
#include "Engine/Resources/Data/TextureData.h"
class TextureComponent : public IComponent {
public:

	int UV_Slice_X = 1;
	int UV_Slice_Y = 1;

	int AnimationNum = 0;

	TextureData* m_TextureData = nullptr;
	MATERIAL Material;

	YAML::Node encode() override{
		YAML::Node node;
		node["Component"] = "TextureComponent";
		if (m_TextureData) {
			node["FilePath"] = m_TextureData->FilePath;
		}
		node["Material"] = Material;
		node["UV_Slice_X"] = UV_Slice_X;
		node["UV_Slice_Y"] = UV_Slice_Y;
		node["AnimationNum"] = AnimationNum;


		return node;
	}

	bool decode(const YAML::Node& node) override{
		if (!node.IsMap()) { return false; }

		if (m_TextureData && node["FilePath"]) {
			m_TextureData->FilePath = node["FilePath"].as<std::string>();
		}
		if (node["Material"]) {
			Material = node["Material"].as<MATERIAL>();
		}
		if(node["UV_Slice_X"]){
			UV_Slice_X = node["UV_Slice_X"].as<int>();
		}
		if(node["UV_Slice_Y"]){
			UV_Slice_Y = node["UV_Slice_Y"].as<int>();
		}
		if(node["AnimationNum"]){
			AnimationNum = node["AnimationNum"].as<int>();
		}
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::PushID(this);

		// Diffuseカラー
		ImGui::Text("Diffuse");
		ImGui::SameLine(100);
		ImGui::ColorEdit4("##Diffuse", &Material.Diffuse.x);

		// UV Slice ラベル
		ImGui::Text("UV Slice");
		ImGui::SameLine(100);

		// 残り横幅を取得
		float availWidth = ImGui::GetContentRegionAvail().x;

		// X,Yラベル幅を計算（固定でOKなら不要）
		float labelXWidth = ImGui::CalcTextSize("X").x + 5.0f;
		float labelYWidth = ImGui::CalcTextSize("Y").x + 5.0f;

		// X入力幅：残り幅の半分からラベル幅を引いた分
		float inputWidthX = (availWidth / 2) - labelXWidth - 5.0f; // 4px余白

		// Y入力幅：残り幅の半分からラベル幅を引いた分
		float inputWidthY = (availWidth / 2) - labelYWidth - 5.0f;

		// --- X ---
		ImGui::TextUnformatted("X");
		ImGui::SameLine();

		ImGui::PushItemWidth(inputWidthX);
		ImGui::DragInt("##UVSliceX", &UV_Slice_X, 1, 1, 256);
		ImGui::PopItemWidth();

		ImGui::SameLine();

		// --- Y ---
		ImGui::TextUnformatted("Y");
		ImGui::SameLine();

		ImGui::PushItemWidth(inputWidthY);
		ImGui::DragInt("##UVSliceY", &UV_Slice_Y, 1, 1, 256);
		ImGui::PopItemWidth();


		// Animation Frame
		ImGui::Text("Frame");
		ImGui::SameLine(100);
		int maxFrame = UV_Slice_X * UV_Slice_Y - 1;
		if(maxFrame < 0) maxFrame = 0;
		if(ImGui::DragInt("##Frame", &AnimationNum, 1, -1, maxFrame)){
			if(AnimationNum < 0) AnimationNum = 0;
		}
		// テクスチャプレビュー
		if(m_TextureData && m_TextureData->pTexture){

			// 現在の利用可能な横幅を取得
			ImVec2 availSize = ImGui::GetContentRegionAvail();

			//アスペクト比を固定（ここでは正方形にする）
			float size = min(availSize.x, 128.0f);

			ImGui::Image(
				(ImTextureID)m_TextureData->pTexture.Get(),
				ImVec2(size, size),             // サイズを自動調整
				ImVec2(0, 0), ImVec2(1, 1),     // UV
				ImVec4(1, 1, 1, 1),             // 色
				ImVec4(0, 0, 0, 0)              // 枠線なし
			);

			ImGui::SameLine();


			UVMatrix uv;
			ImVec2 start;
			ImVec2 end;
			start.x = (float)(AnimationNum % UV_Slice_Y) * 1.0f / (float)UV_Slice_X;
			start.y = (float)(AnimationNum / UV_Slice_X) * 1.0f / (float)UV_Slice_Y;

			end.x = (float)start.x + 1.0f / (float)UV_Slice_X;
			end.y = (float)start.y + 1.0f / (float)UV_Slice_Y;

			ImGui::Image(
				(ImTextureID)m_TextureData->pTexture.Get(),
				ImVec2(size, size),             // サイズを自動調整
				start, end,     // UV
				ImVec4(Material.Diffuse.x, Material.Diffuse.y, Material.Diffuse.z, Material.Diffuse.w),             // 色
				ImVec4(0, 0, 0, 0)              // 枠線なし
			);
		} else{
			ImGui::TextDisabled("No texture loaded");
		}
		ImGui::PopID();

		static char filepathBuffer[256] = {0}; // 適当な最大長
		// バッファに現在の文字列をコピー（初回か変更時だけにすると効率的）
		if(m_TextureData){
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), m_TextureData->FilePath.c_str(), _TRUNCATE);
		}
		if(ImGui::InputText("Texture", filepathBuffer, sizeof(filepathBuffer))){
			// 編集されたら std::string に反映
			m_TextureData = context->manager->resource->GetTextureLoader()->LoadTexture(filepathBuffer);
		}

		// ドロップ対象の処理
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
				const char* droppedPath = (const char*)payload->Data;
				std::string _texturePath = std::string(droppedPath);

				// TODO: 実際のリソースロード処理に差し替えて
				m_TextureData = context->manager->resource->GetTextureLoader()->LoadTexture(_texturePath);
			}
			ImGui::EndDragDropTarget();
		}
	}
};
