#pragma once
#include <vector>
#include <string>
#include "buildSetting.h"

enum APPTYPE{
	Editor = 0,
	Player,
	DebugPlayer,
	DebugEditor,
};

struct ShaderMaterial
{
	std::string filePath;
	std::string entryPoint;
};

struct APPCONFIG
{
	APPCONFIG(){

		ShaderMaterials.clear();

		ShaderMaterial unlitMaterial;
		unlitMaterial.filePath = "UnlitShader.hlsli";
		unlitMaterial.entryPoint = "ShadeMaterial_Unlit";

		ShaderMaterial PBRMaterial;
		PBRMaterial.filePath = "PBRShader.hlsli";
		PBRMaterial.entryPoint = "ShadeMaterial_PBR";

		ShaderMaterials.push_back(unlitMaterial);
		ShaderMaterials.push_back(PBRMaterial);
	}

	// アプリケーションの種類
	APPTYPE AppType = Editor;

	// Video Settings
	bool Vsync = false;
	std::string ShaderPath = "Shader/AutoGen/";
	std::vector<ShaderMaterial> ShaderMaterials;

	bool FullScreen = false;
	int Width = DEFAULT_WINDOW_WIDTH;
	int Height = DEFAULT_WINDOW_HEIGHT;

	// Audio Settings
	float MasterVolume = 1.0f;
	float BGMVolume = 1.0f;
	float SEVolume = 1.0f;

	// Other Settings

};
