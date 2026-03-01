#pragma once
#include "Interface/IComponent.h"
#include "BackEnds/YAMLConverters.h"
#include "BackEnds/ImGuiFunc.h"

// Forward declarations for PhysX types
namespace physx {
	class PxController;
	class PxMaterial;
}

class CharacterControllerComponent : public IComponent {
public:
	~CharacterControllerComponent() {
		// PxController is released by PhysicSystem via PxControllerManager::release()
		pxController = nullptr;
		pxMaterial   = nullptr;
	}

	// ============================================================
	// Configurable parameters (Inspector / YAML)
	// ============================================================
	BEGIN_REFLECT(CharacterControllerComponent)

		REFLECT_FIELD(float, radius,      0.4f)
		REFLECT_FIELD(float, height,      1.0f)
		REFLECT_FIELD(float, stepOffset,  0.3f)
		REFLECT_FIELD(float, slopeLimit,  45.0f)  // degrees
		REFLECT_FIELD(float, gravity,    -9.8f)
		REFLECT_FIELD(float, jumpPower,   5.0f)

	// ============================================================
	// Runtime state – written by script, read by PhysicSystem
	// ============================================================

	// Horizontal movement input (world-space, set each frame by script)
	float inputVelocityX = 0.0f;
	float inputVelocityZ = 0.0f;

	// True while the player wants to jump (script sets, PhysicSystem consumes)
	bool jumpPressed = false;

	// ============================================================
	// Runtime state – written by PhysicSystem, read by script
	// ============================================================
	bool  isGrounded       = false;
	float verticalVelocity = 0.0f;

	// ============================================================
	// PhysX internals – managed exclusively by PhysicSystem
	// ============================================================
	physx::PxController* pxController = nullptr;
	physx::PxMaterial*   pxMaterial   = nullptr;
	bool needsCreate = true;

	// ============================================================
	// IComponent
	// ============================================================
	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* /*context*/, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		needsCreate = true;
		return true;
	}

	void inspector(SceneContext* /*context*/) override {
		ImGui::Text("CharacterController Component");
		INSPECTOR_FIELDS();
		ImGui::Separator();
		ImGui::Text("isGrounded: %s", isGrounded ? "true" : "false");
		ImGui::Text("verticalVelocity: %.2f", verticalVelocity);
	}
};
