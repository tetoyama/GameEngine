// =======================================================================
// 
// IRenderPass.h
// 
// =======================================================================
#pragma once

class RenderSystem;
struct SceneManagerContext;

struct RenderPassContext;

// レンダーパスの基底インターフェース
// 各描画パス（GBuffer, Shadow, Forward, PostEffect 等）はこのインターフェースを実装する
// RenderSystem が登録されたパスを順番に Execute() で呼び出す
class IRenderPass{
public:
	virtual ~IRenderPass() = default;

	// パスの初期化（シェーダーやリソースの生成）
	virtual void Initialize(RenderSystem* renderSystem, SceneManagerContext* context) = 0;

	// パスの終了処理（リソースの解放）
	virtual void Finalize() = 0;

	// 1 フレームの描画処理を実行する
	virtual void Execute(const RenderPassContext& ctx) = 0;

	SceneManagerContext* m_pContext      = nullptr;  // シーンコンテキストへの参照
	RenderSystem*        m_pRenderSystem = nullptr;  // 親となる RenderSystem への参照
};
