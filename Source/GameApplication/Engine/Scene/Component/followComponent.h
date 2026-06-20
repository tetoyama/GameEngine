// =======================================================================
//
// followComponent.h
//
// =======================================================================
#pragma once

#include <string>

#include "Interface/IComponent.h"
#include "Backends/myVector3.h"
#include "Entity/Entity.h"

// 指定Entityまたはボーンへの追従設定を保持するComponent。
// YAML・Inspector処理はFollowComponentOperationsへ分離する。
struct FollowComponent {
	Entity targetEntity{};
	std::string boneName;
	Vector3 positionOffset = Vector3(0.0f, 0.0f, 0.0f);
	bool followPosition = true;
	bool followRotation = false;

	YAML::Node encode();
	bool decode(SceneContext* context, const YAML::Node& node);
	void inspector(SceneContext* context);
};

#include "Operations/FollowComponentOperations.h"
