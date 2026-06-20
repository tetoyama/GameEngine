// =======================================================================
//
// ParticleComponentOperations.h
//
// ParticleComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include <algorithm>

#include "Backends/ImGuiFunc.h"
#include "Backends/YAMLConverters.h"

namespace ParticleComponentOperations {

inline YAML::Node Encode(const ParticleComponent& component){
	YAML::Node node;
	node["IsLoop"] = component.isLoop;
	node["SpawnInterval"] = component.SpawnInterval;
	node["SpawnCount"] = component.SpawnCount;
	node["SpawnTimer"] = component.SpawnTimer;
	node["ParticleLifeTime"] = component.particleLifeTime;
	node["ParticleSize"] = component.particleSize;
	node["ParticleSpeed"] = component.particleSpeed;
	node["SpawnPosition"] = component.SpawnPosition;
	node["SpawnPositionRandom"] = component.SpawnPositionRandom;
	node["StartSpeed"] = component.StartSpeed;
	node["StartSpeedRandom"] = component.StartSpeedRandom;
	node["AddSpeed"] = component.AddSpeed;
	node["MulSpeed"] = component.MulSpeed;
	return node;
}

inline bool Decode(ParticleComponent& component, const YAML::Node& node){
	if(!node.IsMap()) return false;

	if(node["IsLoop"]) component.isLoop = node["IsLoop"].as<bool>();
	if(node["SpawnInterval"]) component.SpawnInterval = node["SpawnInterval"].as<float>();
	if(node["SpawnCount"]) component.SpawnCount = node["SpawnCount"].as<int>();
	if(node["SpawnTimer"]) component.SpawnTimer = node["SpawnTimer"].as<float>();
	if(node["ParticleLifeTime"]) component.particleLifeTime = node["ParticleLifeTime"].as<float>();
	if(node["ParticleSize"]) component.particleSize = node["ParticleSize"].as<float>();
	if(node["ParticleSpeed"]) component.particleSpeed = node["ParticleSpeed"].as<float>();
	if(node["SpawnPosition"]) component.SpawnPosition = node["SpawnPosition"].as<Vector3>();
	if(node["SpawnPositionRandom"]) component.SpawnPositionRandom = node["SpawnPositionRandom"].as<Vector3>();
	if(node["StartSpeed"]) component.StartSpeed = node["StartSpeed"].as<Vector3>();
	if(node["StartSpeedRandom"]) component.StartSpeedRandom = node["StartSpeedRandom"].as<Vector3>();
	if(node["AddSpeed"]) component.AddSpeed = node["AddSpeed"].as<Vector3>();
	if(node["MulSpeed"]) component.MulSpeed = node["MulSpeed"].as<Vector3>();

	component.SpawnCount = (std::clamp)(component.SpawnCount, 1, MAXPARTICLE);
	component.SpawnInterval = (std::max)(component.SpawnInterval, 0.001f);
	component.particleLifeTime = (std::max)(component.particleLifeTime, 0.01f);
	component.particleSize = (std::max)(component.particleSize, 0.01f);
	return true;
}

inline void Inspect(ParticleComponent& component){
	if(ImGui::TreeNodeEx("Particle", ImGuiTreeNodeFlags_DefaultOpen)){
		constexpr float labelWidth = 120.0f;
		const float dragWidth =
			ImGui::GetContentRegionAvail().x -
			labelWidth -
			ImGui::GetStyle().ItemSpacing.x * 2.0f;

		ImGui::TextUnformatted("isLoop?");
		ImGui::SameLine(labelWidth);
		ImGui::UndoCheckbox("##isLoop", &component.isLoop);

		if(component.isLoop){
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("SpawnInterval");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(dragWidth);
			ImGui::UndoDragFloat(
				"##SpawnInterval",
				&component.SpawnInterval,
				0.001f,
				0.001f,
				128.0f
			);
		} else {
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("SpawnCount");
			ImGui::SameLine(labelWidth);
			ImGui::SetNextItemWidth(dragWidth);
			ImGui::UndoDragInt(
				"##SpawnCount",
				&component.SpawnCount,
				0.1f,
				1,
				MAXPARTICLE
			);
		}

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("particleLifeTime");
		ImGui::SameLine(labelWidth);
		ImGui::SetNextItemWidth(dragWidth);
		ImGui::UndoDragFloat(
			"##particleLifeTime",
			&component.particleLifeTime,
			0.01f,
			0.01f,
			128.0f
		);

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("particleSize");
		ImGui::SameLine(labelWidth);
		ImGui::SetNextItemWidth(dragWidth);
		ImGui::UndoDragFloat(
			"##particleSize",
			&component.particleSize,
			0.01f,
			0.01f,
			128.0f
		);
		ImGui::TreePop();
	}

	if(ImGui::TreeNodeEx("Spawn Position", ImGuiTreeNodeFlags_DefaultOpen)){
		ImGui::UndoDragVec3("BasePosition", component.SpawnPosition);
		ImGui::UndoDragVec3("RandomPos", component.SpawnPositionRandom);
		ImGui::TreePop();
	}

	if(ImGui::TreeNodeEx("Initial Velocity", ImGuiTreeNodeFlags_DefaultOpen)){
		ImGui::UndoDragVec3("BaseSpeed", component.StartSpeed);
		ImGui::UndoDragVec3("RandomSpeed", component.StartSpeedRandom);
		ImGui::TreePop();
	}

	if(ImGui::TreeNodeEx("Motion", ImGuiTreeNodeFlags_DefaultOpen)){
		ImGui::UndoDragVec3("Acceleration", component.AddSpeed);
		ImGui::UndoDragVec3("Velocity Multiplier", component.MulSpeed);
		ImGui::TreePop();
	}
}

} // namespace ParticleComponentOperations

inline YAML::Node ParticleComponent::encode(){
	return ParticleComponentOperations::Encode(*this);
}

inline bool ParticleComponent::decode(SceneContext*, const YAML::Node& node){
	return ParticleComponentOperations::Decode(*this, node);
}

inline void ParticleComponent::inspector(SceneContext*){
	ParticleComponentOperations::Inspect(*this);
}
