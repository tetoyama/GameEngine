// =======================================================================
//
// PhysicsRuntimeStore.h
//
// ECS Colliderデータから分離したPhysXネイティブ資源の所有Store。
// Collider側の生ポインタは移行期間中の非所有エイリアスとして扱う。
//
// =======================================================================
#pragma once

#include "Backends/PhysX/PxPhysicsAPI.h"
#include "Scene/Entity/Entity.h"

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

struct SceneContext;

struct PhysicsRuntimeKey {
	SceneContext* context = nullptr;
	Entity entity;

	bool operator==(const PhysicsRuntimeKey& other) const noexcept {
		return context == other.context && entity == other.entity;
	}
};

struct PhysicsRuntimeKeyHash {
	size_t operator()(const PhysicsRuntimeKey& key) const noexcept {
		const size_t contextHash = std::hash<void*>{}(key.context);
		const size_t entityHash = std::hash<Entity>{}(key.entity);
		return contextHash ^ (entityHash + 0x9e3779b9u +
			(contextHash << 6) + (contextHash >> 2));
	}
};

struct PhysicsShapeRuntime {
	// PxShapeはExclusive ShapeとしてActorが所有するため、非所有参照だけを保持する。
	physx::PxShape* shape = nullptr;

	// 以下は生成時に得た参照をStoreが所有する。
	physx::PxMaterial* material = nullptr;
	physx::PxHeightField* heightField = nullptr;
	physx::PxTriangleMesh* triangleMesh = nullptr;
	physx::PxConvexMesh* convexMesh = nullptr;
};

struct PhysicsEntityRuntime {
	physx::PxRigidDynamic* dynamicActor = nullptr;
	physx::PxRigidStatic* staticActor = nullptr;
	void* actorUserData = nullptr;
	void (*destroyActorUserData)(void*) = nullptr;
	std::vector<PhysicsShapeRuntime> shapes;
};

class PhysicsRuntimeStore {
public:
	PhysicsRuntimeStore() = default;
	~PhysicsRuntimeStore(){
		ReleaseAll();
	}

	PhysicsRuntimeStore(const PhysicsRuntimeStore&) = delete;
	PhysicsRuntimeStore& operator=(const PhysicsRuntimeStore&) = delete;

	bool Contains(SceneContext* context, Entity entity) const noexcept {
		return m_entries.contains({context, entity});
	}

	void Adopt(
		SceneContext* context,
		Entity entity,
		PhysicsEntityRuntime runtime
	){
		PhysicsRuntimeKey key{context, entity};
		Release(key);
		m_entries.emplace(key, std::move(runtime));
	}

	physx::PxRigidActor* GetActor(
		SceneContext* context,
		Entity entity
	) const noexcept {
		auto iterator = m_entries.find({context, entity});
		if(iterator == m_entries.end()) return nullptr;
		const PhysicsEntityRuntime& runtime = iterator->second;
		if(runtime.dynamicActor) return runtime.dynamicActor;
		return runtime.staticActor;
	}

	void Release(SceneContext* context, Entity entity) noexcept {
		Release(PhysicsRuntimeKey{context, entity});
	}

	void ReleaseMissing(
		const std::vector<PhysicsRuntimeKey>& liveKeys
	) noexcept {
		for(auto iterator = m_entries.begin(); iterator != m_entries.end();){
			bool live = false;
			for(const PhysicsRuntimeKey& key : liveKeys){
				if(iterator->first == key){
					live = true;
					break;
				}
			}

			if(live){
				++iterator;
				continue;
			}

			ReleaseRuntime(iterator->second);
			iterator = m_entries.erase(iterator);
		}
	}

	void ReleaseAll() noexcept {
		for(auto& [key, runtime] : m_entries){
			(void)key;
			ReleaseRuntime(runtime);
		}
		m_entries.clear();
	}

	size_t Size() const noexcept {
		return m_entries.size();
	}

private:
	void Release(const PhysicsRuntimeKey& key) noexcept {
		auto iterator = m_entries.find(key);
		if(iterator == m_entries.end()) return;
		ReleaseRuntime(iterator->second);
		m_entries.erase(iterator);
	}

	static void ReleaseRuntime(PhysicsEntityRuntime& runtime) noexcept {
		physx::PxRigidActor* actor = runtime.dynamicActor
			? static_cast<physx::PxRigidActor*>(runtime.dynamicActor)
			: static_cast<physx::PxRigidActor*>(runtime.staticActor);

		if(actor){
			actor->userData = nullptr;
			actor->release();
		}
		runtime.dynamicActor = nullptr;
		runtime.staticActor = nullptr;

		if(runtime.destroyActorUserData && runtime.actorUserData){
			runtime.destroyActorUserData(runtime.actorUserData);
		}
		runtime.actorUserData = nullptr;

		// Actor release後にShapeが保持していた参照が外れるため、
		// Storeが保持する生成時参照を安全に解放できる。
		for(PhysicsShapeRuntime& shape : runtime.shapes){
			shape.shape = nullptr;
			ReleaseRef(shape.material);
			ReleaseRef(shape.heightField);
			ReleaseRef(shape.triangleMesh);
			ReleaseRef(shape.convexMesh);
		}
		runtime.shapes.clear();
	}

	template<typename T>
	static void ReleaseRef(T*& resource) noexcept {
		if(!resource) return;
		resource->release();
		resource = nullptr;
	}

	std::unordered_map<
		PhysicsRuntimeKey,
		PhysicsEntityRuntime,
		PhysicsRuntimeKeyHash
	> m_entries;
};
