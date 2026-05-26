// =======================================================================
// 
// appConfig.h
// 
// =======================================================================
#pragma once
#include <vector>
#include <string>
#include "buildSetting.h"

enum class APPTYPE{
	EDITOR = 0,
	PLAYER,
	DEBUG_PLAYER,
	DEBUG_EDITOR,
};



struct APPCONFIG
{
	APPCONFIG(){
	}

	// アプリケーションの種類
	APPTYPE AppType = APPTYPE::EDITOR;
	std::string startSceneFilePath = DEFAULT_SCENE;

	// Video Settings
	bool Vsync = false;

	bool FullScreen = false;
	int Width = DEFAULT_WINDOW_WIDTH;
	int Height = DEFAULT_WINDOW_HEIGHT;

	// Audio Settings
	float MasterVolume = 1.0f;
	float BGMVolume = 1.0f;
	float SEVolume = 1.0f;

	// Other Settings
	std::string templateDir = "Asset/Prefab/template";

};
