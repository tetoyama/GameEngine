#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"
#include "Service/YAMLConverters.h"
#include "GameApplication/Engine/DebugTools/ImGuiSystem.h"


class TransformComponent : public IComponent{
public:
	Vector3 position = Vector3(0, 0, 0);
	Vector3 rotation = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);

	YAML::Node encode() override{
		YAML::Node node;
		node["Position"] = position;
		node["Rotation"] = rotation;
		node["Scale"] = scale;

		return node;
	}

	bool decode(const YAML::Node& node) override {
		if (!node.IsMap()) { return false; }

		if (node["Position"]) {
			position = node["Position"].as<Vector3>();
		}
		if (node["Rotation"]) {
			rotation = node["Rotation"].as<Vector3>();
		}
		if (node["Scale"]) {
			scale = node["Scale"].as<Vector3>();
		}
		return true;
	}
	Vector3 front() const {
		float pitch = rotation.x;
		float yaw = rotation.y;
		float roll = rotation.z;

		float cosPitch = cos(pitch);
		float sinPitch = sin(pitch);
		float cosYaw = cos(yaw);
		float sinYaw = sin(yaw);
		float cosRoll = cos(roll);
		float sinRoll = sin(roll);

		return Vector3(
			cosPitch * sinYaw,
			sinPitch,
			cosPitch * cosYaw
		);
	}

	Vector3 up() const {
		float pitch = rotation.x;
		float yaw = rotation.y;
		float roll = rotation.z;

		float cosPitch = cos(pitch);
		float sinPitch = sin(pitch);
		float cosYaw = cos(yaw);
		float sinYaw = sin(yaw);
		float cosRoll = cos(roll);
		float sinRoll = sin(roll);

		return Vector3(
			-cosPitch * sinYaw * sinRoll + sinPitch * cosRoll,
			cosPitch * cosRoll + sinPitch * sinYaw * sinRoll,
			cosYaw * sinPitch
		);
	}

	Vector3 right() const {
		float pitch = rotation.x;
		float yaw = rotation.y;
		float roll = rotation.z;

		float cosPitch = cos(pitch);
		float sinPitch = sin(pitch);
		float cosYaw = cos(yaw);
		float sinYaw = sin(yaw);
		float cosRoll = cos(roll);
		float sinRoll = sin(roll);

		return Vector3(
			cosPitch * cosYaw,
			-sinPitch,
			cosPitch * sinYaw
		);
	}

    void inspector(SceneContext* context) override {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

		const float labelWidth = 100.0f;
		const int axisCount = 3;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		float totalRegion = ImGui::GetContentRegionAvail().x;
		float fieldRegion = totalRegion - labelWidth - spacing * 2 * (axisCount);
		float fieldWidth = fieldRegion / axisCount;

		static bool isUniformLocked = false;
		static DirectX::XMFLOAT3 baseScale = {1.0f, 1.0f, 1.0f};

		ImVec4 colorX = ImVec4(0.7f, 0.4f, 0.4f, 0.3f);
		ImVec4 colorY = ImVec4(0.4f, 0.7f, 0.4f, 0.3f);
		ImVec4 colorZ = ImVec4(0.4f, 0.4f, 0.7f, 0.3f);

		auto DrawVec3Control = [&](const char* label, float& x, float& y, float& z, bool readOnly = false){
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);
			ImGui::SameLine();
			ImGui::SetCursorPosX(labelWidth); // 確実にラベル後ろに揃える

			auto DrawComponent = [&](const char* id, float& value, const ImVec4& borderColor, const char* uniqueId, bool isLast){
				ImGui::PushID(uniqueId);
				ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
				ImGui::PushItemWidth(fieldWidth - 10.0f);

				// 軸名（X/Y/Z）
				ImGui::Text("%s", id);
				ImGui::SameLine(0.0f,10.0f);

				ImGui::DragFloat("##", &value, 0.01f, -1000.0f, 1000.0f);

				ImGui::PopItemWidth();
				ImGui::PopStyleVar();
				ImGui::PopStyleColor();
				ImGui::PopID();

				if(!isLast) ImGui::SameLine();
				};

			DrawComponent("X", x, colorX, (std::string(label) + "X").c_str(), false);
			DrawComponent("Y", y, colorY, (std::string(label) + "Y").c_str(), false);
			DrawComponent("Z", z, colorZ, (std::string(label) + "Z").c_str(), true);
			};

        // ----------- Position -----------
        DrawVec3Control("Position", position.x, position.y, position.z);

        // ----------- Rotation -----------
        DrawVec3Control("Rotation", rotation.x, rotation.y, rotation.z);

		// ----------- Scale -----------
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Scale");
		ImGui::SameLine();

		// ロックボタン（+ がON、- がOFF）
		bool pressed = ImGui::SmallButton(isUniformLocked ? "-" : "+");
		if(pressed){
			isUniformLocked = !isUniformLocked;
			if(isUniformLocked)
				baseScale = {scale.x, scale.y, scale.z};
		}
		if(ImGui::IsItemHovered())
			ImGui::SetTooltip(isUniformLocked ? "Uniform lock ON" : "Uniform lock OFF");

		ImGui::SameLine(labelWidth);

		if(!isUniformLocked){
			DrawVec3Control("", scale.x, scale.y, scale.z);
		} else{
			float ratio = 1.0f;
			bool changed = false;

			auto DrawLockedComponent = [&](const char* id, float& value, float baseValue, float& outRatio, const ImVec4& borderColor, bool isLast){
				float temp = value;
				ImGui::PushID(id);
				ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
				ImGui::PushItemWidth(fieldWidth - 10.0f); // <- はみ出し防止：DrawVec3Control と統一
				ImGui::Text("%s", id);
				ImGui::SameLine(0.0f, 10.0f);
				if(ImGui::DragFloat("##", &temp, 0.01f, 0.01f, 100.0f)){
					if(baseValue != 0.0f){
						outRatio = temp / baseValue;
						changed = true;
					}
				}
				ImGui::PopItemWidth();
				ImGui::PopStyleVar();
				ImGui::PopStyleColor();
				ImGui::PopID();
				if(!isLast) ImGui::SameLine();
				};

			// X/Y/Zを並べて描画（同じ幅とスタイルで）
			DrawLockedComponent("X", scale.x, baseScale.x, ratio, colorX, false);
			DrawLockedComponent("Y", scale.y, baseScale.y, ratio, colorY, false);
			DrawLockedComponent("Z", scale.z, baseScale.z, ratio, colorZ, true);

			if(changed){
				scale.x = baseScale.x * ratio;
				scale.y = baseScale.y * ratio;
				scale.z = baseScale.z * ratio;
			}

			if(ImGui::IsItemHovered())
				ImGui::SetTooltip("Locked: scale all axes proportionally");
		}


        ImGui::PopStyleVar(); // ItemSpacing
    }



};
