// =======================================================================
// 
// materialComponent.h
// 
// =======================================================================
#pragma once


#include <memory>

#include "Interface/IComponent.h"

#include "Graphics/graphicsContext.h"
#include "Backends/YAMLConverters.h"
#include "scene.h"
#include "sceneManager.h"
#include "Service/Config/configSystem.h"
#include "Registry/systemRegistry.h"
#include "System/Render/RenderSystem/renderSystem.h"

// マテリアル設定を管理するコンポーネント
// シェーダー ID（RenderSystem に登録されたシェーダーのインデックス）と
// MATERIAL 構造体（色・粗さ・金属度等の PBR パラメーター）を保持する
class MaterialComponent: public IComponent {
public:

	int shaderId= 0;    // 使用するシェーダーのインデックス（RenderSystem::ShaderMaterials への参照）
	MATERIAL material;   // PBR マテリアルパラメーター（色・粗さ・金属度等）

	// デフォルトマテリアル: 白色・アルファ 1.0 で初期化
	MaterialComponent(){
		MATERIAL defaultMat = {};
		Material = defaultMat;
		Material.BaseColor = float4(1, 1, 1, 1);
	}

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

	void inspector(SceneContext* context) override {
		ImGui::PushID(this);

// ShaderMaterials から表示用リストを作る（毎フレームでもOKだが、可能ならキャッシュ）
		std::vector<std::string> shaderNames;
		shaderNames.reserve(context->pSystem->GetSystem<RenderSystem>()->ShaderMaterials.size());

		for (const auto& mat : context->pSystem->GetSystem<RenderSystem>()->ShaderMaterials) {
			shaderNames.push_back(mat.entryPoint);
		}

		// ImGui::Combo 用に const char* 配列へ変換
		std::vector<const char*> shaderNamePtrs;
		shaderNamePtrs.reserve(shaderNames.size());

		for (const auto& name : shaderNames) {
			shaderNamePtrs.push_back(name.c_str());
		}

		ImGui::Text("Shader");
		ImGui::SameLine(100);

		// Combo
		ImGui::Combo(
			"##Shader",
			&ShaderID,
			shaderNamePtrs.data(),
			static_cast<int>(shaderNamePtrs.size())
		);


		// 色定義
		// 各チャンネルの背景色テーマ（軽い色でチャンネルを視覚的に区別）
		ImVec4 colorR = ImVec4(0.7f, 0.4f, 0.4f, 0.3f); // R
		ImVec4 colorG = ImVec4(0.4f, 0.7f, 0.4f, 0.3f); // G
		ImVec4 colorB = ImVec4(0.4f, 0.4f, 0.7f, 0.3f); // B
		ImVec4 colorA = ImVec4(0.8f, 0.8f, 0.8f, 0.3f); // A

		float* c = &Material.BaseColor.x;

		// --- Material Properties ---

		// 色プロパティを 4 チャンネルのドラッグスライダー + カラーピッカーで描画するヘルパーラムダ
		// label: 表示名, c: RGBA フロート配列の先頭ポインタ
		auto drawColorProperty= [](const char* label, float* c) {
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);
			ImGui::SameLine(100);
			float totalWidth = ImGui::GetContentRegionAvail().x;
			float spacing = ImGui::GetStyle().ItemSpacing.x;
			float pickerWidth = 24.0f;
			int sliderCount = 4;
			// 利用可能幅からカラーボタンとスペーシングを引いた残りを 4 等分してスライダー幅とする
			float sliderWidth = (totalWidth - pickerWidth - spacing * (sliderCount)) / sliderCount;

			const char* labels[] = { "R", "G", "B", "A" };
			for (int i = 0; i < 4; ++i) {
				ImGui::PushID((std::to_string(i) + label).c_str());
				ImGui::PushItemWidth(sliderWidth);
				// 対応するチャンネル色のボーダーでスライダーを着色
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4((i == 0) * 0.7f, (i == 1) * 0.7f, (i == 2) * 0.7f, 0.3f));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
				ImGui::UndoDragFloat(("##" + (std::string)label).c_str(), &c[i], 0.01f, 0.0f, 1.0f);
				ImGui::PopStyleVar(); ImGui::PopStyleColor(); ImGui::PopItemWidth(); ImGui::PopID();
				ImGui::SameLine();
			}

			// カラーボタン: クリックするとカラーピッカーポップアップを開く
			if (ImGui::ColorButton(("##ColorBtn" + (std::string)label).c_str(), ImVec4(c[0], c[1], c[2], c[3]), ImGuiColorEditFlags_NoTooltip)) {
				ImGui::OpenPopup(("ColorPickerPopup" + (std::string)label).c_str());
			}
			if (ImGui::BeginPopup(("ColorPickerPopup" + (std::string)label).c_str())) {
				ImGui::ColorPicker4(("##ColorPicker" + (std::string)label).c_str(), c, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar);
				ImGui::EndPopup();
			}
		};

		DrawColorProperty("BaseColor", &Material.BaseColor.x);
		// Material: Metallic
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Metallic");
		ImGui::SameLine(100);
		ImGui::UndoDragFloat("##Metallic", &Material.Metallic, 0.01f, 0.0f, 1.0f);
		// Material: Roughness
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Roughness");
		ImGui::SameLine(100);
		ImGui::UndoDragFloat("##Roughness", &Material.Roughness, 0.01f, 0.0f, 1.0f);
		// Material: AO
		ImGui::AlignTextToFramePadding();
		ImGui::Text("AO");
		ImGui::SameLine(100);
		ImGui::UndoDragFloat("##AO", &Material.AO, 0.5f);

		// Emissive
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Emissive");
		ImGui::SameLine(100);
		ImGui::UndoDragFloat4("##Emissive", &Material.EmissiveColor.x, 0.01f, 0.0f, 1.0f);

		// Environment Map
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Env Map");
		ImGui::SameLine(100);
		bool useEnvMap = (Material.MaterialFlags & MATERIAL_FLAG_USE_ENVIRONMENT_MAP) != 0;
		if (ImGui::Checkbox("##UseEnvMap", &useEnvMap)) {
			if (useEnvMap)
				Material.MaterialFlags |= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
			else
				Material.MaterialFlags &= ~MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
		}



		ImGui::PopID(); // コンポーネントのIDをポップ
	}

};
