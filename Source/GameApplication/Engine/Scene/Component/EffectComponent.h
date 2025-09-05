#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

#include <memory>
#include <string>

#include <xaudio2.h>
#include "Engine//Scene/scene.h"

#include "Engine/Resources/Data/effectData.h"
#include "GameApplication/Backends/Effekseer/Effekseer.h"
#include "GameApplication/Backends/Effekseer/EffekseerRendererDX11.h"

class EffectComponent : public IComponent {
public:	
	std::shared_ptr<EffectData> m_EffectData;
	std::string FilePath;

	bool Loop = false;
	float Volume = 1.0f;
	bool PlayOnStart = false;
	bool Playing = false;

	Effekseer::Handle  m_Handle; // 再生ハンドルは実行時に取得する

	EffectComponent() = default;
	~EffectComponent(){}

	  YAML::Node encode() override {
		  YAML::Node node;
		  node["FilePath"] = FilePath;
		  node["Loop"] = Loop;
		  node["Volume"] = Volume;
		  node["PlayOnStart"] = PlayOnStart;
		  return node;
	  }

	  bool decode(const YAML::Node& node) override {
		  if (!node.IsMap()) return false;
		  if (node["FilePath"]) FilePath = node["FilePath"].as<std::string>();
		  if (node["Loop"]) Loop = node["Loop"].as<bool>();
		  if (node["Volume"]) Volume = node["Volume"].as<float>();
		  if (node["PlayOnStart"]) PlayOnStart = node["PlayOnStart"].as<bool>();
		  return true;
	  }

	  void inspector(SceneContext* context) override {
		  ImGui::PushID(this);

		  char filepathBuffer[256] = "";
		  strncpy_s(filepathBuffer, sizeof(filepathBuffer), FilePath.c_str(), _TRUNCATE);

		  ImGui::BeginGroup();

		  ImGui::Text("Effect");
		  ImGui::SameLine(100);
		  ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 24.0f);
		  if (ImGui::InputText("##EffectInput", filepathBuffer, sizeof(filepathBuffer))) {
			  FilePath = std::string(filepathBuffer);
		  }
		  ImGui::PopItemWidth();

		  ImGui::SameLine();
		  if (ImGui::SmallButton("x")) {
			  FilePath.clear();
			  m_EffectData.reset();
		  }
		  ImGui::EndGroup();

		  if (ImGui::BeginDragDropTarget()) {
			  if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
				  const char* droppedPath = (const char*)payload->Data;
				  FilePath = droppedPath;
				  m_EffectData = context->manager->resource->Load<EffectData>(FilePath);
			  }
			  ImGui::EndDragDropTarget();
		  }

		  ImGui::Checkbox("Loop", &Loop);
		  ImGui::SameLine();
		  ImGui::Checkbox("Play On Start", &PlayOnStart);

		  ImGui::PopID();
	  }

	bool Play(SceneContext* sceneContext) {
		if (!m_EffectData && !FilePath.empty()) {
			m_EffectData = sceneContext->manager->resource->Load<EffectData>(FilePath);
		}

		if (!m_EffectData || !sceneContext->manager->graphics)
			return false;

		if (Playing) {
			return false;
		}

		m_Handle = sceneContext->manager->graphics->GetEffectManager()->Play(m_EffectData->effect, 0.0f, 0.0f, 0.0f);

		Playing = true;

		return true;
	  }

	void Stop() {
		if (Playing) {
			m_Handle = -1;
		}
		Playing = false;
	}
};
