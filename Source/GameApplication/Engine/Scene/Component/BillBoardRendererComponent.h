#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

class BillBoardRendererComponent : public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;

		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::Text("BillBoardRendererComponent");
		ImGui::Text("FreezeXZ");
		ImGui::SameLine(100);
		if(ImGui::Button(FreezeXZ ? "Yes" : "No")){
			FreezeXZ = !FreezeXZ;
		}
	}

	bool FreezeXZ = false;
};
