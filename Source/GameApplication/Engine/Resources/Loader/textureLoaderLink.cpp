// =======================================================================
//
// textureLoaderLink.cpp
//
// DirectXTexのリンク指示をGameEngine本体の翻訳単位へ閉じ込める。
// 公開ヘッダに#pragma comment(lib)を置くと、ヘッダをincludeしただけの
// 単体Smoke Testまで本体用ライブラリを要求するため、ここで一元化する。
//
// =======================================================================

#if defined(_DEBUG)
#pragma comment(lib, "DirectXTex_Debug.lib")
#else
#pragma comment(lib, "DirectXTex_Release.lib")
#endif
