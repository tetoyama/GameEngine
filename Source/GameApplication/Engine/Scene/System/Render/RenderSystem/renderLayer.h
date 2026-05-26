// =======================================================================
// 
// renderLayer.h
// 
// =======================================================================
#pragma once

// 描画レイヤーを定義する列挙型
// レイヤーは番号の小さい順に描画される（BACKGROUND_2D が最初、DEBUG が最後）
enum class RenderLayer: int {
	BACKGROUND_2D      = 0,  // 2D 背景（スカイボックス等）
	OPAQUE_3D,               // 不透明 3D オブジェクト（GBuffer 経由）
	TRANSPARENT_3D,          // 半透明 3D オブジェクト（Zソートなし）
	SORT_TRANSPARENT_3D,     // 半透明 3D オブジェクト（カメラ距離でソート）
	OVERLAY_UI,              // 2D UI オーバーレイ
	DEBUG,                   // デバッグ描画（PhysX ワイヤーフレーム等）
	MAX_RENDER_LAYER         // レイヤー数（配列サイズ定義用）
};

// レイヤー名を文字列で取得するユーティリティ
inline const char* GetRenderLayerName(const RenderLayer& layer){
	switch(layer){
		case RenderLayer::BACKGROUND_2D:				return "Background 2D";
		case RenderLayer::OPAQUE_3D:					return "Opaque 3D";
		case RenderLayer::SORT_TRANSPARENT_3D:		return "Sorted Transparent 3D";
		case RenderLayer::TRANSPARENT_3D:			return "Transparent 3D";
		case RenderLayer::OVERLAY_UI:				return "Overlay UI";
		case RenderLayer::DEBUG:					return "Debug";
		default:									return "Unknown";
	}
}
