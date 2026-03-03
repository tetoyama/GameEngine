// =======================================================================
// 
// buildSetting.h
// 
// =======================================================================
#pragma once
#define DEFAULT_SCENE "Asset\\Scene\\scene_title.yaml"

#define ASSET_PATH "Asset/"
#define TEMP_SAVE_PATH "Asset/Temp/"
#define APPLICATION_CONFIG_PATH L"Asset/ApplicationConfig.yaml"
#define EDITOR_CONFIG_PATH L"Asset/EngineConfig.yaml"

#define DEFAULT_WINDOW_POSTEFFECT_PS_PATH "Asset/Shader/PostEffectPS.cso"
#define DEFAULT_WINDOW_POSTEFFECT_VS_PATH "Asset/Shader/PostEffectVS.cso"

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

#define TARGET_FPS (60)

#ifdef _DEBUG 

#define _DEBUG_BUILD
//#define _RELEASE_BUILD
#define _EDITOR

#else

//#define _DEBUG_BUILD
#define _RELEASE_BUILD
#define _EDITOR

#endif // _DEBUG


