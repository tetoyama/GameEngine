#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"
#include "Service/ImGuiFunc.h"
#include "Backends/myVector2.h"
#include "registry/SystemRegistry.h"
#include "Source/GameApplication/BackEnds/PhysX/PxPhysicsAPI.h"

enum class ColliderType {
	Box,
	Sphere,
	Capsule,
	Mesh
};

struct ColliderShape {
	ColliderType type = ColliderType::Box;

	Vector3 size = {1.0f, 1.0f, 1.0f};   // BoxのサイズやSphereの半径
	Vector3 offset = {0.0f, 0.0f, 0.0f}; // Transformとの相対位置
	float radius = 0.5f;                  // Sphere/Capsule用
	float height = 1.0f;                  // Capsule用

	// マテリアル情報
	float staticFriction = 0.5f;
	float dynamicFriction = 0.5f;
	float restitution = 0.1f;

	// レイヤー
	uint32_t collisionLayer = 0;

	// Rigidbodyの回転軸固定
	bool lockRotX = false;
	bool lockRotY = false;
	bool lockRotZ = false;

	physx::PxShape* pxShape = nullptr;
	physx::PxMaterial* pxMaterial = nullptr;
};

class ColliderComponent : public IComponent {
public:
	~ColliderComponent(){
		for(auto& col : colliders){
			if(col.pxShape){
				 col.pxShape = nullptr;
			}
			if(col.pxMaterial){
				col.pxMaterial->release(); col.pxMaterial = nullptr;
			}
		}
	}

	physx::PxRigidDynamic* pRigidbodyDynamic = nullptr;
	physx::PxRigidStatic* pRigidbodyStatic = nullptr;
	bool needsUpdate = false;

	BEGIN_REFLECT(ColliderComponent)
	REFLECT_FIELD(bool, isDynamic, false)

	std::vector<ColliderShape> colliders; // 複数のコライダーを保持

	YAML::Node encode() override{
		YAML::Node node;
		ENCODE_FIELDS(node);
		for(auto& col : colliders){
			YAML::Node colNode;
			colNode["type"] = static_cast<int>(col.type);

			colNode["size"] = YAML::Node();
			colNode["size"].push_back(col.size.x);
			colNode["size"].push_back(col.size.y);
			colNode["size"].push_back(col.size.z);

			colNode["offset"] = YAML::Node();
			colNode["offset"].push_back(col.offset.x);
			colNode["offset"].push_back(col.offset.y);
			colNode["offset"].push_back(col.offset.z);

			colNode["radius"] = col.radius;
			colNode["height"] = col.height;

			// マテリアル
			colNode["staticFriction"] = col.staticFriction;
			colNode["dynamicFriction"] = col.dynamicFriction;
			colNode["restitution"] = col.restitution;

			// レイヤー
			colNode["collisionLayer"] = col.collisionLayer;

			// 回転軸固定
			colNode["lockRotX"] = col.lockRotX;
			colNode["lockRotY"] = col.lockRotY;
			colNode["lockRotZ"] = col.lockRotZ;

			node["colliders"].push_back(colNode);
		}
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		DECODE_FIELDS(node);
		if(node["colliders"]){
			for(auto& colNode : node["colliders"]){
				ColliderShape col;
				col.type = static_cast<ColliderType>(colNode["type"].as<int>());

				auto sz = colNode["size"];
				col.size = Vector3(sz[0].as<float>(), sz[1].as<float>(), sz[2].as<float>());

				auto off = colNode["offset"];
				col.offset = Vector3(off[0].as<float>(), off[1].as<float>(), off[2].as<float>());

				col.radius = colNode["radius"].as<float>();
				col.height = colNode["height"].as<float>();

				// マテリアル
				col.staticFriction = colNode["staticFriction"].as<float>();
				col.dynamicFriction = colNode["dynamicFriction"].as<float>();
				col.restitution = colNode["restitution"].as<float>();

				// レイヤー
				col.collisionLayer = colNode["collisionLayer"].as<uint32_t>();

				// 回転軸固定
				col.lockRotX = colNode["lockRotX"].as<bool>();
				col.lockRotY = colNode["lockRotY"].as<bool>();
				col.lockRotZ = colNode["lockRotZ"].as<bool>();

				colliders.push_back(col);
			}
		}
		return true;
	}


	void inspector(SceneContext* context) override{
		ImGui::Text("Collider Component");

		INSPECTOR_FIELDS();

		auto Physics = context->system->GetSystem<PhysicSystem>();

		if(ImGui::Button("Add Collider")){
			colliders.push_back(ColliderShape());
			needsUpdate = true; // 即時反映ではなく、更新フラグだけ
		}

		for(size_t i = 0; i < colliders.size(); ++i){
			ImGui::Separator();
			ImGui::Text("Collider %d", (int)i);

			ImGui::SameLine();
			if(ImGui::Button(("Remove##" + std::to_string(i)).c_str())){
				// 削除前に PhysX リソースを解放
				if(colliders[i].pxShape){
					colliders[i].pxShape->release();
					colliders[i].pxShape = nullptr;
				}
				if(colliders[i].pxMaterial){
					colliders[i].pxMaterial->release();
					colliders[i].pxMaterial = nullptr;
				}
				colliders.erase(colliders.begin() + i);
				needsUpdate = true; // 必要ならフラグ
				continue;
			}

			int type = static_cast<int>(colliders[i].type);
			if(ImGui::Combo(("Type##" + std::to_string(i)).c_str(), &type, "Box\0Sphere\0Capsule\0Mesh\0")){
				colliders[i].type = static_cast<ColliderType>(type);
				needsUpdate = true;
			}

			if(colliders[i].type == ColliderType::Box){
				if(ImGui::DragFloat3(("Size##" + std::to_string(i)).c_str(), &colliders[i].size.x, 0.1f)){
					needsUpdate = true;
				}
			} else if(colliders[i].type == ColliderType::Sphere){
				if(ImGui::DragFloat(("Radius##" + std::to_string(i)).c_str(), &colliders[i].radius, 0.1f)){
					needsUpdate = true;
				}
			} else if(colliders[i].type == ColliderType::Capsule){
				if(ImGui::DragFloat(("Radius##" + std::to_string(i)).c_str(), &colliders[i].radius, 0.1f) ||
				   ImGui::DragFloat(("Height##" + std::to_string(i)).c_str(), &colliders[i].height, 0.1f)){
					needsUpdate = true;
				}
			}

			if(ImGui::DragFloat3(("Offset##" + std::to_string(i)).c_str(), &colliders[i].offset.x, 0.1f)){
				needsUpdate = true;
			}

			// マテリアル
			if(ImGui::DragFloat(("StaticFriction##" + std::to_string(i)).c_str(), &colliders[i].staticFriction, 0.01f, 0.0f, 1.0f) ||
			   ImGui::DragFloat(("DynamicFriction##" + std::to_string(i)).c_str(), &colliders[i].dynamicFriction, 0.01f, 0.0f, 1.0f) ||
			   ImGui::DragFloat(("Restitution##" + std::to_string(i)).c_str(), &colliders[i].restitution, 0.01f, 0.0f, 1.0f)){
				needsUpdate = true;
			}

			// 回転軸固定
			if(ImGui::Checkbox(("Lock Rot X##" + std::to_string(i)).c_str(), &colliders[i].lockRotX) ||
			   ImGui::Checkbox(("Lock Rot Y##" + std::to_string(i)).c_str(), &colliders[i].lockRotY) ||
			   ImGui::Checkbox(("Lock Rot Z##" + std::to_string(i)).c_str(), &colliders[i].lockRotZ)){
				needsUpdate = true;
			}
		}
	}

};
