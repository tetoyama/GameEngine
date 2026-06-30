// =======================================================================
// 
// textureComponent.h
// 
// =======================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <memory>

#include "Interface/IComponent.h"
#include "Graphics/graphicsContext.h"
#include "Backends/YAMLConverters.h"
#include "scene.h"
#include "sceneManager.h"
#include "Resources/resourceService.h"
#include "Resources/Loader/textureLoader.h"
#include "Resources/Data/TextureData.h"

// テクスチャリソースと UV 変換・スライスアニメーションを管理するコンポーネント。
// UV_Slice_X / UV_Slice_Y は以下の契約で扱う。
//   value == 1.0 : 等倍
//   value >  1.0 : スライス数。2.0 = 2分割、4.0 = 4分割。
//   0 < value < 1.0 : リピート除数。0.5 = 2リピート、0.25 = 4リピート。
// Shader側は常に TransformUV(uv, UVStart, UVEnd) を使い、CPU側でこの契約を
// UVStart / UVEnd へ解決して渡す。
class TextureComponent : public IComponent {
public:
	float UV_Slice_X = 1.0f;
	float UV_Slice_Y = 1.0f;

	int AnimationNum = 0;  // 表示するセルのインデックス

	std::shared_ptr<TextureData> m_TextureData;  // ロード済みテクスチャデータ

	static bool IsSliceValue(float value){
		return value >= 1.0f;
	}

	static float ResolveUVSpan(float value){
		if(value <= 0.0f){
			return 1.0f;
		}
		return 1.0f / value;
	}

	static int ResolveSliceCount(float value){
		if(!IsSliceValue(value)){
			return 1;
		}
		return (std::max)(1, static_cast<int>(std::floor(value + 0.0001f)));
	}

	int ResolveMaxAnimationFrame() const{
		const int columnCount = ResolveSliceCount(UV_Slice_X);
		const int rowCount = ResolveSliceCount(UV_Slice_Y);
		return (std::max)(0, columnCount * rowCount - 1);
	}

	UVMatrixBuffer ResolveUVMatrixBuffer() const{
		UVMatrixBuffer uv{};
		uv.UVStart = float2(0.0f, 0.0f);
		uv.UVEnd = float2(1.0f, 1.0f);

		if(UV_Slice_X <= 0.0f || UV_Slice_Y <= 0.0f){
			return uv;
		}

		const bool sliceX = IsSliceValue(UV_Slice_X);
		const bool sliceY = IsSliceValue(UV_Slice_Y);
		const float spanX = ResolveUVSpan(UV_Slice_X);
		const float spanY = ResolveUVSpan(UV_Slice_Y);
		const int columnCount = ResolveSliceCount(UV_Slice_X);
		const int rowCount = ResolveSliceCount(UV_Slice_Y);
		const int maxFrame = (std::max)(0, columnCount * rowCount - 1);
		const int safeFrame = std::clamp(AnimationNum, 0, maxFrame);

		if(sliceX){
			uv.UVStart.x = (safeFrame % columnCount) * spanX;
			uv.UVEnd.x = uv.UVStart.x + spanX;
		}else{
			uv.UVStart.x = 0.0f;
			uv.UVEnd.x = spanX;
		}

		if(sliceY){
			uv.UVStart.y = (safeFrame / columnCount) * spanY;
			uv.UVEnd.y = uv.UVStart.y + spanY;
		}else{
			uv.UVStart.y = 0.0f;
			uv.UVEnd.y = spanY;
		}

		return uv;
	}

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

        // --- UV Divisor / Slice ---
        ImGui::Text("UV Slice / Repeat");
        ImGui::SameLine(140);

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
        ImGui::UndoDragFloat("##UVSliceX", &UV_Slice_X, 0.01f, 0.0001f, 256.0f);
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
        ImGui::UndoDragFloat("##UVSliceY", &UV_Slice_Y, 0.01f, 0.0001f, 256.0f);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

		ImGui::TextDisabled("1 = Full / 2 = 2 slices / 0.5 = Repeat x2");

		// --- Animation Frame ---
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Frame");
		ImGui::SameLine(100.0f);

		float totalAvail = ImGui::GetContentRegionAvail().x;
		float sliderWidth = totalAvail * 0.6f;
		float inputWidth = totalAvail - sliderWidth - ImGui::GetStyle().ItemSpacing.x * 2;

		int maxFrame = ResolveMaxAnimationFrame();
		if(AnimationNum > maxFrame) AnimationNum = maxFrame;
		if(AnimationNum < 0) AnimationNum = 0;

		// スライダー（左）
		ImGui::PushItemWidth(sliderWidth);
		ImGuiStyle& style = ImGui::GetStyle();
		float oldGrabRounding = style.GrabRounding;

		style.GrabRounding = 100.0f;
		if(ImGui::SliderInt("##FrameSlider", &AnimationNum, 0, maxFrame)){
			AnimationNum = std::clamp(AnimationNum, 0, maxFrame);
		}
		ImGui::PopItemWidth();
		style.GrabRounding = oldGrabRounding;

		ImGui::SameLine();

		// 数値入力（右）
		ImGui::PushItemWidth(inputWidth);
		if(ImGui::UndoDragInt("##FrameInput", &AnimationNum, 1, 0, maxFrame)){
			AnimationNum = std::clamp(AnimationNum, 0, maxFrame);
		}
		ImGui::PopItemWidth();

		// --- Texture Input ---
		ImGui::BeginGroup();
		float textLabelWidth = 100.0f;
		float inputFieldWidth = ImGui::GetContentRegionAvail().x - textLabelWidth - 24.0f;

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
			filepathBuffer[0] = '\0';
			m_TextureData = nullptr;
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
			const UVMatrixBuffer previewUv = ResolveUVMatrixBuffer();
            ImGui::Image(
                (ImTextureID)m_TextureData->pTexture.Get(),
                ImVec2(previewSize, previewSize),
                ImVec2(previewUv.UVStart.x, previewUv.UVStart.y),
                ImVec2(previewUv.UVEnd.x, previewUv.UVEnd.y),
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

		ImGui::PopID();
    }
};
