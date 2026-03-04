// =======================================================================
// 
// followComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include "Backends/YAMLConverters.h"
#include "Backends/myVector3.h"
#include "Entity/Entity.h"
#include "Scene/scene.h"
#include "Registry/componentRegistry.h"

#include "modelRendererComponent.h"
#include "entityNameComponent.h"
#include "transformComponent.h"

#include <string>
#include <vector>
#include <DirectXMath.h>

// 指定したエンティティ（またはボーン）に追従するコンポーネント
class FollowComponent : public IComponent {
public:
	Entity targetEntity = 0;		// 追従対象のエンティティID
	std::string boneName = "";		// 追従するボーン名（スキニングアニメーション時に使用）
	Vector3 positionOffset;			// 追従位置へのオフセット
	bool followPosition = true;		// 位置を追従するか
	bool followRotation = false;	// 回転を追従するか

	YAML::Node encode() override {
		YAML::Node node;
		node["TargetEntity"] = (int)targetEntity;
		node["BoneName"] = boneName;
		node["PositionOffset"] = positionOffset;
		node["FollowPosition"] = followPosition;
		node["FollowRotation"] = followRotation;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		if (node["TargetEntity"]) targetEntity = Entity(node["TargetEntity"].as<int>());
		if (node["BoneName"]) boneName = node["BoneName"].as<std::string>();
		if (node["PositionOffset"]) positionOffset = node["PositionOffset"].as<Vector3>();
		if (node["FollowPosition"]) followPosition = node["FollowPosition"].as<bool>();
		if (node["FollowRotation"]) followRotation = node["FollowRotation"].as<bool>();
		return true;
	}

	void inspector(SceneContext* context) override {
		// --- 追従対象エンティティ ---
		ImGui::Text("Target Entity");
		ImGui::SameLine(120.0f);
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		int entityID = (int)targetEntity;
		if (ImGui::InputInt("##TargetEntity", &entityID)) {
			targetEntity = Entity(entityID);
		}

		// 対象エンティティの名前表示
		if (targetEntity != 0 && context) {
			if (context->entity->IsAlive(targetEntity)) {
				auto* nameComp = context->component->GetComponent<NameComponent>(targetEntity);
				if (nameComp) {
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[%s]", nameComp->name.c_str());
				}
			} else {
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[Invalid]");
			}
		}

		// --- ボーン名 ---
		// 対象にスキニングモデルがある場合はコンボボックス、なければテキスト入力
		bool hasBones = false;
		if (targetEntity != 0 && context && context->entity->IsAlive(targetEntity)) {
			auto* mr = context->component->GetComponent<ModelRendererComponent>(targetEntity);
			if (mr && mr->model && !mr->model->m_BoneIndexMap.empty()) {
				hasBones = true;

				ImGui::Text("Bone Name");
				ImGui::SameLine(120.0f);

				// ボーン名一覧を収集
				std::vector<std::string> boneNames;
				boneNames.push_back(""); // (None)
				for (const auto& [name, idx] : mr->model->m_BoneIndexMap) {
					boneNames.push_back(name);
				}

				// 現在選択中のインデックスを検索
				int currentIdx = 0;
				for (int i = 0; i < (int)boneNames.size(); i++) {
					if (boneNames[i] == boneName) { currentIdx = i; break; }
				}

				const char* previewLabel = boneNames[currentIdx].empty()
					? "(None)" : boneNames[currentIdx].c_str();
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
				if (ImGui::BeginCombo("##BoneName", previewLabel)) {
					for (int i = 0; i < (int)boneNames.size(); i++) {
						bool sel = (i == currentIdx);
						const char* label = boneNames[i].empty() ? "(None)" : boneNames[i].c_str();
						if (ImGui::Selectable(label, sel)) boneName = boneNames[i];
						if (sel) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}
		}
		if (!hasBones) {
			// スキニングモデルがない場合は手動入力
			ImGui::Text("Bone Name");
			ImGui::SameLine(120.0f);
			char buf[256] = "";
			strncpy_s(buf, sizeof(buf), boneName.c_str(), _TRUNCATE);
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			if (ImGui::InputText("##BoneName", buf, sizeof(buf))) boneName = std::string(buf);
		}

		// --- 位置オフセット ---
		ImGui::UndoDragVec3("Offset", positionOffset);

		// --- 追従フラグ ---
		ImGui::UndoCheckbox("Follow Position", &followPosition);
		ImGui::SameLine();
		ImGui::UndoCheckbox("Follow Rotation", &followRotation);
	}
};
