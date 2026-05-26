// =======================================================================
// 
// appConfig.h
// 
// =======================================================================
#pragma once
#include <vector>
#include <string>
#include "buildSetting.h"

enum APPTYPE {
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
	APPTYPE appType= Editor;
	std::string startSceneFilePath = DEFAULT_SCENE;

	// Video Settings
	bool vsync= false;

	bool fullScreen= false;
	int width= DEFAULT_WINDOW_WIDTH;
	int height= DEFAULT_WINDOW_HEIGHT;

	// Audio Settings
	float masterVolume= 1.0f;
	float bgmvolume= 1.0f;
	float sevolume= 1.0f;

	// Other Settings
	std::string templateDir = "Asset/Prefab/template";
};
