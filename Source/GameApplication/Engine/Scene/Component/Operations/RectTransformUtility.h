// =======================================================================
//
// RectTransformUtility.h
//
// TransformComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

namespace RectTransformUtility {

inline TransformComponent Calculate(
	const Vector2& viewportSize,
	const SpriteRendererComponent& sprite,
	const TransformComponent& originalTransform
){
	TransformComponent adjustedTransform = originalTransform;

	if(viewportSize.x <= 0.0f || viewportSize.y <= 0.0f){
		return adjustedTransform;
	}

	// RectTransformのposition / scaleはViewportに対する正規化値として扱う。
	// scale=(1,1)はViewport全体の幅・高さに対応する。
	const Vector2 adjustedScale = {
		originalTransform.scale.x * viewportSize.x,
		originalTransform.scale.y * viewportSize.y
	};

	// AnchorはViewport左上を(0,0)、右下を(1,1)とした基準位置。
	const Vector2 anchoredPosition = {
		viewportSize.x * sprite.anchor.x,
		viewportSize.y * sprite.anchor.y
	};

	// Transform.positionもViewport比率で指定する。
	const Vector2 positionOffset = {
		originalTransform.position.x * viewportSize.x,
		originalTransform.position.y * viewportSize.y
	};

	// 描画Quadのローカル原点は中央なので、Pivotから中央までの差を加える。
	// Pivot=(0.5,0.5)では補正なし。
	// Pivot=(0,0)ではRect中心を右下方向へ半サイズ移動する。
	const Vector2 pivotToCenterOffset = {
		(0.5f - sprite.pivot.x) * adjustedScale.x,
		(0.5f - sprite.pivot.y) * adjustedScale.y
	};

	const Vector2 finalPosition = {
		anchoredPosition.x + positionOffset.x + pivotToCenterOffset.x,
		anchoredPosition.y + positionOffset.y + pivotToCenterOffset.y
	};

	adjustedTransform.position = Vector3(
		finalPosition.x,
		finalPosition.y,
		originalTransform.position.z
	);
	adjustedTransform.scale = Vector3(
		adjustedScale.x,
		adjustedScale.y,
		originalTransform.scale.z
	);

	return adjustedTransform;
}

inline TransformComponent Reverse(
	const Vector2& viewportSize,
	const SpriteRendererComponent& sprite,
	const TransformComponent& adjustedTransform
){
	TransformComponent virtualTransform = adjustedTransform;

	if(viewportSize.x <= 0.0f || viewportSize.y <= 0.0f){
		return virtualTransform;
	}

	const Vector2 virtualScale = {
		adjustedTransform.scale.x / viewportSize.x,
		adjustedTransform.scale.y / viewportSize.y
	};

	const Vector2 anchoredPosition = {
		viewportSize.x * sprite.anchor.x,
		viewportSize.y * sprite.anchor.y
	};

	const Vector2 pivotToCenterOffset = {
		(0.5f - sprite.pivot.x) * adjustedTransform.scale.x,
		(0.5f - sprite.pivot.y) * adjustedTransform.scale.y
	};

	const Vector2 positionOffset = {
		adjustedTransform.position.x - anchoredPosition.x - pivotToCenterOffset.x,
		adjustedTransform.position.y - anchoredPosition.y - pivotToCenterOffset.y
	};

	const Vector2 virtualPosition = {
		positionOffset.x / viewportSize.x,
		positionOffset.y / viewportSize.y
	};

	virtualTransform.position = Vector3(
		virtualPosition.x,
		virtualPosition.y,
		adjustedTransform.position.z
	);
	virtualTransform.scale = Vector3(
		virtualScale.x,
		virtualScale.y,
		adjustedTransform.scale.z
	);

	return virtualTransform;
}

} // namespace RectTransformUtility

// 既存コードを段階移行するための互換ラッパー。
inline TransformComponent TransformComponent::CalculateRectTransform(
	const Vector2& viewportSize,
	const SpriteRendererComponent& sprite,
	const TransformComponent& originalTransform
){
	return RectTransformUtility::Calculate(
		viewportSize,
		sprite,
		originalTransform
	);
}

inline TransformComponent TransformComponent::ReverseCalculateRectTransform(
	const Vector2& viewportSize,
	const SpriteRendererComponent& sprite,
	const TransformComponent& adjustedTransform
){
	return RectTransformUtility::Reverse(
		viewportSize,
		sprite,
		adjustedTransform
	);
}
