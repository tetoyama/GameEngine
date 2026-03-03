// =======================================================================
// 
// effectData.h
// 
// =======================================================================
#pragma once
#include <string>
#include <debugapi.h>

#include "Backends/Effekseer/Effekseer.h"
#include "Backends/Effekseer/EffekseerRendererDX11.h"

// エフェクトリソースデータを保持するクラス
class EffectData {
public:
	std::string FilePath = "";

	Effekseer::EffectRef effect = nullptr;

	EffectData() {
		OutputDebugStringA("Created EffectData\n");
	}

	~EffectData() {
		OutputDebugStringA(("Destroyed EffectData: " + FilePath + "\n").c_str());
	}
};
