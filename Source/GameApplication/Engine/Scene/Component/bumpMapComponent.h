// =======================================================================
// 
// bumpMapComponent.h
// 
// =======================================================================
#pragma once

#include <memory>
#include "Interface/IComponent.h"

#include "Graphics/graphicsContext.h"
#include "BackEnds/YAMLConverters.h"
#include "scene.h"
#include "sceneManager.h"
#include "Resources/resourceService.h"
#include "Resources/Loader/textureLoader.h"
#include "Resources/Data/TextureData.h"
// バンプマップテクスチャを管理するコンポーネント
class BumpMapComponent: public IComponent {
public:
	~BumpMapComponent() {
		m_TextureData.reset();
	}
	std::shared_ptr<TextureData> m_TextureData;

	YAML::Node encode() override{
		YAML::Node node;
		if(m_TextureData){
			node["FilePath"] = m_TextureData->FilePath;
		}
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(!node.IsMap()){
			return false;
		}

		if(node["FilePath"]){
			m_TextureData = context->manager->resource->Load<TextureData>(node["FilePath"].as<std::string>().c_str());
		}
		return true;
	}
	void inspector(SceneContext* context) override{
		ImGui::PushID(this);

		float imageSize = 64.0f;
		float spacing = 8.0f;
		ImGui::BeginGroup();

		// --- Image Preview ---
		if(m_TextureData && m_TextureData->pTexture){
			ImGui::Image(
				(ImTextureID)m_TextureData->pTexture.Get(),
				ImVec2(imageSize, imageSize),
				ImVec2(0, 0), ImVec2(1, 1),
				ImVec4(1, 1, 1, 1),
				ImVec4(0, 0, 0, 0)
			);
		} else{
			ImGui::Dummy(ImVec2(imageSize, imageSize)); // 画像がない場合のスペース
			//ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f);
			//ImGui::TextDisabled("No\nImage");
		}


		ImGui::SameLine(0.0f, spacing);

		// --- Filepath Text Input + Clear Button ---
		ImGui::BeginGroup();

		char filepathBuffer[256] = "";
		if(m_TextureData && !m_TextureData->FilePath.empty()){
			strncpy_s(filepathBuffer, sizeof(filepathBuffer), m_TextureData->FilePath.c_str(), _TRUNCATE);
		}

		float inputFieldWidth = ImGui::GetContentRegionAvail().x - 24.0f; // xボタン分

		ImGui::Text("Texture");
		ImGui::PushItemWidth(inputFieldWidth);
		if(ImGui::InputText("##TextureInput", filepathBuffer, sizeof(filepathBuffer))){
			m_TextureData = context->manager->resource->Load<TextureData>(filepathBuffer);
		}
		ImGui::PopItemWidth();

		ImGui::SameLine();
		if(ImGui::SmallButton("x")){
			filepathBuffer[0] = '\0';
			m_TextureData = nullptr;
		}

		ImGui::EndGroup();
		ImGui::EndGroup();

		// --- Drag and Drop Support ---
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
