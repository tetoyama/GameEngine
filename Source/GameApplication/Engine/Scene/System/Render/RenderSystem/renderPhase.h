// =======================================================================
// 
// renderPhase.h
// 
// =======================================================================
#pragma once
// 描画パイプラインの各フェーズを定義する列挙型
enum RenderPhase {
	PHASE_SHADOW = 0,
	PHASE_GBUFFER,
	PHASE_LIGHTING,
	PHASE_FOWARD,
};
