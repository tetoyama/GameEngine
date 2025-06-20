#pragma once
#include "Interface/IComponent.h"

#include "GameApplication/Engine/Graphics/graphicsContext.h"
#include "Service/YAMLConverters.h"

class TextureComponent : public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		node["Component"] = "TextureComponent";
		if (m_TextureData) {
			node["FilePath"] = m_TextureData->FilePath;
		}
		node["Material"] = Material;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		if (!node.IsMap()) { return false; }

		if (node["FilePath"]) {
			m_TextureData->FilePath = node["Position"].as<std::string>();
		}
		if (node["Material"]) {
			Material = node["Material"].as<MATERIAL>();
		}
		return true;
	}
	int UV_Slice_X = 1;
	int UV_Slice_Y = 1;

	int AnimationNum = 0;

	TextureData* m_TextureData = nullptr;
	MATERIAL Material;
};
