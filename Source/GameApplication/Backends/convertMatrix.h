#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include "Effekseer/EffekseerRendererDX11.h"

//---------------------------------------------------------------------
// @brief  DirectX::XMMATRIX を Effekseer::Matrix44 に変換する
// @param  matrix  変換元の XMMATRIX
// @return Effekseer 用の Matrix44
//
// XMMATRIX は SIMD 用の内部表現を持つため、一度 XMFLOAT4X4 に
// 展開してから各要素を Effekseer 側の行列へコピーする。
//---------------------------------------------------------------------
inline Effekseer::Matrix44 ConvertXMMATRIXToMatrix44(const DirectX::XMMATRIX& matrix){
	Effekseer::Matrix44 result;

	// XMMATRIX を通常の 4x4 行列形式へ展開
	DirectX::XMFLOAT4X4 tempMatrix;
	DirectX::XMStoreFloat4x4(&tempMatrix, matrix);

	// 各行・各列の値を Effekseer::Matrix44 に転写
	for(int row = 0; row < 4; ++row){
		for(int col = 0; col < 4; ++col){
			result.Values[row][col] = tempMatrix.m[row][col];
		}
	}

	return result;
}