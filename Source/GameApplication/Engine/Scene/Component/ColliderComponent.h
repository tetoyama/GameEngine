#pragma once
#include "Interface/IComponent.h"
#include "BackEnds/YAMLConverters.h"
#include "BackEnds/ImGuiFunc.h"
#include "Backends/myVector2.h"
#include "registry/SystemRegistry.h"
#include "BackEnds/PhysX/PxPhysicsAPI.h"

enum class ColliderType {
	Box,
	Sphere,
	Capsule,
	Mesh
};

struct ColliderShape {
	ColliderType type = ColliderType::Box;

	Vector3 size = {1.0f, 1.0f, 1.0f};
	Vector3 offset = {0.0f, 0.0f, 0.0f};

	Vector3 rotationOffset = {0.0f, 0.0f, 0.0f};

	float radius = 0.5f;
	float height = 1.0f;

	float staticFriction = 0.5f;
	float dynamicFriction = 0.5f;
	float restitution = 0.1f;

	uint32_t collisionLayer = 0;

	bool lockRotX = false;
	bool lockRotY = false;
	bool lockRotZ = false;

	physx::PxShape* pxShape = nullptr;
	physx::PxMaterial* pxMaterial = nullptr;
};

class ColliderComponent: public IComponent {
public:
	~ColliderComponent(){
		for(auto& col : colliders){
			//if (col.pxShape) {
			//	col.pxShape->release();
			//	col.pxShape = nullptr;
			//}
			//if(col.pxMaterial){
			//	col.pxMaterial->release();
			//	col.pxMaterial = nullptr;
			//}
			if (pRigidbodyStatic) {
				OutputDebugStringA(("Finalize Release Static Actor: " + std::to_string((uintptr_t)pRigidbodyStatic) + "\n").c_str());
				pRigidbodyStatic->release();
				pRigidbodyStatic = nullptr;
			}
			if (pRigidbodyDynamic) {
				OutputDebugStringA(("Finalize Release Dynamic Actor: " + std::to_string((uintptr_t)pRigidbodyDynamic) + "\n").c_str());
				pRigidbodyDynamic->release();
				pRigidbodyDynamic = nullptr;
			}
		}
	}

	physx::PxRigidDynamic* pRigidbodyDynamic = nullptr;
	physx::PxRigidStatic* pRigidbodyStatic = nullptr;
	bool needsUpdate = true;

	BEGIN_REFLECT(ColliderComponent)
		REFLECT_FIELD(bool, isDynamic, false)
		REFLECT_FIELD(bool, autoMass, true)
		REFLECT_FIELD(float, Mass, false)

	std::vector<ColliderShape> colliders;

	// =====================================================
	// YAML Encode
	// =====================================================
	YAML::Node encode() override{
		YAML::Node node;
		ENCODE_FIELDS(node);
		for(auto& col : colliders){
			YAML::Node colNode;
			colNode["type"] = static_cast<int>(col.type);

			colNode["size"] = YAML::Load("[]");
			colNode["size"].push_back(col.size.x);
			colNode["size"].push_back(col.size.y);
			colNode["size"].push_back(col.size.z);

			colNode["offset"] = YAML::Load("[]");
			colNode["offset"].push_back(col.offset.x);
			colNode["offset"].push_back(col.offset.y);
			colNode["offset"].push_back(col.offset.z);

			colNode["rotationOffset"] = YAML::Load("[]");
			colNode["rotationOffset"].push_back(col.rotationOffset.x);
			colNode["rotationOffset"].push_back(col.rotationOffset.y);
			colNode["rotationOffset"].push_back(col.rotationOffset.z);

			colNode["radius"] = col.radius;
			colNode["height"] = col.height;

			colNode["staticFriction"] = col.staticFriction;
			colNode["dynamicFriction"] = col.dynamicFriction;
			colNode["restitution"] = col.restitution;

			colNode["collisionLayer"] = col.collisionLayer;
			colNode["lockRotX"] = col.lockRotX;
			colNode["lockRotY"] = col.lockRotY;
			colNode["lockRotZ"] = col.lockRotZ;

			node["colliders"].push_back(colNode);
		}
		return node;
	}

	// =====================================================
	// YAML Decode
	// =====================================================
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

				// ★ rotationOffset読み込み
				if(colNode["rotationOffset"]){
					auto rot = colNode["rotationOffset"];
					col.rotationOffset = Vector3(rot[0].as<float>(), rot[1].as<float>(), rot[2].as<float>());
				}

				col.radius = colNode["radius"].as<float>();
				col.height = colNode["height"].as<float>();
				col.staticFriction = colNode["staticFriction"].as<float>();
				col.dynamicFriction = colNode["dynamicFriction"].as<float>();
				col.restitution = colNode["restitution"].as<float>();
				col.collisionLayer = colNode["collisionLayer"].as<uint32_t>();
				col.lockRotX = colNode["lockRotX"].as<bool>();
				col.lockRotY = colNode["lockRotY"].as<bool>();
				col.lockRotZ = colNode["lockRotZ"].as<bool>();

				colliders.push_back(col);
			}
		}
		return true;
	}

	// =====================================================
	// ImGui Inspector
	// =====================================================
	void inspector(SceneContext* context) override{
		ImGui::Text("Collider Component");
		INSPECTOR_FIELDS();

		if(ImGui::Button("Add Collider")){
			colliders.push_back(ColliderShape());
			needsUpdate = true;
		}

		for(size_t i = 0; i < colliders.size(); ++i){
			ImGui::Separator();

			std::string TreeText = "Collider" + std::to_string((int)i);


			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen;

			// ツリーノードの展開矢印だけ表示（幅だけ取るためにNoTreePushOnOpen）
			bool open = ImGui::TreeNodeEx(TreeText.c_str(), flags, TreeText.c_str());

			// Removeボタンは同じ行の右端に配置
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 60);
			if (ImGui::SmallButton(("Remove##" + TreeText).c_str())) {
				if (colliders[i].pxShape) {
					colliders[i].pxShape->release();
					colliders[i].pxShape = nullptr;
				}
				if (colliders[i].pxMaterial) {
					colliders[i].pxMaterial->release();
					colliders[i].pxMaterial = nullptr;
				}
				colliders.erase(colliders.begin() + i);
				needsUpdate = true;
				continue;

			}else if (!open) {
				continue;
			}



			int type = static_cast<int>(colliders[i].type);
			if(ImGui::Combo(("Type##" + std::to_string(i)).c_str(), &type, "Box\0Sphere\0Capsule\0Mesh\0")){
				colliders[i].type = static_cast<ColliderType>(type);
				needsUpdate = true;
			}

			if(colliders[i].type == ColliderType::Box){
				if(ImGui::DragFloat3(("Size##" + std::to_string(i)).c_str(), &colliders[i].size.x, 0.1f))
					needsUpdate = true;
			} else if(colliders[i].type == ColliderType::Sphere){
				if(ImGui::DragFloat(("Radius##" + std::to_string(i)).c_str(), &colliders[i].radius, 0.1f))
					needsUpdate = true;
			} else if(colliders[i].type == ColliderType::Capsule){
				if(ImGui::DragFloat(("Radius##" + std::to_string(i)).c_str(), &colliders[i].radius, 0.1f) ||
				   ImGui::DragFloat(("Height##" + std::to_string(i)).c_str(), &colliders[i].height, 0.1f))
					needsUpdate = true;
			}

			if(ImGui::DragFloat3(("Offset##" + std::to_string(i)).c_str(), &colliders[i].offset.x, 0.1f))
				needsUpdate = true;

			if(ImGui::DragFloat3(("Rotation Offset (deg)##" + std::to_string(i)).c_str(), &colliders[i].rotationOffset.x, 1.0f))
				needsUpdate = true;

			if(ImGui::DragFloat(("StaticFriction##" + std::to_string(i)).c_str(), &colliders[i].staticFriction, 0.01f, 0.0f, 1.0f) ||
			   ImGui::DragFloat(("DynamicFriction##" + std::to_string(i)).c_str(), &colliders[i].dynamicFriction, 0.01f, 0.0f, 1.0f) ||
			   ImGui::DragFloat(("Restitution##" + std::to_string(i)).c_str(), &colliders[i].restitution, 0.01f, 0.0f, 1.0f))
				needsUpdate = true;

			if(ImGui::Checkbox(("Lock Rot X##" + std::to_string(i)).c_str(), &colliders[i].lockRotX) ||
			   ImGui::Checkbox(("Lock Rot Y##" + std::to_string(i)).c_str(), &colliders[i].lockRotY) ||
			   ImGui::Checkbox(("Lock Rot Z##" + std::to_string(i)).c_str(), &colliders[i].lockRotZ))
				needsUpdate = true;
		}
	}
};
