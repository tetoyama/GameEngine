#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <memory>

#include "Interface/IComponent.h"

#include "Graphics/graphicsContext.h"
#include "BackEnds/YAMLConverters.h"
#include "scene.h"
#include "sceneManager.h"
#include "Resources/resourceService.h"
#include "Resources/Loader/textureLoader.h"
#include "Resources/Data/TextureData.h"


class TextureComponent : public IComponent {
public:
	//~TextureComponent() {
	//	m_TextureData.reset();
	//}
	int UV_Slice_X = 1;
	int UV_Slice_Y = 1;

	int AnimationNum = 0;

	std::shared_ptr<TextureData> m_TextureData;
	MATERIAL Material;

	YAML::Node encode() override{
		YAML::Node node;
		if (m_TextureData) {
			node["FilePath"] = m_TextureData->FilePath;
		}
		node["Material"] = Material;
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

    void inspector(SceneContext* context) override {
        ImGui::PushID(this);

       
        // 色定義
        ImVec4 colorR = ImVec4(0.7f, 0.4f, 0.4f, 0.3f); // R
        ImVec4 colorG = ImVec4(0.4f, 0.7f, 0.4f, 0.3f); // G
        ImVec4 colorB = ImVec4(0.4f, 0.4f, 0.7f, 0.3f); // B
        ImVec4 colorA = ImVec4(0.8f, 0.8f, 0.8f, 0.3f); // A

        float* c = &Material.Diffuse.x;

        // ------------------------------
        // 1. ラベル（左側）
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Diffuse");
        ImGui::SameLine(100);

        // 2. 右側にスライダー群とピッカー
        float totalWidth = ImGui::GetContentRegionAvail().x;

        // 余白と間隔を考慮
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        int sliderCount = 4;
        float pickerWidth = 24.0f;
        float sliderWidth = (totalWidth - pickerWidth - spacing * (sliderCount)) / sliderCount;

        // R
        ImGui::PushID("R");
        ImGui::PushItemWidth(sliderWidth);
        ImGui::PushStyleColor(ImGuiCol_Border, colorR);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::DragFloat("##R", &c[0], 0.01f, 0.0f, 1.0f);
        ImGui::PopStyleVar(); ImGui::PopStyleColor(); ImGui::PopItemWidth(); ImGui::PopID();
        ImGui::SameLine();

        // G
        ImGui::PushID("G");
        ImGui::PushItemWidth(sliderWidth);
        ImGui::PushStyleColor(ImGuiCol_Border, colorG);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::DragFloat("##G", &c[1], 0.01f, 0.0f, 1.0f);
        ImGui::PopStyleVar(); ImGui::PopStyleColor(); ImGui::PopItemWidth(); ImGui::PopID();
        ImGui::SameLine();

        // B
        ImGui::PushID("B");
        ImGui::PushItemWidth(sliderWidth);
        ImGui::PushStyleColor(ImGuiCol_Border, colorB);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::DragFloat("##B", &c[2], 0.01f, 0.0f, 1.0f);
        ImGui::PopStyleVar(); ImGui::PopStyleColor(); ImGui::PopItemWidth(); ImGui::PopID();
        ImGui::SameLine();

        // A
        ImGui::PushID("A");
        ImGui::PushItemWidth(sliderWidth);
        ImGui::PushStyleColor(ImGuiCol_Border, colorA);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::DragFloat("##A", &c[3], 0.01f, 0.0f, 1.0f);
        ImGui::PopStyleVar(); ImGui::PopStyleColor(); ImGui::PopItemWidth(); ImGui::PopID();
        ImGui::SameLine();

        // カラーボタン + ピッカー
        ImGui::PushID("ColorPickerPopup");
        if (ImGui::ColorButton("##ColorBtn", ImVec4(c[0], c[1], c[2], c[3]), ImGuiColorEditFlags_NoTooltip)) {
            ImGui::OpenPopup("ColorPickerPopup");
        }
        if (ImGui::BeginPopup("ColorPickerPopup")) {
            ImGui::ColorPicker4("##ColorPicker", c, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar);
            ImGui::EndPopup();
        }
        ImGui::PopID();




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
        ImGui::DragInt("##UVSliceX", &UV_Slice_X, 1, 1, 256);
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
        ImGui::DragInt("##UVSliceY", &UV_Slice_Y, 1, 1, 256);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

		// --- Animation Frame ---
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Frame");
		ImGui::SameLine(100.0f);

		// 計算
		float totalAvail = ImGui::GetContentRegionAvail().x;
		sliderWidth = totalAvail * 0.6f;
		float inputWidth = totalAvail - sliderWidth - ImGui::GetStyle().ItemSpacing.x * 2;

		int maxFrame = UV_Slice_X * UV_Slice_Y - 1;
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
		if(ImGui::DragInt("##FrameInput", &AnimationNum, 1, 0, maxFrame)){
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
            UVMatrix uv;
            ImVec2 start, end;

            start.x = (float)(AnimationNum % UV_Slice_X) / (float)UV_Slice_X;
            start.y = (float)(AnimationNum / UV_Slice_X) / (float)UV_Slice_Y;

            end.x = start.x + 1.0f / (float)UV_Slice_X;
            end.y = start.y + 1.0f / (float)UV_Slice_Y;

            ImGui::Image(
                (ImTextureID)m_TextureData->pTexture.Get(),
                ImVec2(previewSize, previewSize),
                start, end,
                ImVec4(Material.Diffuse.x, Material.Diffuse.y, Material.Diffuse.z, Material.Diffuse.w),
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

        // --- Material Properties ---
		if(ImGui::TreeNodeEx("Material Properties")){

			auto DrawColorProperty = [](const char* label, float* c, ImVec4 color){
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(label);
				ImGui::SameLine(100);
				float totalWidth = ImGui::GetContentRegionAvail().x;
				float spacing = ImGui::GetStyle().ItemSpacing.x;
				float pickerWidth = 24.0f;
				int sliderCount = 4;
				float sliderWidth = (totalWidth - pickerWidth - spacing * (sliderCount)) / sliderCount;

				const char* labels[] = {"R", "G", "B", "A"};
				for(int i = 0; i < 4; ++i){
					ImGui::PushID((std::to_string(i) + label).c_str());
					ImGui::PushItemWidth(sliderWidth);
					ImGui::PushStyleColor(ImGuiCol_Border, ImVec4((i == 0) * 0.7f, (i == 1) * 0.7f, (i == 2) * 0.7f, 0.3f));
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
					ImGui::DragFloat(("##" + (std::string)label).c_str(), &c[i], 0.01f, 0.0f, 1.0f);
					ImGui::PopStyleVar(); ImGui::PopStyleColor(); ImGui::PopItemWidth(); ImGui::PopID();
					ImGui::SameLine();
				}

				if(ImGui::ColorButton(("##ColorBtn" + (std::string)label).c_str(), ImVec4(c[0], c[1], c[2], c[3]), ImGuiColorEditFlags_NoTooltip)){
					ImGui::OpenPopup(("ColorPickerPopup" + (std::string)label).c_str());
				}
				if(ImGui::BeginPopup(("ColorPickerPopup" + (std::string)label).c_str())){
					ImGui::ColorPicker4(("##ColorPicker" + (std::string)label).c_str(), c, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar);
					ImGui::EndPopup();
				}
				};

			// Material: Ambient
			DrawColorProperty("Ambient", &Material.Ambient.x, colorR);
			// Material: Specular
			DrawColorProperty("Specular", &Material.Specular.x, colorG);
			// Material: Emission
			DrawColorProperty("Emission", &Material.Emission.x, colorB);

			// Shininess
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Shininess");
			ImGui::SameLine(100);
			ImGui::DragFloat("##Shininess", &Material.Shininess, 0.5f, 1.0f, 128.0f);

			ImGui::TreePop(); // Material Propertiesのツリーをポップ

		}


		ImGui::PopID(); // コンポーネントのIDをポップ
    }

};
