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

	const Vector2 referenceResolution = {1.0f, 1.0f};
	const float screenAspect = viewportSize.x / viewportSize.y;
	const float referenceAspect =
		referenceResolution.x / referenceResolution.y;
	const float aspectRatioScaleX = referenceAspect / screenAspect;

	const Vector2 anchoredPosition = {
		viewportSize.x * sprite.anchor.x,
		viewportSize.y * sprite.anchor.y
	};

	const Vector2 adjustedScale = {
		originalTransform.scale.x * aspectRatioScaleX /
			referenceResolution.x * viewportSize.x,
		originalTransform.scale.y /
			referenceResolution.y * viewportSize.y
	};

	const Vector2 pivotOffset = {
		adjustedScale.x * -sprite.pivot.x,
		adjustedScale.y * -sprite.pivot.y
	};

	const Vector2 positionOffset = {
		originalTransform.position.x * aspectRatioScaleX /
			referenceResolution.x * viewportSize.x,
		originalTransform.position.y /
			referenceResolution.y * viewportSize.y
	};

	const Vector2 finalPosition = {
		anchoredPosition.x - pivotOffset.x + positionOffset.x,
		anchoredPosition.y - pivotOffset.y + positionOffset.y
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

	const Vector2 referenceResolution = {1.0f, 1.0f};
	const float screenAspect = viewportSize.x / viewportSize.y;
	const float referenceAspect =
		referenceResolution.x / referenceResolution.y;
	const float aspectRatioScaleX = referenceAspect / screenAspect;

	const Vector2 anchoredPosition = {
		viewportSize.x * sprite.anchor.x,
		viewportSize.y * sprite.anchor.y
	};

	const Vector2 virtualScale = {
		adjustedTransform.scale.x / viewportSize.x *
			referenceResolution.x / aspectRatioScaleX,
		adjustedTransform.scale.y / viewportSize.y *
			referenceResolution.y
	};

	const Vector2 pivotOffset = {
		adjustedTransform.scale.x * -sprite.pivot.x,
		adjustedTransform.scale.y * -sprite.pivot.y
	};

	const Vector2 anchoredDifference = {
		adjustedTransform.position.x - anchoredPosition.x + pivotOffset.x,
		adjustedTransform.position.y - anchoredPosition.y + pivotOffset.y
	};

	const Vector2 virtualPosition = {
		anchoredDifference.x / viewportSize.x *
			referenceResolution.x / aspectRatioScaleX,
		anchoredDifference.y / viewportSize.y *
			referenceResolution.y
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
