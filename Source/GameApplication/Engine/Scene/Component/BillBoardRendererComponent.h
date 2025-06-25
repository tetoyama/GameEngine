#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

class BillBoardRendererComponent : public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		node["Component"] = "BillBoardRendererComponent";

		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}

	void inspector() override{
		ImGui::Text("BillBoardRendererComponent");
	}

	bool RotateX = false;
};
