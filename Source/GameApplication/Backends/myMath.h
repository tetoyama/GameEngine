#pragma once
#include "DirectXMath.h"
#include <assimp/matrix4x4.h>

namespace DirectX {
	inline DirectX::XMMATRIX XMMatrixLerp(const DirectX::XMMATRIX& a, const DirectX::XMMATRIX& b, float t) {
		using namespace DirectX;
		XMMATRIX result;
		for (int i = 0; i < 4; ++i) {
			XMVECTOR rowA = a.r[i];
			XMVECTOR rowB = b.r[i];
			result.r[i] = XMVectorLerp(rowA, rowB, t);
		}
		return result;
	}

	inline DirectX::XMMATRIX AssimpToXM(const aiMatrix4x4& mat) {
		using namespace DirectX;

		return XMMATRIX(
			mat.a1, mat.b1, mat.c1, mat.d1,
			mat.a2, mat.b2, mat.c2, mat.d2,
			mat.a3, mat.b3, mat.c3, mat.d3,
			mat.a4, mat.b4, mat.c4, mat.d4
		);
	}
}
