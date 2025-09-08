#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"
#include "Backends/myVector2.h"

class ColliderComponent : public IComponent {
public:
	Vector2 anchor = { 0.5f, 0.5f }; // 中央
	Vector2 pivot = { 0.0f, 0.0f };  // 中心

	physx::PxRigidDynamic* pRigidbodyDynamic = nullptr;
	physx::PxRigidStatic* pRigidbodyStatic = nullptr;

	bool isDynamic = false;

	YAML::Node encode() override {
		YAML::Node node;
		node["isDynamic"] = isDynamic;


		return node;
	}

	bool decode(const YAML::Node& node) override {
		if (node["isDynamic"])
			isDynamic = node["isDynamic"].as<bool>();

		return true;
	}

	void inspector(SceneContext* context) override {
		ImGui::Text("Collider");

		ImGui::Checkbox("isDynamic", &isDynamic);
	}
};
