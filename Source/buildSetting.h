#pragma once
#define DEFAULT_SCENE "Asset\\Scene\\scene_title.yaml"

#define ASSET_PATH "Asset/"
#define TEMP_SAVE_PATH "Asset/Temp/"
#define CONFIG_PATH L"config.yaml"

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

#define TARGET_FPS (60)

#ifdef _DEBUG 

#define _DEBUG_BUILD
//#define _RELEASE_BUILD

#else

//#define _DEBUG_BUILD
#define _RELEASE_BUILD

#endif // _DEBUG

#define _EDITOR

