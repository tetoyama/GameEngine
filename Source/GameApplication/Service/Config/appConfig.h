// =======================================================================
// 
// appConfig.h
// 
// =======================================================================
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



struct APPCONFIG
{
	APPCONFIG(){
	}

	// アプリケーションの種類
	APPTYPE AppType = Editor;
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
