// =======================================================================
// 
// textureComponent.h
// 
// =======================================================================
#pragma once

#include <algorithm>
#include <cfloat>
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

	int AnimationNum = 0;

	std::shared_ptr<TextureData> m_TextureData;

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
		const int safeFrame = (std::clamp)(AnimationNum, 0, maxFrame);

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
		if(m_TextureData){
			node["FilePath"] = m_TextureData->FilePath;
		}
		node["UV_Slice_X"] = UV_Slice_X;
		node["UV_Slice_Y"] = UV_Slice_Y;
		node["AnimationNum"] = AnimationNum;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(!node.IsMap()) return false;

		if(node["FilePath"]){
			m_TextureData = context->manager->resource->Load<TextureData>(
				node["FilePath"].as<std::string>().c_str()
			);
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

	void inspector(SceneContext* context) override{
		ImGui::PushID(this);

		const ImVec4 colorR(0.7f, 0.4f, 0.4f, 0.48f);
		const ImVec4 colorG(0.4f, 0.7f, 0.4f, 0.48f);

		ImGui::TextUnformatted("UV Slice / Repeat");
		if(ImGui::BeginTable(
			"UVSliceGrid",
			2,
			ImGuiTableFlags_SizingStretchSame |
			ImGuiTableFlags_NoPadOuterX |
			ImGuiTableFlags_NoSavedSettings
		)){
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("X");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::PushStyleColor(ImGuiCol_Border, colorR);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
			ImGui::UndoDragFloat("##UVSliceX", &UV_Slice_X, 0.01f, 0.0001f, 256.0f);
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();

			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Y");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::PushStyleColor(ImGuiCol_Border, colorG);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
			ImGui::UndoDragFloat("##UVSliceY", &UV_Slice_Y, 0.01f, 0.0001f, 256.0f);
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();
			ImGui::EndTable();
		}

		ImGui::TextDisabled("1 = Full / 2 = 2 slices / 0.5 = Repeat x2");

		int maxFrame = ResolveMaxAnimationFrame();
		AnimationNum = (std::clamp)(AnimationNum, 0, maxFrame);

		ImGui::TextUnformatted("Frame");
		if(ImGui::BeginTable(
			"FrameGrid",
			2,
			ImGuiTableFlags_SizingStretchProp |
			ImGuiTableFlags_NoPadOuterX |
			ImGuiTableFlags_NoSavedSettings
		)){
			ImGui::TableSetupColumn("Slider", ImGuiTableColumnFlags_WidthStretch, 0.68f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.32f);

			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGuiStyle& style = ImGui::GetStyle();
			const float oldGrabRounding = style.GrabRounding;
			style.GrabRounding = 100.0f;
			if(ImGui::SliderInt("##FrameSlider", &AnimationNum, 0, maxFrame)){
				AnimationNum = (std::clamp)(AnimationNum, 0, maxFrame);
			}
			style.GrabRounding = oldGrabRounding;

			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			if(ImGui::UndoDragInt("##FrameInput", &AnimationNum, 1, 0, maxFrame)){
				AnimationNum = (std::clamp)(AnimationNum, 0, maxFrame);
			}
			ImGui::EndTable();
		}

		char filepathBuffer[256] = "";
		if(m_TextureData && !m_TextureData->FilePath.empty()){
			strncpy_s(
				filepathBuffer,
				sizeof(filepathBuffer),
				m_TextureData->FilePath.c_str(),
				_TRUNCATE
			);
		}

		if(ImGui::BeginTable(
			"TexturePathRow",
			3,
			ImGuiTableFlags_SizingFixedFit |
			ImGuiTableFlags_NoPadOuterX |
			ImGuiTableFlags_NoSavedSettings
		)){
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 76.0f);
			ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Clear", ImGuiTableColumnFlags_WidthFixed, 28.0f);

			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Texture");

			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			if(ImGui::InputText("##TextureInput", filepathBuffer, sizeof(filepathBuffer))){
				m_TextureData = context->manager->resource->Load<TextureData>(filepathBuffer);
			}

			ImGui::TableNextColumn();
			if(ImGui::SmallButton("x")){
				filepathBuffer[0] = '\0';
				m_TextureData = nullptr;
			}
			ImGui::EndTable();
		}

		ImGui::BeginGroup();
		if(m_TextureData && m_TextureData->pTexture){
			const float availableWidth = ImGui::GetContentRegionAvail().x;
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const bool sideBySide = availableWidth >= 220.0f;
			const float previewSize = sideBySide
				? (std::max)(48.0f, (availableWidth - spacing) * 0.5f)
				: (std::max)(48.0f, (std::min)(availableWidth, 180.0f));

			ImGui::Image(
				(ImTextureID)m_TextureData->pTexture.Get(),
				ImVec2(previewSize, previewSize),
				ImVec2(0, 0),
				ImVec2(1, 1),
				ImVec4(1, 1, 1, 1),
				ImVec4(0, 0, 0, 0)
			);

			if(sideBySide){
				ImGui::SameLine(0.0f, spacing);
			}

			const UVMatrixBuffer previewUv = ResolveUVMatrixBuffer();
			ImGui::Image(
				(ImTextureID)m_TextureData->pTexture.Get(),
				ImVec2(previewSize, previewSize),
				ImVec2(previewUv.UVStart.x, previewUv.UVStart.y),
				ImVec2(previewUv.UVEnd.x, previewUv.UVEnd.y),
				ImVec4(1, 1, 1, 1),
				ImVec4(0, 0, 0, 0)
			);
		} else{
			ImGui::TextDisabled("No texture loaded");
		}
		ImGui::EndGroup();

		if(ImGui::BeginDragDropTarget()){
			if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
				const char* droppedPath = static_cast<const char*>(payload->Data);
				m_TextureData = context->manager->resource->Load<TextureData>(
					std::string(droppedPath)
				);
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::Spacing();
		ImGui::PopID();
	}
};
