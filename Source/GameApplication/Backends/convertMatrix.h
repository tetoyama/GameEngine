#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "Effekseer/EffekseerRendererDX11.h"

inline Effekseer::Matrix44 ConvertXMMATRIXToMatrix44(const DirectX::XMMATRIX& matrix) {
	Effekseer::Matrix44 result;
	DirectX::XMFLOAT4X4 tempMatrix;
	DirectX::XMStoreFloat4x4(&tempMatrix, matrix);

	//行列の各成分をEffekseerの行列にコピー
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			result.Values[row][col] = tempMatrix.m[row][col];
		}
	}
	return result;
}
