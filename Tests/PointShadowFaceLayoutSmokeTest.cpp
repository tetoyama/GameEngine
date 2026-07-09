#include <cassert>
#include <cmath>
#include <array>

#include "Engine/Scene/System/Render/Lighting/PointShadowFaceLayout.h"

namespace {

bool NearlyEqual(float a, float b, float epsilon = 0.0001f){
	return std::abs(a - b) <= epsilon;
}

void VerifyUnitOrthogonal(
	const PointShadowFaceLayout::FaceBasis& face
){
	using namespace DirectX;
	const XMVECTOR direction = XMLoadFloat3(&face.direction);
	const XMVECTOR up = XMLoadFloat3(&face.up);
	const float directionLength = XMVectorGetX(XMVector3Length(direction));
	const float upLength = XMVectorGetX(XMVector3Length(up));
	const float dot = XMVectorGetX(XMVector3Dot(direction, up));
	assert(NearlyEqual(directionLength, 1.0f));
	assert(NearlyEqual(upLength, 1.0f));
	assert(NearlyEqual(dot, 0.0f));
}

void VerifyBoundaryProjectsInside(float x, float y, float z){
	using namespace DirectX;
	const int faceIndex = PointShadowFaceLayout::SelectFace(x, y, z);
	assert(faceIndex >= 0);
	assert(faceIndex < PointShadowFaceLayout::FaceCount);

	const auto& face = PointShadowFaceLayout::Faces()[faceIndex];
	const XMVECTOR eye = XMVectorZero();
	const XMVECTOR direction = XMLoadFloat3(&face.direction);
	const XMVECTOR up = XMLoadFloat3(&face.up);
	const XMMATRIX view = XMMatrixLookAtLH(eye, direction, up);
	const XMMATRIX projection = XMMatrixPerspectiveFovLH(
		PointShadowFaceLayout::ProjectionFovRadians(),
		1.0f,
		0.1f,
		10.0f
	);

	XMVECTOR clip = XMVector4Transform(
		XMVectorSet(x, y, z, 1.0f),
		view
	);
	clip = XMVector4Transform(clip, projection);
	const float w = XMVectorGetW(clip);
	assert(w > 0.0f);
	const float ndcX = XMVectorGetX(clip) / w;
	const float ndcY = XMVectorGetY(clip) / w;

	// Face FOVのOverlap(現在92度)により、主軸切替境界でも投影端より内側になる。
	assert(std::abs(ndcX) < 1.0f);
	assert(std::abs(ndcY) < 1.0f);
}

} // namespace

int main(){
	const auto& faces = PointShadowFaceLayout::Faces();
	assert(faces.size() == PointShadowFaceLayout::FaceCount);

	for(const auto& face : faces){
		VerifyUnitOrthogonal(face);
	}

	assert(PointShadowFaceLayout::SelectFace( 1.0f, 0.0f, 0.0f) == 0);
	assert(PointShadowFaceLayout::SelectFace(-1.0f, 0.0f, 0.0f) == 1);
	assert(PointShadowFaceLayout::SelectFace(0.0f,  1.0f, 0.0f) == 2);
	assert(PointShadowFaceLayout::SelectFace(0.0f, -1.0f, 0.0f) == 3);
	assert(PointShadowFaceLayout::SelectFace(0.0f, 0.0f,  1.0f) == 4);
	assert(PointShadowFaceLayout::SelectFace(0.0f, 0.0f, -1.0f) == 5);

	// Shader側と同じtie-break: X、Y、Zの順で優先する。
	assert(PointShadowFaceLayout::SelectFace(1.0f, 1.0f, 0.0f) == 0);
	assert(PointShadowFaceLayout::SelectFace(0.0f, 1.0f, 1.0f) == 2);
	assert(PointShadowFaceLayout::SelectFace(1.0f, 1.0f, 1.0f) == 0);

	// 92度: 512px Faceで既定5x5 PCF(radius=2)のfootprintを吸収する
	// Overlapを確保する。93度以上は歪みと無駄が増えるため上限とする。
	assert(PointShadowFaceLayout::FieldOfViewDegrees > 90.0f);
	assert(PointShadowFaceLayout::FieldOfViewDegrees <= 92.0f);

	const std::array<std::array<float, 3>, 20> boundaryDirections = {{
		{{ 1.0f,  1.0f,  0.0f}},
		{{ 1.0f, -1.0f,  0.0f}},
		{{-1.0f,  1.0f,  0.0f}},
		{{-1.0f, -1.0f,  0.0f}},
		{{ 1.0f,  0.0f,  1.0f}},
		{{ 1.0f,  0.0f, -1.0f}},
		{{-1.0f,  0.0f,  1.0f}},
		{{-1.0f,  0.0f, -1.0f}},
		{{ 0.0f,  1.0f,  1.0f}},
		{{ 0.0f,  1.0f, -1.0f}},
		{{ 0.0f, -1.0f,  1.0f}},
		{{ 0.0f, -1.0f, -1.0f}},
		{{ 1.0f,  1.0f,  1.0f}},
		{{ 1.0f,  1.0f, -1.0f}},
		{{ 1.0f, -1.0f,  1.0f}},
		{{ 1.0f, -1.0f, -1.0f}},
		{{-1.0f,  1.0f,  1.0f}},
		{{-1.0f,  1.0f, -1.0f}},
		{{-1.0f, -1.0f,  1.0f}},
		{{-1.0f, -1.0f, -1.0f}},
	}};

	for(const auto& direction : boundaryDirections){
		VerifyBoundaryProjectsInside(
			direction[0],
			direction[1],
			direction[2]
		);
	}

	return 0;
}
