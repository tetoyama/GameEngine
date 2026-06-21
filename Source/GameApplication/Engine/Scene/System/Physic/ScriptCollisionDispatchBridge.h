// =======================================================================
//
// ScriptCollisionDispatchBridge.h
//
// PhysX callbackからScript通知を即時実行せず、Physics Taskへ転送するBridge。
//
// =======================================================================
#pragma once

#include "System/Physic/HitInfo.h"

#include <mutex>

class CustomScriptComponent;

enum class ScriptCollisionEventType {
	CollisionEnter,
	CollisionStay,
	CollisionExit,
	TriggerEnter,
	TriggerExit
};

class ScriptCollisionDispatchBridge {
public:
	using Hook = bool(*)(
		void* owner,
		CustomScriptComponent* script,
		ScriptCollisionEventType eventType,
		const HitInfo& hit
	);

	static void Install(void* owner, Hook hook){
		std::scoped_lock lock(s_mutex);
		s_owner = owner;
		s_hook = hook;
	}

	static void Remove(void* owner){
		std::scoped_lock lock(s_mutex);
		if(s_owner != owner) return;
		s_hook = nullptr;
		s_owner = nullptr;
	}

	static bool TryDispatch(
		CustomScriptComponent* script,
		ScriptCollisionEventType eventType,
		const HitInfo& hit
	){
		Hook hook = nullptr;
		void* owner = nullptr;
		{
			std::scoped_lock lock(s_mutex);
			hook = s_hook;
			owner = s_owner;
		}

		return hook && owner
			? hook(owner, script, eventType, hit)
			: false;
	}

private:
	inline static std::mutex s_mutex;
	inline static Hook s_hook = nullptr;
	inline static void* s_owner = nullptr;
};
