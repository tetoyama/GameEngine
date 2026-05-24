// =======================================================================
// 
// textureComponent.h
// 
// =======================================================================
#pragma once


#include <memory>

#include "Interface/IComponent.h"

#include "Graphics/graphicsContext.h"
#include "Backends/YAMLConverters.h"
#include "scene.h"
#include "sceneManager.h"
#include "Resources/resourceService.h"
#include "Resources/Loader/textureLoader.h"
#include "Resources/Data/TextureData.h"


// テクスチャリソースと UV スライスアニメーションを管理するコンポーネント
// UV_Slice_X / UV_Slice_Y でテクスチャを格子状に分割し、
// AnimationNum で表示するセルを選択するスプライトシート方式をサポートする
class TextureComponent : public IComponent {
public:
	float uvSliceX= 1.0f;  // テクスチャを横方向に分割する数
	float uvSliceY= 1.0f;  // テクスチャを縦方向に分割する数

	int animationNum= 0;  // 表示するセルのインデックス（0 から UV_Slice_X * UV_Slice_Y - 1）

	std::shared_ptr<TextureData> textureData;  // ロード済みテクスチャデータ

	YAML::Node encode() override{
		YAML::Node node;
		if (m_TextureData) {
			node["FilePath"] = m_TextureData->FilePath;
		}
		node["UV_Slice_X"] = UV_Slice_X;
		node["UV_Slice_Y"] = UV_Slice_Y;
		node["AnimationNum"] = AnimationNum;

		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if (!node.IsMap()) { return false; }

		if (node["FilePath"]) {
			m_TextureData = context->manager->resource->Load<TextureData>(node["FilePath"].as<std::string>().c_str());
		}
		if(node["UV_Slice_X"]){
			UV_Slice_X = node["UV_Slice_X"].as<float>();
		}
		if(node["UV_Slice_Y"]){
			UV_Slice_Y = node["UV_Slice_Y"].as<float>();
		}
		if(node["AnimationNum"]){
			AnimationNum = node["AnimationNum"].as<int>();
		}
		return true;
	}

    void inspector(SceneContext* context) override {
        ImGui::PushID(this);

       
        // 色定義
        ImVec4 colorR = ImVec4(0.7f, 0.4f, 0.4f, 0.3f); // R
        ImVec4 colorG = ImVec4(0.4f, 0.7f, 0.4f, 0.3f); // G

        // --- UV Slice ---
        ImGui::Text("UV Slice");
        ImGui::SameLine(100);

        float availWidth = ImGui::GetContentRegionAvail().x;

        float labelXWidth = ImGui::CalcTextSize("X").x + 5.0f;
        float labelYWidth = ImGui::CalcTextSize("Y").x + 5.0f;

        float inputWidthX = (availWidth / 2) - labelXWidth - 5.0f;
        float inputWidthY = (availWidth / 2) - labelYWidth - 5.0f;

        // X
        ImGui::TextUnformatted("X");
        ImGui::SameLine();
        ImGui::PushItemWidth(inputWidthX);
        ImGui::PushStyleColor(ImGuiCol_Border, colorR);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
        ImGui::UndoDragFloat("##UVSliceX", &UV_Slice_X, 1, 1, 256);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        ImGui::SameLine();

        // Y
        ImGui::TextUnformatted("Y");
        ImGui::SameLine();
        ImGui::PushItemWidth(inputWidthY);
        ImGui::PushStyleColor(ImGuiCol_Border, colorG);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
        ImGui::UndoDragFloat("##UVSliceY", &UV_Slice_Y, 1, 1, 256);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

		// --- Animation Frame ---
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Frame");
		ImGui::SameLine(100.0f);

		// 計算
		float totalAvail = ImGui::GetContentRegionAvail().x;
		float sliderWidth = totalAvail * 0.6f;
		float inputWidth = totalAvail - sliderWidth - ImGui::GetStyle().ItemSpacing.x * 2;

		int maxFrame = (int)(UV_Slice_X * UV_Slice_Y) - 1;
		if(maxFrame < 0) maxFrame = 0;

		// スライダー（左）
		ImGui::PushItemWidth(sliderWidth);
		// スタイル調整（丸いスライダーグリップ、細いバー）
		ImGuiStyle& style = ImGui::GetStyle();
		float oldGrabRounding = style.GrabRounding;

		style.GrabRounding = 100.0f;             // 丸いつまみ
		if(ImGui::SliderInt("##FrameSlider", &AnimationNum, 0, maxFrame)){
			if(AnimationNum < 0) AnimationNum = 0;
		}
		ImGui::PopItemWidth();
		// スタイル戻す
		style.GrabRounding = oldGrabRounding;

		ImGui::SameLine();

		// 数値入力（右）
		ImGui::PushItemWidth(inputWidth);
		if(ImGui::UndoDragInt("##FrameInput", &AnimationNum, 1, 0, maxFrame)){
			if(AnimationNum < 0) AnimationNum = 0;
		}
		ImGui::PopItemWidth();


		// --- Texture Input ---
		ImGui::BeginGroup();
		float textLabelWidth = 100.0f;
		float inputFieldWidth = ImGui::GetContentRegionAvail().x - textLabelWidth - 24.0f; // 余白 + ボタンサイズ分調整

		char filepathBuffer[256] = "";
		if(m_TextureData && !m_TextureData->FilePath.empty()){
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), m_TextureData->FilePath.c_str(), _TRUNCATE);
		}

		ImGui::Text("Texture");
		ImGui::SameLine(textLabelWidth);
		ImGui::PushItemWidth(inputFieldWidth);
		if(ImGui::InputText("##TextureInput", filepathBuffer, sizeof(filepathBuffer))){
			m_TextureData = context->manager->resource->Load<TextureData>(filepathBuffer);
		}
		ImGui::PopItemWidth();


		// Clear Button
		ImGui::SameLine();
		if(ImGui::SmallButton("x")){
			filepathBuffer[0] = '\0'; // クリア
			m_TextureData = nullptr; // テクスチャデータもクリア
		}


        // --- Texture Preview ---
        if (m_TextureData && m_TextureData->pTexture) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float previewSize = avail.x * 0.5f - ImGui::GetStyle().ItemSpacing.x * 2;
            float spacing = 8.0f;

            ImGui::BeginGroup();
            ImGui::Image(
                (ImTextureID)m_TextureData->pTexture.Get(),
                ImVec2(previewSize, previewSize),
                ImVec2(0, 0), ImVec2(1, 1),
                ImVec4(1, 1, 1, 1),
                ImVec4(0, 0, 0, 0)
            );
            ImGui::EndGroup();

            ImGui::SameLine(0.0f, spacing);

            ImGui::BeginGroup();
            ImVec2 start, end;

			if (UV_Slice_X > 0.0f && UV_Slice_Y > 0.0f) {
				// UV_Slice_X/Y は「1セルのUVサイズ」
				// 例:
				// 0.25f = 4分割
				// 0.125f = 8分割

				int column = (int)(1.0f / UV_Slice_X);

				start.x = (AnimationNum % column) * UV_Slice_X;
				start.y = (AnimationNum / column) * UV_Slice_Y;

				// 1 セルの UV サイズ: 1/スライス数
				end.x = start.x + 1.0f / UV_Slice_X;  // セルの右端 UV
				end.y = start.y + 1.0f / UV_Slice_Y;  // セルの下端 UV
			}
            ImGui::Image(
                (ImTextureID)m_TextureData->pTexture.Get(),
                ImVec2(previewSize, previewSize),
                start, end,
                ImVec4(1, 1, 1, 1),
                ImVec4(0, 0, 0, 0)
            );
            ImGui::EndGroup();
        } else {
            ImGui::TextDisabled("No texture loaded");
        }

		ImGui::EndGroup();
		// --- Drag and Drop for Texture ---
		if(ImGui::BeginDragDropTarget()){
			if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
				const char* droppedPath = (const char*)payload->Data;
				std::string _texturePath = std::string(droppedPath);

				m_TextureData = context->manager->resource->Load<TextureData>(_texturePath);
			}
			ImGui::EndDragDropTarget();
		}
        ImGui::Spacing();

		ImGui::PopID(); // コンポーネントのIDをポップ
    }

};
