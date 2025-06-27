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
		node["Component"] = "TransformComponent";
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
        ImVec2 prevSpacing = ImGui::GetStyle().ItemSpacing;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, prevSpacing.y));

        float labelWidth = 100.0f;
        float fieldWidth = 60.0f;

        static bool isUniformLocked = false;

        static DirectX::XMFLOAT3 baseScale = { 1.0f, 1.0f, 1.0f }; // ロックON時に記録する元のスケール

        // ----------- Position -----------
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Position");
        ImGui::SameLine(labelWidth);
        ImGui::Text("X"); ImGui::SameLine(0, 4); ImGui::PushItemWidth(fieldWidth);
        ImGui::DragFloat("##PosX", &position.x, 0.1f); ImGui::PopItemWidth(); ImGui::SameLine();

        ImGui::Text("Y"); ImGui::SameLine(0, 4); ImGui::PushItemWidth(fieldWidth);
        ImGui::DragFloat("##PosY", &position.y, 0.1f); ImGui::PopItemWidth(); ImGui::SameLine();

        ImGui::Text("Z"); ImGui::SameLine(0, 4); ImGui::PushItemWidth(fieldWidth);
        ImGui::DragFloat("##PosZ", &position.z, 0.1f); ImGui::PopItemWidth();

        // ----------- Rotation -----------
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Rotation");
        ImGui::SameLine(labelWidth);
        ImGui::Text("X"); ImGui::SameLine(0, 4); ImGui::PushItemWidth(fieldWidth);
        ImGui::DragFloat("##RotX", &rotation.x, 0.01f, -DirectX::XM_PI, DirectX::XM_PI); ImGui::PopItemWidth(); ImGui::SameLine();

        ImGui::Text("Y"); ImGui::SameLine(0, 4); ImGui::PushItemWidth(fieldWidth);
        ImGui::DragFloat("##RotY", &rotation.y, 0.01f, -DirectX::XM_PI, DirectX::XM_PI); ImGui::PopItemWidth(); ImGui::SameLine();

        ImGui::Text("Z"); ImGui::SameLine(0, 4); ImGui::PushItemWidth(fieldWidth);
        ImGui::DragFloat("##RotZ", &rotation.z, 0.01f, -DirectX::XM_PI, DirectX::XM_PI); ImGui::PopItemWidth();

        // ----------- Scale -----------
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Scale");

        // ----------- Lock Toggle Button -----------
        ImGui::SameLine();

        bool pressed = ImGui::SmallButton(isUniformLocked ? "-" : "+");
        if (pressed) {
            isUniformLocked = !isUniformLocked;
            if (isUniformLocked) {
                baseScale = DirectX::XMFLOAT3{ scale.x,scale.y,scale.z }; // 現在のスケールを基準として記録
            }
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(isUniformLocked ?
                              "Uniform lock ON\nEdit one axis to scale all proportionally" :
                              "Uniform lock OFF\nEdit each axis freely");
        }

        ImGui::SameLine(labelWidth);

        if (!isUniformLocked) {
            // 通常編集（個別）
            ImGui::Text("X"); ImGui::SameLine(0, 4); ImGui::PushItemWidth(fieldWidth);
            ImGui::DragFloat("##ScaleX", &scale.x, 0.01f, 0.01f, 100.0f); ImGui::PopItemWidth(); ImGui::SameLine();

            ImGui::Text("Y"); ImGui::SameLine(0, 4); ImGui::PushItemWidth(fieldWidth);
            ImGui::DragFloat("##ScaleY", &scale.y, 0.01f, 0.01f, 100.0f); ImGui::PopItemWidth(); ImGui::SameLine();

            ImGui::Text("Z"); ImGui::SameLine(0, 4); ImGui::PushItemWidth(fieldWidth);
            ImGui::DragFloat("##ScaleZ", &scale.z, 0.01f, 0.01f, 100.0f); ImGui::PopItemWidth();
        } else {
            // ロックON → どこか1軸を変更したら、全軸に倍率を掛ける

            bool changed = false;
            float ratio = 1.0f;

            // ---- X ----
            float x = scale.x;
            ImGui::Text("X"); ImGui::SameLine(0, 4); ImGui::PushItemWidth(fieldWidth);
            if (ImGui::DragFloat("##ScaleX", &x, 0.01f, 0.01f, 100.0f)) {
                if (baseScale.x != 0.0f) {
                    ratio = x / baseScale.x;
                    changed = true;
                }
            }
            ImGui::PopItemWidth(); ImGui::SameLine();

            // ---- Y ----
            float y = scale.y;
            ImGui::Text("Y"); ImGui::SameLine(0, 4); ImGui::PushItemWidth(fieldWidth);
            if (ImGui::DragFloat("##ScaleY", &y, 0.01f, 0.01f, 100.0f)) {
                if (baseScale.y != 0.0f) {
                    ratio = y / baseScale.y;
                    changed = true;
                }
            }
            ImGui::PopItemWidth(); ImGui::SameLine();

            // ---- Z ----
            float z = scale.z;
            ImGui::Text("Z"); ImGui::SameLine(0, 4); ImGui::PushItemWidth(fieldWidth);
            if (ImGui::DragFloat("##ScaleZ", &z, 0.01f, 0.01f, 100.0f)) {
                if (baseScale.z != 0.0f) {
                    ratio = z / baseScale.z;
                    changed = true;
                }
            }
            ImGui::PopItemWidth();

            if (changed) {
                scale.x = baseScale.x * ratio;
                scale.y = baseScale.y * ratio;
                scale.z = baseScale.z * ratio;
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Locked: scale all axes proportionally based on changed axis");
        }

        ImGui::PopStyleVar();
    }


};
