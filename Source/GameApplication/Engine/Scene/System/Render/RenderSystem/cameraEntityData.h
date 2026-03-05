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

	CameraComponent* cameraComponent = nullptr;
	TransformComponent* transformComponent = nullptr;

	// エンティティへの安全なリファレンス
	EntityRef ref;
};
