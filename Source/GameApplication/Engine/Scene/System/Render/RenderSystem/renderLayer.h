// =======================================================================
// 
// renderLayer.h
// 
// =======================================================================
#pragma once

// 描画レイヤーを定義する列挙型
// レイヤーは番号の小さい順に描画される（Background2D が最初、Debug が最後）
enum RenderLayer: int{
	Background2D      = 0,  // 2D 背景（スカイボックス等）
	Opaque3D,               // 不透明 3D オブジェクト（GBuffer 経由）
	Transparent3D,          // 半透明 3D オブジェクト（Zソートなし）
	SortTransparent3D,      // 半透明 3D オブジェクト（カメラ距離でソート）
	OverlayUI,              // 2D UI オーバーレイ
	Debug,                  // デバッグ描画（PhysX ワイヤーフレーム等）
	MaxRenderLayer          // レイヤー数（配列サイズ定義用）
};

// レイヤー名を文字列で取得するユーティリティ
inline const char* GetRenderLayerName(const RenderLayer& layer){
	switch(layer){
		case RenderLayer::Background2D:				return "Background 2D";
		case RenderLayer::Opaque3D:					return "Opaque 3D";
		case RenderLayer::SortTransparent3D:		return "Sorted Transparent 3D";
		case RenderLayer::Transparent3D:			return "Transparent 3D";
		case RenderLayer::OverlayUI:				return "Overlay UI";
		case RenderLayer::Debug:					return "Debug";
		default:									return "Unknown";
	}
}
