// =======================================================================
// 
// buildSetting.h
// 
// =======================================================================
#pragma once

// 既定で読み込む初期シーン
// Release起動時のフォールバック。Editorでは開いているSceneをPlayする。
#define DEFAULT_SCENE "Asset/Game/ElemenTactics/Scene/ElemenTactics.yaml"

// アセット・設定ファイルの共通パス
#define ASSET_PATH "Asset/"
#define TEMP_SAVE_PATH "Asset/Temp/"
#define APPLICATION_CONFIG_PATH L"Asset/ApplicationConfig.yaml"
#define EDITOR_CONFIG_PATH L"Asset/EngineConfig.yaml"

// メインウィンドウで使用する既定のポストエフェクトシェーダ
#define DEFAULT_WINDOW_POSTEFFECT_PS_PATH "Asset/Shader/PostEffectPS.cso"
#define DEFAULT_WINDOW_POSTEFFECT_VS_PATH "Asset/Shader/PostEffectVS.cso"

// 既定ウィンドウサイズ
#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

// 固定更新の基準となる目標 FPS
#define TARGET_FPS (60)

#ifdef _DEBUG

// Debugは通常のEditor経路を使う。
// Engine起動後にElemenTactics.yamlを開き、PlayしたScene自身をゲーム入口とする。
#define _DEBUG_BUILD
#define _EDITOR

#else

#define _RELEASE_BUILD
#define _EDITOR

#endif // _DEBUG
