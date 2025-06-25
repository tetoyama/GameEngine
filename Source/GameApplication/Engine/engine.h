#pragma once
#include <memory>
#include <windows.h>

class EngineContext;

// ゲームエンジンの主要処理を担うクラス
class Engine
{
public:
	// 初期化処理（各サービスやウィンドウ・描画コンテキストの構築）
	void Initialize(std::shared_ptr<EngineContext> context, HINSTANCE hInstance, int nCmdShow);

	// 終了処理（リソースの解放、DXGIデバッグ出力など）
	void Shutdown(std::shared_ptr<EngineContext> context);

	// メインループ処理（イベント処理・更新・描画など）
	void Run(std::shared_ptr<EngineContext> context);
};
