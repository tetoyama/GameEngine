// =======================================================================
// 
// RenderPassContext.h
// 
// =======================================================================
#pragma once
#include <memory>
#include <d3d11.h>

#include "Backends/myVector2.h"
#include "Resources/Data/shaderData.h"
#include "../renderPhase.h"
#include "../renderLayer.h"
#include "../CameraEntityData.h"
#include "Scene/Reference/EntityRef.h"

// 1 フレームのレンダーパス実行に必要なコンテキスト情報
// カメラのビュー/プロジェクション行列・レイヤー表示設定・スクリーンサイズを保持する
struct RenderPassContext {

	RenderPassContext(
		const RenderPhase& renderPhase,
		bool* renderLayer,
		const CameraEntityData& data,
		const Vector2& setScreenSize
	);

	bool renderLayerVisibility[RenderLayer::MAX_RENDER_LAYER]; // 各レンダーレイヤーの表示フラグ
	RenderPhase passPhase = RenderPhase::PHASE_SHADOW;       // このパスで処理するフェーズ

	DirectX::XMFLOAT4 CameraPosition = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f); // カメラのワールド位置

	DirectX::XMMATRIX viewMatrix       = DirectX::XMMatrixIdentity(); // ビュー行列
	DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixIdentity(); // プロジェクション行列

	CameraEntityData cameraData;                         // カメラエンティティのデータ（ポストエフェクト等）
	Vector2 screenSize = Vector2(1280.0f, 720.0f);       // レンダリング解像度
};

// 半透明オブジェクトのソート用データ（カメラからの距離二乗で降順ソートする）
struct TransparentDrawItem {
	EntityRef ref;       // 描画対象エンティティ
	float distanceSq;    // カメラからの距離の二乗（降順ソートに使用）
};

// スプライト（2D UI）のソート用データ（OrderInLayer で昇順ソートする）
struct SpriteDrawItem {
	EntityRef ref;        // 描画対象エンティティ
	int orderInLayer;     // レイヤー内の描画順序
};
