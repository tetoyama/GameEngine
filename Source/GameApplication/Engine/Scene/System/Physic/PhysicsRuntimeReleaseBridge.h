// =======================================================================
//
// PhysicsRuntimeReleaseBridge.h
//
// Colliderの破棄・Inspector操作から、PhysX側の解放処理へ転送するBridge。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <mutex>

class ColliderComponent;

enum class PhysicsRuntimeReleaseType {
	Entity,
	Shape
};

class PhysicsRuntimeReleaseBridge {
public:
	using Hook = void(*)(
		ColliderComponent* collider,
		PhysicsRuntimeReleaseType releaseType,
		size_t shapeIndex
	);

	static void Install(Hook hook){
		std::scoped_lock lock(s_mutex);
		s_hook = hook;
	}

	static void ReleaseEntity(ColliderComponent* collider){
		Dispatch(collider, PhysicsRuntimeReleaseType::Entity, 0);
	}

	static void ReleaseShape(ColliderComponent* collider, size_t shapeIndex){
		Dispatch(collider, PhysicsRuntimeReleaseType::Shape, shapeIndex);
	}

private:
	static void Dispatch(
		ColliderComponent* collider,
		PhysicsRuntimeReleaseType releaseType,
		size_t shapeIndex
	){
		Hook hook = nullptr;
		{
			std::scoped_lock lock(s_mutex);
			hook = s_hook;
		}
		if(hook && collider){
			hook(collider, releaseType, shapeIndex);
		}
	}

	inline static std::mutex s_mutex;
	inline static Hook s_hook = nullptr;
};
