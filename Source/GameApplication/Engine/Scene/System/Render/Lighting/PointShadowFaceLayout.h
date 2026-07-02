#pragma once

#include <DirectXMath.h>
#include <array>
#include <cmath>

namespace PointShadowFaceLayout {

struct FaceBasis {
	DirectX::XMFLOAT3 direction;
	DirectX::XMFLOAT3 up;
};

inline constexpr int FaceCount = 6;

// Point Shadowは6面をAtlasへ格納し、HLSL側で1面を選択してPCFする。
// 低解像度設定では2048px Atlas / 4x4 grid = 1面512pxとなる。
// 旧90.5度では45度境界の余白が約2.2pxしかなく、既定5x5 PCF
// (radius=2)と比較フィルタのfootprintを吸収できなかった。
// 92度なら境界を約8.8px内側へ収め、辺・角で隣接Faceとの重複を確保できる。
inline constexpr float FieldOfViewDegrees = 92.0f;

inline const std::array<FaceBasis, FaceCount>& Faces() noexcept {
	static const std::array<FaceBasis, FaceCount> faces = {{
		{{ 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f,  0.0f}}, // +X
		{{-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f,  0.0f}}, // -X
		{{ 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f, -1.0f}}, // +Y
		{{ 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f,  1.0f}}, // -Y
		{{ 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f,  0.0f}}, // +Z
		{{ 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f,  0.0f}}, // -Z
	}};
	return faces;
}

inline float ProjectionFovRadians() noexcept {
	return DirectX::XMConvertToRadians(FieldOfViewDegrees);
}

inline int SelectFace(float x, float y, float z) noexcept {
	const float ax = std::abs(x);
	const float ay = std::abs(y);
	const float az = std::abs(z);

	if(ax >= ay && ax >= az){
		return x >= 0.0f ? 0 : 1;
	}
	if(ay >= az){
		return y >= 0.0f ? 2 : 3;
	}
	return z >= 0.0f ? 4 : 5;
}

} // namespace PointShadowFaceLayout
