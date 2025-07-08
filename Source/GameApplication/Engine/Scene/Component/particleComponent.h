#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"

#define MAXPARTICLE 512

struct PARTICLE
{
	Vector3 Position = Vector3(0,0,0);
	Vector3 Speed = Vector3(0, 0, 0);
	float LifeTime = 0.0f;
};

class ParticleComponent : public IComponent {
public:
	YAML::Node encode() override {
		YAML::Node node;
		node["particleLifeTime"] = particleLifeTime;
		node["particleSize"] = particleSize;
		node["particleSpeed"] = particleSpeed;
		node["SpawnRate"] = SpawnRate;
		node["SpawnTimer"] = SpawnTimer;
		node["StartSpeed"] = StartSpeed;
		node["AddSpeed"] = AddSpeed;
		node["StartRandom"] = StartRandom;

		return node;
	}

	bool decode(const YAML::Node& node) override {
		if (!node.IsMap()) { return false; }
		if (node["particleLifeTime"]) {
			particleLifeTime = node["particleLifeTime"].as<float>();
		}
		if (node["particleSize"]) {
			particleSize = node["particleSize"].as<float>();
		}
		if (node["particleSpeed"]) {
			particleSpeed = node["particleSpeed"].as<float>();
		}
		if (node["SpawnRate"]) {
			SpawnRate = node["SpawnRate"].as<float>();
		}
		if (node["SpawnTimer"]) {
			SpawnTimer = node["SpawnTimer"].as<float>();
		}
		if (node["StartSpeed"]) {
			StartSpeed = node["StartSpeed"].as<Vector3>();
		}
		if (node["AddSpeed"]) {
			AddSpeed = node["AddSpeed"].as<Vector3>();
		}
		if (node["StartRandom"]) {
			StartRandom = node["StartRandom"].as<Vector3>();
		}

		return true;
	}

	void inspector(SceneContext* context) override {

		const float labelWidth = 100.0f;
		const int axisCount = 3;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		float totalRegion = ImGui::GetContentRegionAvail().x;
		float fieldRegion = totalRegion - labelWidth - spacing * 2 * (axisCount);
		float fieldWidth = fieldRegion / axisCount;

		ImVec4 colorX = ImVec4(0.7f, 0.4f, 0.4f, 0.3f);
		ImVec4 colorY = ImVec4(0.4f, 0.7f, 0.4f, 0.3f);
		ImVec4 colorZ = ImVec4(0.4f, 0.4f, 0.7f, 0.3f);

		auto DrawVec3Control = [&](const char* label, float& x, float& y, float& z, bool readOnly = false) {
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);
			ImGui::SameLine();
			ImGui::SetCursorPosX(labelWidth); // 確実にラベル後ろに揃える

			auto DrawComponent = [&](const char* id, float& value, const ImVec4& borderColor, const char* uniqueId, bool isLast) {
				ImGui::PushID(uniqueId);
				ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
				ImGui::PushItemWidth(fieldWidth - 10.0f);

				// 軸名（X/Y/Z）
				ImGui::Text("%s", id);
				ImGui::SameLine(0.0f, 10.0f);

				ImGui::DragFloat("##", &value, 0.01f, -1000.0f, 1000.0f);

				ImGui::PopItemWidth();
				ImGui::PopStyleVar();
				ImGui::PopStyleColor();
				ImGui::PopID();

				if (!isLast) ImGui::SameLine();
			};

			DrawComponent("X", x, colorX, (std::string(label) + "X").c_str(), false);
			DrawComponent("Y", y, colorY, (std::string(label) + "Y").c_str(), false);
			DrawComponent("Z", z, colorZ, (std::string(label) + "Z").c_str(), true);
		};

	// ----------- StartSpeed -----------
		DrawVec3Control("StartSpeed", StartSpeed.x, StartSpeed.y, StartSpeed.z);
		DrawVec3Control("StartRandom", StartRandom.x, StartRandom.y, StartRandom.z);
		DrawVec3Control("AddSpeed", AddSpeed.x, AddSpeed.y, AddSpeed.z);

		ImGui::AlignTextToFramePadding();
		ImGui::Text("SpawnRate");
		ImGui::SameLine(labelWidth);
		ImGui::DragFloat("##SpawnRate", &SpawnRate, 0.001f, 0.001f, 128.0f);

		ImGui::AlignTextToFramePadding();
		ImGui::Text("particleLifeTime");
		ImGui::SameLine(labelWidth);
		ImGui::DragFloat("##particleLifeTime", &particleLifeTime, 0.01f, 0.01f, 128.0f);

		ImGui::AlignTextToFramePadding();
		ImGui::Text("particleSize");
		ImGui::SameLine(labelWidth);
		ImGui::DragFloat("##particleSize", &particleSize, 0.01f, 0.01f, 128.0f);
	}

	PARTICLE Particle[MAXPARTICLE];

	float particleLifeTime = 1.0f; // パーティクルのライフタイム
	float particleSize = 1.0f; // パーティクルのサイズ
	float particleSpeed = 1.0f; // パーティクルの速度
	float SpawnRate = 0.05f;

	float SpawnTimer = 0.0f;

	Vector3 StartSpeed = Vector3(0, 10, 0);
	Vector3 AddSpeed = Vector3(0, -15, 0);
	Vector3 StartRandom = Vector3(3, 0, 3);
};
