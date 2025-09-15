#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"
#include "Service/YAMLConverters.h"
#include "Service/ImGuiFunc.h"
#include "Registry/componentRegistry.h"
#include "GameApplication/Engine/DebugTools/ImGuiSystem.h"
#include <DirectXMath.h>

class TransformComponent: public IComponent {
private:
	Vector3 rotationEular;
	DirectX::XMFLOAT4 rotation = {0, 0, 0, 1}; // クォータニオン

public:
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Entity parent = 0;
	// rotation を XMVECTOR として取得
	DirectX::XMVECTOR rotationVector() const{
		return DirectX::XMLoadFloat4(&rotation);
	}

	void SetRotationX(float x){
		Vector3 euler = GetRotationEuler();
		euler.x = x;
		SetRotationEuler(euler);
	}

	void SetRotationY(float y){
		Vector3 euler = GetRotationEuler();
		euler.y = y;
		SetRotationEuler(euler);
	}

	void SetRotationZ(float z){
		Vector3 euler = GetRotationEuler();
		euler.z = z;
		SetRotationEuler(euler);
	}

	void AddRotationX(float x){
		Vector3 euler = GetRotationEuler();
		euler.x += x;
		SetRotationEuler(euler);
	}

	void AddRotationY(float y){
		Vector3 euler = GetRotationEuler();
		euler.y += y;
		SetRotationEuler(euler);
	}

	void AddRotationZ(float z){
		Vector3 euler = GetRotationEuler();
		euler.z += z;
		SetRotationEuler(euler);
	}

	// オイラー角 (ラジアン) から回転を設定する
	void SetRotationEuler(const Vector3& euler){
		rotationEular = euler;
		DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(
			euler.x, // Pitch (X軸)
			euler.y, // Yaw   (Y軸)
			euler.z  // Roll  (Z軸)
		);
		DirectX::XMStoreFloat4(&rotation, q);
	}

	void SetRotation(const DirectX::XMFLOAT4 q){
		rotation = q;
		rotationEular = GetRotationEuler();
	}

	const DirectX::XMFLOAT4& GetRotation() const{
		return rotation;
	}

	// オイラー角 (ラジアン) を取得する
	Vector3 GetRotationEuler() const{
		DirectX::XMVECTOR q = rotationVector();
		DirectX::XMFLOAT4 qf;
		DirectX::XMStoreFloat4(&qf, q);

		// クォータニオン → オイラー角変換
		float ysqr = qf.y * qf.y;

		float t0 = +2.0f * (qf.w * qf.x + qf.y * qf.z);
		float t1 = +1.0f - 2.0f * (qf.x * qf.x + ysqr);
		float roll = atan2f(t0, t1);

		float t2 = +2.0f * (qf.w * qf.y - qf.z * qf.x);
		t2 = (t2 > 1.0f) ? 1.0f : t2;
		t2 = (t2 < -1.0f) ? -1.0f : t2;
		float pitch = asinf(t2);

		float t3 = +2.0f * (qf.w * qf.z + qf.x * qf.y);
		float t4 = +1.0f - 2.0f * (ysqr + qf.z * qf.z);
		float yaw = atan2f(t3, t4);

		return Vector3(roll, pitch, yaw);
	}
	YAML::Node encode() override{
		YAML::Node node;
		node["Position"] = position;
		node["Rotation"] = std::vector<float>{rotation.x, rotation.y, rotation.z, rotation.w};
		node["Scale"] = scale;
		node["Parent"] = (int)parent;
		return node;
	}

	bool decode(const YAML::Node& node) override{
		if(!node.IsMap()){
			return false;
		}

		if(node["Position"]){
			position = node["Position"].as<Vector3>();
		}
		if(node["Rotation"] && node["Rotation"].IsSequence() && node["Rotation"].size() == 4){
			auto q = node["Rotation"].as<std::vector<float>>();
			rotation = {q[0], q[1], q[2], q[3]};
		}
		if(node["Scale"]){
			scale = node["Scale"].as<Vector3>();
		}
		if(node["Parent"]){
			parent = Entity(node["Parent"].as<int>());
		}
		return true;
	}

	void inspector(SceneContext* context) override{
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
			ImGui::SetCursorPosX(labelWidth);

			auto DrawComponent = [&](const char* id, float& value, const ImVec4& borderColor, const char* uniqueId, bool isLast){
				ImGui::PushID(uniqueId);
				ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
				ImGui::PushItemWidth(fieldWidth - 10.0f);

				ImGui::Text("%s", id);
				ImGui::SameLine(0.0f, 10.0f);
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

		// ----------- Rotation (Euler UI) -----------
		{
			float e[3] = {rotationEular.x, rotationEular.y, rotationEular.z};
			if(ImGui::DragFloat3("Rotation (Euler)", e, 0.01f)){
				DirectX::XMVECTOR qNew = DirectX::XMQuaternionRotationRollPitchYaw(
					e[0], e[1], e[2]
				);
				DirectX::XMStoreFloat4(&rotation, qNew);
				rotationEular.x = e[0];
				rotationEular.y = e[1];
				rotationEular.z = e[2];
			}

		}


		// ----------- Scale -----------
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Scale");
		ImGui::SameLine();

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
			colorX = ImVec4(1.0f, 0.8f, 0.8f, 0.6f);
			colorY = ImVec4(0.8f, 1.0f, 0.8f, 0.6f);
			colorZ = ImVec4(0.8f, 0.8f, 1.0f, 0.6f);

			float ratio = 1.0f;
			bool changed = false;

			auto DrawLockedComponent = [&](const char* id, float& value, float baseValue, float& outRatio, const ImVec4& borderColor, bool isLast){
				float temp = value;
				ImGui::PushID(id);
				ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
				ImGui::PushItemWidth(fieldWidth - 10.0f);
				ImGui::Text("%s", id);
				ImGui::SameLine(0.0f, 10.0f);
				if(ImGui::DragFloat("##", &temp, 0.01f, 0.01f)){
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
		ImGui::InputInt("Parent Entity", (int*)&parent);

		ImGui::PopStyleVar(); // ItemSpacing
	}

	Vector3 front() const{
		DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&rotation);
		DirectX::XMVECTOR f = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0, 0, 1, 0), q);
		DirectX::XMFLOAT3 out;
		XMStoreFloat3(&out, f);
		return Vector3(out.x, out.y, out.z);
	}

	Vector3 up() const{
		DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&rotation);
		DirectX::XMVECTOR u = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0, 1, 0, 0), q);
		DirectX::XMFLOAT3 out;
		XMStoreFloat3(&out, u);
		return Vector3(out.x, out.y, out.z);
	}

	Vector3 right() const{
		DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&rotation);
		DirectX::XMVECTOR r = DirectX::XMVector3Rotate(DirectX::XMVectorSet(1, 0, 0, 0), q);
		DirectX::XMFLOAT3 out;
		XMStoreFloat3(&out, r);
		return Vector3(out.x, out.y, out.z);
	}

	DirectX::XMMATRIX CalculateWorldMatrix(TransformComponent* transform, ComponentRegistry* registry){
		DirectX::XMMATRIX local =
			DirectX::XMMatrixScaling(transform->scale.x, transform->scale.y, transform->scale.z) *
			DirectX::XMMatrixRotationQuaternion(XMLoadFloat4(&transform->rotation)) *
			DirectX::XMMatrixTranslation(transform->position.x, transform->position.y, transform->position.z);

		if(transform->parent != 0){
			auto* parentTransform = registry->GetComponent<TransformComponent>(transform->parent);
			if(parentTransform && parentTransform->parent != transform->parent){
				return local * CalculateWorldMatrix(parentTransform, registry);
			}
		}
		return local;
	}
};
