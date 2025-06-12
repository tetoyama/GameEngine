#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"

struct TextureData;

class TextureComponent : public IComponent {
public:
	int UV_Slice_X = 1;
	int UV_Slice_Y = 1;

	int AnimationNum = 0;

	TextureData* m_TextureData = nullptr;
};
