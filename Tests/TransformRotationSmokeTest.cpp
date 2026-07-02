#include "Component/Operations/TransformRotationMath.h"

#include <cassert>
#include <cmath>

namespace {

constexpr float kAngleEpsilon = 1.0e-4f;

void AssertNear(float lhs, float rhs, float epsilon = kAngleEpsilon){
	assert(std::fabs(lhs - rhs) <= epsilon);
}

void AssertEquivalent(
	const DirectX::XMFLOAT4& lhs,
	const DirectX::XMFLOAT4& rhs
){
	assert(TransformRotationMath::Equivalent(lhs, rhs, 1.0e-5f));
}

void TestCombinedRotationRoundTrip(){
	const DirectX::XMFLOAT3 expected(0.35f, -1.10f, 0.70f);
	const DirectX::XMFLOAT4 quaternion =
		TransformRotationMath::QuaternionFromEuler(expected);
	const DirectX::XMFLOAT3 actual =
		TransformRotationMath::EulerFromQuaternion(
			quaternion,
			DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f)
		);

	AssertNear(actual.x, expected.x);
	AssertNear(actual.y, expected.y);
	AssertNear(actual.z, expected.z);
	AssertEquivalent(
		TransformRotationMath::QuaternionFromEuler(actual),
		quaternion
	);
}

void TestQuaternionSignEquivalence(){
	const DirectX::XMFLOAT4 quaternion =
		TransformRotationMath::QuaternionFromEuler(
			DirectX::XMFLOAT3(0.2f, 0.8f, -0.4f)
		);
	const DirectX::XMFLOAT4 negated(
		-quaternion.x,
		-quaternion.y,
		-quaternion.z,
		-quaternion.w
	);

	assert(TransformRotationMath::Equivalent(quaternion, negated));
	const DirectX::XMFLOAT4 aligned =
		TransformRotationMath::AlignHemisphere(negated, quaternion);
	assert(TransformRotationMath::QuaternionDot(aligned, quaternion) > 0.0f);
}

void TestPositiveGimbalContinuity(){
	const DirectX::XMFLOAT3 expected(
		DirectX::XM_PIDIV2,
		0.80f,
		-0.30f
	);
	const DirectX::XMFLOAT4 quaternion =
		TransformRotationMath::QuaternionFromEuler(expected);
	const DirectX::XMFLOAT3 actual =
		TransformRotationMath::EulerFromQuaternion(quaternion, expected);

	AssertNear(actual.x, expected.x);
	AssertNear(actual.y, expected.y);
	AssertNear(actual.z, expected.z);
	AssertEquivalent(
		TransformRotationMath::QuaternionFromEuler(actual),
		quaternion
	);
}

void TestNegativeGimbalContinuity(){
	const DirectX::XMFLOAT3 expected(
		-DirectX::XM_PIDIV2,
		-0.65f,
		0.45f
	);
	const DirectX::XMFLOAT4 quaternion =
		TransformRotationMath::QuaternionFromEuler(expected);
	const DirectX::XMFLOAT3 actual =
		TransformRotationMath::EulerFromQuaternion(quaternion, expected);

	AssertNear(actual.x, expected.x);
	AssertNear(actual.y, expected.y);
	AssertNear(actual.z, expected.z);
	AssertEquivalent(
		TransformRotationMath::QuaternionFromEuler(actual),
		quaternion
	);
}

void TestUnwrappedYawContinuity(){
	const DirectX::XMFLOAT3 expected(
		0.15f,
		DirectX::XM_2PI * 2.0f + 0.40f,
		-0.25f
	);
	const DirectX::XMFLOAT4 quaternion =
		TransformRotationMath::QuaternionFromEuler(expected);
	const DirectX::XMFLOAT3 actual =
		TransformRotationMath::EulerFromQuaternion(quaternion, expected);

	AssertNear(actual.x, expected.x);
	AssertNear(actual.y, expected.y);
	AssertNear(actual.z, expected.z);
	AssertEquivalent(
		TransformRotationMath::QuaternionFromEuler(actual),
		quaternion
	);
}

} // namespace

int main(){
	TestCombinedRotationRoundTrip();
	TestQuaternionSignEquivalence();
	TestPositiveGimbalContinuity();
	TestNegativeGimbalContinuity();
	TestUnwrappedYawContinuity();
	return 0;
}
