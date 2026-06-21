// =======================================================================
//
// TransformRotationMath.h
//
// DirectXMathのRoll-Pitch-Yaw規約に対応したQuaternion / Euler変換。
// EulerはInspector表示用であり、Quaternionを実際の回転値とする。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cmath>

#include <DirectXMath.h>

namespace TransformRotationMath {

inline constexpr float Pi = DirectX::XM_PI;
inline constexpr float TwoPi = DirectX::XM_2PI;

inline float UnwrapNear(float angle, float reference) noexcept {
	while(angle - reference > Pi) angle -= TwoPi;
	while(angle - reference < -Pi) angle += TwoPi;
	return angle;
}

inline DirectX::XMFLOAT4 NormalizeQuaternion(
	const DirectX::XMFLOAT4& quaternion
) noexcept {
	DirectX::XMVECTOR value = DirectX::XMLoadFloat4(&quaternion);
	const float lengthSquared = DirectX::XMVectorGetX(
		DirectX::XMVector4LengthSq(value)
	);

	if(lengthSquared <= 1.0e-12f || !std::isfinite(lengthSquared)) {
		return DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	value = DirectX::XMQuaternionNormalize(value);
	DirectX::XMFLOAT4 result{};
	DirectX::XMStoreFloat4(&result, value);
	return result;
}

inline DirectX::XMFLOAT4 QuaternionFromEuler(
	const DirectX::XMFLOAT3& eulerRadians
) noexcept {
	const DirectX::XMVECTOR quaternion =
		DirectX::XMQuaternionRotationRollPitchYaw(
			eulerRadians.x,
			eulerRadians.y,
			eulerRadians.z
		);

	DirectX::XMFLOAT4 result{};
	DirectX::XMStoreFloat4(&result, quaternion);
	return NormalizeQuaternion(result);
}

inline float QuaternionDot(
	const DirectX::XMFLOAT4& lhs,
	const DirectX::XMFLOAT4& rhs
) noexcept {
	return lhs.x * rhs.x + lhs.y * rhs.y +
		lhs.z * rhs.z + lhs.w * rhs.w;
}

inline DirectX::XMFLOAT4 AlignHemisphere(
	const DirectX::XMFLOAT4& quaternion,
	const DirectX::XMFLOAT4& reference
) noexcept {
	DirectX::XMFLOAT4 result = NormalizeQuaternion(quaternion);
	if(QuaternionDot(result, reference) < 0.0f) {
		result.x = -result.x;
		result.y = -result.y;
		result.z = -result.z;
		result.w = -result.w;
	}
	return result;
}

inline bool Equivalent(
	const DirectX::XMFLOAT4& lhs,
	const DirectX::XMFLOAT4& rhs,
	float epsilon = 1.0e-6f
) noexcept {
	const DirectX::XMFLOAT4 normalizedLhs = NormalizeQuaternion(lhs);
	const DirectX::XMFLOAT4 normalizedRhs = NormalizeQuaternion(rhs);
	return std::fabs(QuaternionDot(normalizedLhs, normalizedRhs)) >=
		1.0f - epsilon;
}

inline DirectX::XMFLOAT3 EulerFromQuaternion(
	const DirectX::XMFLOAT4& quaternion,
	const DirectX::XMFLOAT3& referenceRadians
) noexcept {
	const DirectX::XMFLOAT4 q = NormalizeQuaternion(quaternion);

	// XMQuaternionRotationRollPitchYaw(PitchX, YawY, RollZ)の逆変換。
	// DirectXMathはRoll(Z) -> Pitch(X) -> Yaw(Y)をグローバル軸で適用する。
	const float sinPitch = std::clamp(
		2.0f * (q.w * q.x - q.y * q.z),
		-1.0f,
		1.0f
	);
	const float pitch = std::asin(sinPitch);
	const float cosPitch = std::cos(pitch);

	float yaw = referenceRadians.y;
	float roll = referenceRadians.z;

	if(std::fabs(cosPitch) > 1.0e-4f) {
		yaw = std::atan2(
			2.0f * (q.w * q.y + q.x * q.z),
			1.0f - 2.0f * (q.x * q.x + q.y * q.y)
		);
		roll = std::atan2(
			2.0f * (q.w * q.z + q.x * q.y),
			1.0f - 2.0f * (q.x * q.x + q.z * q.z)
		);
	} else {
		// Pitchが±90度ではYawとRollを一意に分離できない。
		// 前回値に近い方を維持してInspector上の不連続な跳ねを抑える。
		const float coupled = 2.0f * std::atan2(q.z, q.w);

		DirectX::XMFLOAT3 preserveYaw{};
		preserveYaw.x = pitch;
		preserveYaw.y = referenceRadians.y;
		preserveYaw.z = sinPitch >= 0.0f
			? coupled + preserveYaw.y
			: coupled - preserveYaw.y;

		DirectX::XMFLOAT3 preserveRoll{};
		preserveRoll.x = pitch;
		preserveRoll.z = referenceRadians.z;
		preserveRoll.y = sinPitch >= 0.0f
			? preserveRoll.z - coupled
			: coupled - preserveRoll.z;

		preserveYaw.y = UnwrapNear(preserveYaw.y, referenceRadians.y);
		preserveYaw.z = UnwrapNear(preserveYaw.z, referenceRadians.z);
		preserveRoll.y = UnwrapNear(preserveRoll.y, referenceRadians.y);
		preserveRoll.z = UnwrapNear(preserveRoll.z, referenceRadians.z);

		const float yawCandidateError =
			std::pow(preserveYaw.y - referenceRadians.y, 2.0f) +
			std::pow(preserveYaw.z - referenceRadians.z, 2.0f);
		const float rollCandidateError =
			std::pow(preserveRoll.y - referenceRadians.y, 2.0f) +
			std::pow(preserveRoll.z - referenceRadians.z, 2.0f);

		if(yawCandidateError <= rollCandidateError) {
			yaw = preserveYaw.y;
			roll = preserveYaw.z;
		} else {
			yaw = preserveRoll.y;
			roll = preserveRoll.z;
		}
	}

	return DirectX::XMFLOAT3(
		UnwrapNear(pitch, referenceRadians.x),
		UnwrapNear(yaw, referenceRadians.y),
		UnwrapNear(roll, referenceRadians.z)
	);
}

} // namespace TransformRotationMath
