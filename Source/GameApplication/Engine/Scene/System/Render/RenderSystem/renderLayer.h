#pragma once

enum RenderLayer: int {
	Background2D = 0,
	Opaque3D,
	Transparent3D,
	SortTransparent3D,
	OverlayUI,
	Debug,
	MaxRenderLayer
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
