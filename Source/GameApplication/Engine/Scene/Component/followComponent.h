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
class FollowComponent: public IComponent {
public:
	Entity targetEntity{};
	std::string boneName;
	Vector3 positionOffset = Vector3(0.0f, 0.0f, 0.0f);
	bool followPosition = true;
	bool followRotation = false;

	// 現StorageがIComponentを要求する移行期間中の互換API。
	YAML::Node encode() override;
	bool decode(SceneContext* context, const YAML::Node& node) override;
	void inspector(SceneContext* context) override;
};

#include "Operations/FollowComponentOperations.h"
