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
		node["UV_Slice_X"] = UV_Slice_X;
		node["UV_Slice_Y"] = UV_Slice_Y;
		node["AnimationNum"] = AnimationNum;


		return node;
	}

	bool decode(const YAML::Node& node) override{
		if (!node.IsMap()) { return false; }

		if (m_TextureData && node["FilePath"]) {
			m_TextureData->FilePath = node["FilePath"].as<std::string>();
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
	int UV_Slice_X = 1;
	int UV_Slice_Y = 1;

	int AnimationNum = 0;

	TextureData* m_TextureData = nullptr;
	MATERIAL Material;
};
