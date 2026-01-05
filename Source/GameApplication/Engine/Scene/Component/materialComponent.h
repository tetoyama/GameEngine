#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <memory>

#include "Interface/IComponent.h"

#include "Graphics/graphicsContext.h"
#include "BackEnds/YAMLConverters.h"
#include "scene.h"
#include "sceneManager.h"

class MaterialComponent: public IComponent {
public:

	int ShaderID = 0;
	MATERIAL Material;

	YAML::Node encode() override{
		YAML::Node node;
		node["ShaderID"] = ShaderID;
		node["Material"] = Material;

		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(!node.IsMap()){
			return false;
		}
		if(node["ShaderID"]){
			ShaderID = node["ShaderID"].as<int>();
		}
		if(node["Material"]){
			Material = node["Material"].as<MATERIAL>();
		}
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::PushID(this);

		ImGui::DragInt("Shader ID", &ShaderID,0,1);

		// 色定義
		ImVec4 colorR = ImVec4(0.7f, 0.4f, 0.4f, 0.3f); // R
		ImVec4 colorG = ImVec4(0.4f, 0.7f, 0.4f, 0.3f); // G
		ImVec4 colorB = ImVec4(0.4f, 0.4f, 0.7f, 0.3f); // B
		ImVec4 colorA = ImVec4(0.8f, 0.8f, 0.8f, 0.3f); // A

		float* c = &Material.Diffuse.x;

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
