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
inline constexpr float FieldOfViewDegrees = 90.5f;

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
