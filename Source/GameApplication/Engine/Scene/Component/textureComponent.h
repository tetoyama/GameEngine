#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"
#include "GameApplication/Engine/Graphics/graphicsContext.h"
struct TextureData;
struct MATERIAL;

class TextureComponent : public IComponent {
public:
	YAML::Node encode() override{
		YAML::Node node;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		return true;
	}
	int UV_Slice_X = 1;
	int UV_Slice_Y = 1;

	int AnimationNum = 0;

	TextureData* m_TextureData = nullptr;
	MATERIAL Material;
};
