#pragma once
#include "Interface/IComponent.h"
#include "Service/YAMLConverters.h"
#include "Backends/myVector3.h"
#include <DirectXMath.h>
#include "Engine/Resources/ResourceService.h"

#include "Engine/Resources/Data/vertexShaderData.h"
#include "Engine/Resources/Data/pixelShaderData.h"
#include "Engine/Resources/Loader/shaderLoader.h"
#include "Engine/Graphics/graphicsContext.h"
#include "scene.h"

struct CameraPostEffect {
	std::shared_ptr<PixelShaderData> ps;
	std::shared_ptr<VertexShaderData> vs; // optional
	std::string name;
	bool enabled = true;
};

class CameraComponent: public IComponent {
public:
	SceneContext* context = nullptr;

	YAML::Node encode() override{
		YAML::Node node;
		node["isLock"] = isLock;
		node["Target"] = Target;
		node["NearClip"] = NearClip;
		node["FarClip"] = FarClip;
		node["FOV"] = FOV;
		node["viewMatrix"] = viewMatrix;

		for(auto& effect : postEffects){
			YAML::Node e;
			e["Name"] = effect.name;
			e["Enabled"] = effect.enabled;
			if(effect.vs) e["VS"] = effect.vs->FilePath;
			if(effect.ps) e["PS"] = effect.ps->FilePath;
			node["PostEffects"].push_back(e);
		}
		return node;
	}

	bool decode(const YAML::Node& node) override{
		if(node["isLock"]) isLock = node["isLock"].as<bool>();
		if(node["Target"]) Target = node["Target"].as<Vector3>();
		if(node["NearClip"]) NearClip = node["NearClip"].as<float>();
		if(node["FarClip"]) FarClip = node["FarClip"].as<float>();
		if(node["FOV"]) FOV = node["FOV"].as<float>();
		if(node["viewMatrix"]) viewMatrix = node["viewMatrix"].as<DirectX::XMMATRIX>();

		if(node["PostEffects"] && context){
			for(auto eNode : node["PostEffects"]){
				CameraPostEffect effect;
				effect.name = eNode["Name"].as<std::string>();
				effect.enabled = eNode["Enabled"].as<bool>();
				if(eNode["VS"]) effect.vs = context->manager->resource->Load<VertexShaderData>(eNode["VS"].as<std::string>());
				if(eNode["PS"]) effect.ps = context->manager->resource->Load<PixelShaderData>(eNode["PS"].as<std::string>());
				postEffects.push_back(effect);
			}
		}
		return true;
	}

	void inspector(SceneContext* ctx) override{
		context = ctx;

		ImGui::PushID(this);

		// --- Camera Properties ---
		ImGui::Text("NearClip"); ImGui::SameLine(100); ImGui::DragFloat("##NearClip", &NearClip, 0.01f, 0.01f, FarClip - 0.01f);
		ImGui::Text("FarClip"); ImGui::SameLine(100); ImGui::DragFloat("##FarClip", &FarClip, 0.01f, NearClip + 0.01f, 4096.0f);
		if(NearClip <= 0.0f) NearClip = 0.01f;
		if(FarClip <= NearClip) FarClip = NearClip + 0.01f;

		ImGui::Text("FOV"); ImGui::SameLine(100); ImGui::DragFloat("##FOV", &FOV, 0.01f, 0.01f); if(FOV <= 0.0f) FOV = 0.01f;

		ImGui::Text("isLock"); ImGui::SameLine(100); if(ImGui::Button(isLock ? "On" : "Off")) isLock = !isLock;
		if(isLock){
			ImGui::Text("Target Position"); ImGui::SameLine(100); ImGui::DragFloat3("##Target", &Target.x, 0.01f);
		}

		char filepathBuffer[256];

		// --- Post Effects ---
		ImGui::Separator();
		ImGui::Text("Post-Process Effects");
		int idx = 0;
		for(auto& effect : postEffects){
			ImGui::PushID(idx);
			ImGui::Checkbox("##Enabled", &effect.enabled); ImGui::SameLine();
			ImGui::InputText("Name", &effect.name[0], effect.name.size() + 1);

			// --- Pixel Shader ---
			if(effect.ps) strncpy_s(filepathBuffer, sizeof(filepathBuffer), effect.ps->FilePath.c_str(), _TRUNCATE);
			else filepathBuffer[0] = '\0';

			if(ImGui::InputText("PS", filepathBuffer, sizeof(filepathBuffer)) && context)
				effect.ps = context->manager->resource->Load<PixelShaderData>(filepathBuffer);

			// Drag&Drop (PS)
			if(ImGui::BeginDragDropTarget() && context){
				if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
					const char* droppedPath = (const char*)payload->Data;
					std::string path(droppedPath);
					if(path.find(".cso") != std::string::npos){
						effect.ps = context->manager->resource->Load<PixelShaderData>(path);
					}
				}
				ImGui::EndDragDropTarget();
			}

			// --- Vertex Shader ---
			if(effect.vs) strncpy_s(filepathBuffer, sizeof(filepathBuffer), effect.vs->FilePath.c_str(), _TRUNCATE);
			else filepathBuffer[0] = '\0';

			if(ImGui::InputText("VS", filepathBuffer, sizeof(filepathBuffer)) && context)
				effect.vs = context->manager->resource->Load<VertexShaderData>(filepathBuffer);

			// Drag&Drop (VS)
			if(ImGui::BeginDragDropTarget() && context){
				if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
					const char* droppedPath = (const char*)payload->Data;
					std::string path(droppedPath);
					if(path.find(".cso") != std::string::npos){
						effect.vs = context->manager->resource->Load<VertexShaderData>(path);
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::PopID();
			idx++;
		}

		if(ImGui::Button("Add Effect") && context){
			CameraPostEffect newEffect;
			newEffect.name = "NewEffect"; newEffect.enabled = true;
			postEffects.push_back(newEffect);
		}

		ImGui::PopID();
	}

	// --- Camera Properties ---
	bool isLock = false;
	Vector3 Target{0,0,0};
	float NearClip = 0.01f;
	float FarClip = 1024.0f;
	float FOV = 1.0f;
	DirectX::XMMATRIX viewMatrix{};

	// --- Shaders ---
	std::vector<CameraPostEffect> postEffects;
};
