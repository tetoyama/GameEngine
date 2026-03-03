#pragma once
#include "Interface/IComponent.h"
#include "BackEnds/YAMLConverters.h"
#include "BackEnds/ImGuiFunc.h"

#define MAXPARTICLE 512

struct PARTICLE
{
	Vector3 Position = Vector3(0,0,0);
	Vector3 Speed = Vector3(0, 0, 0);
	float LifeTime = 0.0f;
};

class ParticleComponent : public IComponent {
public:
	PARTICLE Particle[MAXPARTICLE];

	bool isLoop = true;

	float SpawnInterval = 0.05f;

	int SpawnCount = 100;

	float particleLifeTime = 1.0f; // パーティクルのライフタイム
	float particleSize = 1.0f; // パーティクルのサイズ
	float particleSpeed = 1.0f; // パーティクルの速度
	
	float SpawnTimer = 0.0f;

	Vector3 SpawnPosition = Vector3(0, 0, 0);
	Vector3 SpawnPositionRandom = Vector3(0, 0, 0);

	Vector3 StartSpeed = Vector3(0, 10, 0);
	Vector3 StartSpeedRandom = Vector3(3, 0, 3);

	Vector3 AddSpeed = Vector3(0, -15, 0);
	Vector3 MulSpeed = Vector3(1, 1, 1);

	YAML::Node encode() override{
		YAML::Node node;
		node["IsLoop"] = isLoop;
		node["SpawnInterval"] = SpawnInterval;
		node["SpawnCount"] = SpawnCount;
		node["SpawnTimer"] = SpawnTimer;

		node["ParticleLifeTime"] = particleLifeTime;
		node["ParticleSize"] = particleSize;
		node["ParticleSpeed"] = particleSpeed;

		node["SpawnPosition"] = SpawnPosition;
		node["SpawnPositionRandom"] = SpawnPositionRandom;

		node["StartSpeed"] = StartSpeed;
		node["StartSpeedRandom"] = StartSpeedRandom;

		node["AddSpeed"] = AddSpeed;
		node["MulSpeed"] = MulSpeed;

		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(!node.IsMap()) return false;

		if(node["IsLoop"]) isLoop = node["IsLoop"].as<bool>();
		if(node["SpawnInterval"]) SpawnInterval = node["SpawnInterval"].as<float>();
		if(node["SpawnCount"]) SpawnCount = node["SpawnCount"].as<int>();
		if(node["SpawnTimer"]) SpawnTimer = node["SpawnTimer"].as<float>();

		if(node["ParticleLifeTime"]) particleLifeTime = node["ParticleLifeTime"].as<float>();
		if(node["ParticleSize"]) particleSize = node["ParticleSize"].as<float>();
		if(node["ParticleSpeed"]) particleSpeed = node["ParticleSpeed"].as<float>();

		if(node["SpawnPosition"]) SpawnPosition = node["SpawnPosition"].as<Vector3>();
		if(node["SpawnPositionRandom"]) SpawnPositionRandom = node["SpawnPositionRandom"].as<Vector3>();

		if(node["StartSpeed"]) StartSpeed = node["StartSpeed"].as<Vector3>();
		if(node["StartSpeedRandom"]) StartSpeedRandom = node["StartSpeedRandom"].as<Vector3>();

		if(node["AddSpeed"]) AddSpeed = node["AddSpeed"].as<Vector3>();
		if(node["MulSpeed"]) MulSpeed = node["MulSpeed"].as<Vector3>();

		return true;
	}

	void inspector(SceneContext* context) override {
		if(ImGui::TreeNodeEx("Particle", ImGuiTreeNodeFlags_DefaultOpen)){

			float labelWidth = 120.0f;
			float dragWidth = ImGui::GetContentRegionAvail().x - labelWidth - ImGui::GetStyle().ItemSpacing.x * 2;

			ImGui::Text("isLoop?");
			ImGui::SameLine(labelWidth);
			ImGui::UndoCheckbox("##isLoop", &isLoop);

			if(isLoop){
				ImGui::AlignTextToFramePadding();
				ImGui::Text("SpawnInterval");
				ImGui::SameLine(labelWidth);
				ImGui::PushItemWidth(dragWidth);
				ImGui::UndoDragFloat("##SpawnInterval", &SpawnInterval, 0.001f, 0.001f, 128.0f);
			} else{
				ImGui::AlignTextToFramePadding();
				ImGui::Text("SpawnCount");
				ImGui::SameLine(labelWidth);
				ImGui::PushItemWidth(dragWidth);
				ImGui::UndoDragInt("##SpawnCount", &SpawnCount, 0.1f, 1, MAXPARTICLE);
			}
			ImGui::AlignTextToFramePadding();
			ImGui::Text("particleLifeTime");
			ImGui::SameLine(labelWidth);
			ImGui::PushItemWidth(dragWidth);
			ImGui::UndoDragFloat("##particleLifeTime", &particleLifeTime, 0.01f, 0.01f, 128.0f);

			ImGui::AlignTextToFramePadding();
			ImGui::Text("particleSize");
			ImGui::SameLine(labelWidth);
			ImGui::PushItemWidth(dragWidth);
			ImGui::UndoDragFloat("##particleSize", &particleSize, 0.01f, 0.01f, 128.0f);
			ImGui::TreePop();
		}
		if(ImGui::TreeNodeEx("Spawn Position", ImGuiTreeNodeFlags_DefaultOpen)){

			ImGui::UndoDragVec3("BasePositon", SpawnPosition);
			ImGui::UndoDragVec3("RandomPos", SpawnPositionRandom);

			ImGui::TreePop();
		}
		if(ImGui::TreeNodeEx("Initial Velocity", ImGuiTreeNodeFlags_DefaultOpen)){

			ImGui::UndoDragVec3("BaseSpeed", StartSpeed);
			ImGui::UndoDragVec3("RandomSpd", StartSpeedRandom);

			ImGui::TreePop();
		}
		if(ImGui::TreeNodeEx("Motion", ImGuiTreeNodeFlags_DefaultOpen)){

			ImGui::UndoDragVec3("Acceleration", AddSpeed);
			ImGui::UndoDragVec3("Velocity Multiplier", MulSpeed);

			ImGui::TreePop();
		}

	}
};
