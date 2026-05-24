// =======================================================================
// 
// cameraEntityData.h
// 
// =======================================================================
#pragma once
#include "Scene/Reference/EntityRef.h"

struct SceneContext;

class CameraComponent;
class TransformComponent;

// カメラエンティティの参照データを保持する構造体
struct CameraEntityData {

	CameraComponent* pCameraComponent = nullptr;
	TransformComponent* pTransformComponent = nullptr;

	// エンティティへの安全なリファレンス
	EntityRef ref;
};
