#pragma once

struct HitInfo;
class CustomScriptComponent;

enum class ScriptCollisionEventType {
	CollisionEnter,
	CollisionStay,
	CollisionExit,
	TriggerEnter,
	TriggerExit
};

using ScriptCollisionDispatch = bool(*)(
	void* owner,
	CustomScriptComponent* script,
	ScriptCollisionEventType eventType,
	const HitInfo& hit
);
