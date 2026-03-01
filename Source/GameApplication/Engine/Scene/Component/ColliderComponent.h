#pragma once
#include "Interface/IComponent.h"
#include "BackEnds/YAMLConverters.h"
#include "BackEnds/ImGuiFunc.h"
#include "Backends/myVector2.h"
#include "registry/SystemRegistry.h"
#include "registry/componentRegistry.h"
#include "System/Physic/physicSystem.h"
#include "BackEnds/PhysX/PxPhysicsAPI.h"
#include "Component/modelRendererComponent.h"

enum class ColliderType {
	Box,
	Sphere,
	Capsule,
	Mesh,
	HeightMap
};

struct ColliderShape {
	ColliderType type = ColliderType::Box;

	Vector3 size = {1.0f, 1.0f, 1.0f};
	Vector3 offset = {0.0f, 0.0f, 0.0f};

	Vector3 rotationOffset = {0.0f, 0.0f, 0.0f};

	float radius = 0.5f;
	float height = 1.0f;

	float staticFriction = 0.0f;
	float dynamicFriction = 0.0f;
	float restitution = 0.0f;

	uint32_t collisionLayer = 0;

	bool lockRotX = false;
	bool lockRotY = false;
	bool lockRotZ = false;

	bool isTrigger = false;

	std::string boneName = "";  // スキニングアニメーション時にオフセットの基準となるボーン名

	physx::PxShape* pxShape = nullptr;
	physx::PxMaterial* pxMaterial = nullptr;
	physx::PxHeightField* pxHeightField = nullptr;
	physx::PxTriangleMesh* pxTriangleMesh = nullptr;
	physx::PxConvexMesh* pxConvexMesh = nullptr;
};

class ColliderComponent: public IComponent {
public:
	~ColliderComponent(){
		if (pRigidbodyStatic) {
			if (pRigidbodyStatic->userData)
				delete static_cast<ActorEntityInfo*>(pRigidbodyStatic->userData);
			pRigidbodyStatic->release();
			pRigidbodyStatic = nullptr;
		}
		if (pRigidbodyDynamic) {
			if (pRigidbodyDynamic->userData)
				delete static_cast<ActorEntityInfo*>(pRigidbodyDynamic->userData);
			pRigidbodyDynamic->release();
			pRigidbodyDynamic = nullptr;
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

			colNode["isTrigger"] = col.isTrigger;

			colNode["boneName"] = col.boneName;

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

				if (colNode["isTrigger"]) {
					col.isTrigger = colNode["isTrigger"].as<bool>();
				}
				if (colNode["boneName"]) {
					col.boneName = colNode["boneName"].as<std::string>();
				}
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


		ImGui::Separator();
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;

		bool isOpenColliders = ImGui::TreeNodeEx(("Colliders(" + std::to_string(colliders.size()) + ")").c_str(), flags);
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 85);
		if(ImGui::Button("Add Collider")){
			colliders.push_back(ColliderShape());
			needsUpdate = true;
		}
		if(isOpenColliders){
			ImGui::BeginChild("Colliders Child", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);

			for(size_t i = 0; i < colliders.size(); i++ , ImGui::Separator()){

				std::string TreeText = "Collider" + std::to_string((int)i);

				// ツリーノードの展開矢印だけ表示（幅だけ取るためにNoTreePushOnOpen）
				bool isOpenCollider = ImGui::TreeNodeEx(TreeText.c_str(), flags, TreeText.c_str());


				// Removeボタンは同じ行の右端に配置
				ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 60);
				if(ImGui::SmallButton(("Remove##" + TreeText).c_str())){
					if(colliders[i].pxShape){
						colliders[i].pxShape->release();
						colliders[i].pxShape = nullptr;
					}
					if(colliders[i].pxMaterial){
						colliders[i].pxMaterial->release();
						colliders[i].pxMaterial = nullptr;
					}
					colliders.erase(colliders.begin() + i);
					needsUpdate = true;
					if(isOpenCollider){
						ImGui::TreePop();
					}
					continue;

				} else if(!isOpenCollider){
					continue;
				} else{

					int type = static_cast<int>(colliders[i].type);
					float XPos = 120.0f;
					ImGui::BeginChild("ColliderChild", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);

					if(ImGui::TreeNodeEx("Param", flags)){
						ImGui::BeginChild("ParamChild", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);

						ImGui::Text("Type");
						ImGui::SameLine(XPos);
						if(ImGui::Combo(("##Type" + std::to_string(i)).c_str(), &type, "Box\0Sphere\0Capsule\0Mesh\0HeightMap\0")){
							colliders[i].type = static_cast<ColliderType>(type);
							needsUpdate = true;
						}
						ImGui::Text("Is Trigger");
						ImGui::SameLine(XPos);
						if (ImGui::Checkbox(("##IsTrigger" + std::to_string(i)).c_str(), &colliders[i].isTrigger)) {
							needsUpdate = true;
						}

						auto phys = context->system->GetSystem<PhysicSystem>();
						const auto& layers = phys->GetLayers();

						int current = 0;
						for (int j = 0; j < layers.size(); ++j) {
							if (colliders[i].collisionLayer == layers[j].bit) {
								current = j;
								break;
							}
						}
						ImGui::Text("Collision Layer");
						ImGui::SameLine(XPos);

						if (ImGui::BeginCombo("##Collision Layer", layers[current].name.c_str())) {
							for (int j = 0; j < layers.size(); ++j) {
								bool selected = (i == current);
								if (ImGui::Selectable(layers[j].name.c_str(), selected)) {
									colliders[i].collisionLayer = layers[j].bit;
									needsUpdate = true;
								}
							}
							ImGui::EndCombo();
						}


						if(colliders[i].type == ColliderType::Box){
							ImGui::Text("Size");
							ImGui::SameLine(XPos);
							if(ImGui::DragFloat3(("##Size" + std::to_string(i)).c_str(), &colliders[i].size.x, 0.1f))
								needsUpdate = true;
						} else if(colliders[i].type == ColliderType::Sphere){
							ImGui::Text("Radius");
							ImGui::SameLine(XPos);
							if(ImGui::DragFloat(("##Radius" + std::to_string(i)).c_str(), &colliders[i].radius, 0.1f))
								needsUpdate = true;
						} else if(colliders[i].type == ColliderType::Capsule){
							ImGui::Text("Radius");
							ImGui::SameLine(XPos);
							if(ImGui::DragFloat(("##Radius" + std::to_string(i)).c_str(), &colliders[i].radius, 0.1f)){
								needsUpdate = true;
							}
							ImGui::Text("Height");
							ImGui::SameLine(XPos);
							if(ImGui::DragFloat(("##Height" + std::to_string(i)).c_str(), &colliders[i].height, 0.1f)){
								needsUpdate = true;
							}
						}
						ImGui::EndChild();
						ImGui::TreePop();
					}
					if(ImGui::TreeNodeEx("Offset")){
						ImGui::BeginChild("OffsetChild", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);

						// ボーン名コンボボックス：同エンティティの ModelRendererComponent からボーン一覧を取得
						ImGui::Text("Bone Name");
						ImGui::SameLine(XPos);
						{
							// このコンポーネントを持つエンティティを特定
							ModelRendererComponent* mr = nullptr;
							const auto& colEntities = context->component->FindEntitiesWithComponent<ColliderComponent>();
							for(Entity ce : colEntities){
								if(context->component->GetComponent<ColliderComponent>(ce) == this){
									mr = context->component->GetComponent<ModelRendererComponent>(ce);
									break;
								}
							}

							const std::string& currentBone = colliders[i].boneName;
							const char* previewLabel = currentBone.empty() ? "(none)" : currentBone.c_str();

							if(mr && mr->model && !mr->model->m_BoneIndexMap.empty()){
								if(ImGui::BeginCombo(("##BoneName" + std::to_string(i)).c_str(), previewLabel)){
									// ボーン名をソート済みリストとして収集（コンボが開いている間のみ）
									std::vector<std::string> boneNames;
									boneNames.emplace_back(""); // 空 = なし
									for(const auto& [name, idx] : mr->model->m_BoneIndexMap){
										boneNames.push_back(name);
									}
									std::sort(boneNames.begin() + 1, boneNames.end());

									for(const auto& bname : boneNames){
										const char* displayName = bname.empty() ? "(none)" : bname.c_str();
										bool selected = (currentBone == bname);
										if(ImGui::Selectable(displayName, selected)){
											colliders[i].boneName = bname;
										}
										if(selected) ImGui::SetItemDefaultFocus();
									}
									ImGui::EndCombo();
								}
							} else {
								// モデルがない場合はテキスト入力にフォールバック
								char boneNameBuf[128] = "";
								strncpy_s(boneNameBuf, sizeof(boneNameBuf), currentBone.c_str(), _TRUNCATE);
								if(ImGui::InputText(("##BoneName" + std::to_string(i)).c_str(), boneNameBuf, sizeof(boneNameBuf))){
									colliders[i].boneName = boneNameBuf;
								}
							}
						}

						ImGui::Text("Position");
						ImGui::SameLine(XPos);
						if(ImGui::DragFloat3(("##Position" + std::to_string(i)).c_str(), &colliders[i].offset.x, 0.1f)){
							needsUpdate = true;
						}
						ImGui::Text("Rotation");
						ImGui::SameLine(XPos);
						if(ImGui::DragFloat3(("##Rotation" + std::to_string(i)).c_str(), &colliders[i].rotationOffset.x, 1.0f)){
							needsUpdate = true;
						}
						ImGui::Text("Lock Rotation");
						ImGui::SameLine(XPos);
						if(ImGui::Checkbox(("X##Lock Rot " + std::to_string(i)).c_str(), &colliders[i].lockRotX)){
							needsUpdate = true;
						}
						ImGui::SameLine();
						if(ImGui::Checkbox(("Y##Lock Rot " + std::to_string(i)).c_str(), &colliders[i].lockRotY)){
							needsUpdate = true;
						}
						ImGui::SameLine();
						if(ImGui::Checkbox(("Z##Lock Rot" + std::to_string(i)).c_str(), &colliders[i].lockRotZ)){
							needsUpdate = true;
						}
						ImGui::EndChild();
						ImGui::TreePop();
					}
					if(ImGui::TreeNodeEx("Friction")){
						ImGui::BeginChild("FrictionChild", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);

						ImGui::Text("Static");
						ImGui::SameLine(XPos);
						if(ImGui::DragFloat(("##Static" + std::to_string(i)).c_str(), &colliders[i].staticFriction, 0.01f, 0.0f, 1.0f)){
							needsUpdate = true;
						}
						ImGui::Text("Dynamic");
						ImGui::SameLine(XPos);
						if(ImGui::DragFloat(("##Dynamic" + std::to_string(i)).c_str(), &colliders[i].dynamicFriction, 0.01f, 0.0f, 1.0f)){
							needsUpdate = true;
						}
						ImGui::Text("Restitution");
						ImGui::SameLine(XPos);
						if(ImGui::DragFloat(("##Restitution" + std::to_string(i)).c_str(), &colliders[i].restitution, 0.01f, 0.0f, 1.0f)){
							needsUpdate = true;
						}
						ImGui::EndChild();
						ImGui::TreePop();
					}


					ImGui::EndChild();
					ImGui::TreePop();
				}
			}
			ImGui::EndChild();
			ImGui::TreePop();
		}
	}
};
