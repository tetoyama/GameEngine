// =======================================================================
//
// CameraComponentSerialization.h
//
// CameraComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include <algorithm>

#include "Resources/resourceService.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"

namespace CameraComponentSerialization {

inline YAML::Node Encode(const CameraComponent& camera){
	YAML::Node node;
	node["isLock"] = camera.isLock;
	node["Target"] = camera.Target;
	node["NearClip"] = camera.NearClip;
	node["FarClip"] = camera.FarClip;
	node["FOV"] = camera.FOV;
	node["viewMatrix"] = camera.viewMatrix;
	node["nextLinkId"] = camera.nextLinkId;
	node["nextPinId"] = camera.nextPinId;
	node["PostEffectEditorZoom"] = camera.postEffectEditorZoom;

	for(const CameraPostEffect& effect : camera.postEffects){
		YAML::Node effectNode;
		effectNode["Name"] = effect.name;
		effectNode["Enabled"] = effect.enabled;
		if(effect.vs) effectNode["VS"] = effect.vs->FilePath;
		if(effect.ps) effectNode["PS"] = effect.ps->FilePath;
		effectNode["NodePos"] = effect.nodePos;
		effectNode["InputPins"] = effect.inputPins;
		effectNode["OutputPin"] = effect.outputPin;
		effectNode["Param"] = effect.Param;
		effectNode["ResolutionScale"] = effect.resolutionScale;
		effectNode["MipLevels"] = effect.mipLevels;
		node["PostEffects"].push_back(effectNode);
	}

	for(const CameraPostEffectLink& link : camera.postEffectLinks){
		YAML::Node linkNode;
		linkNode["ID"] = link.id;
		linkNode["StartNode"] = link.startNode;
		linkNode["EndNode"] = link.endNode;
		linkNode["StartAttr"] = link.startAttr;
		linkNode["EndAttr"] = link.endAttr;
		node["PostEffectLinks"].push_back(linkNode);
	}

	YAML::Node inputNode;
	inputNode["Name"] = camera.screenInputNode.name;
	inputNode["NodePos"] = camera.screenInputNode.nodePos;
	inputNode["OutputPin"] = camera.screenInputNode.outputPin;
	node["ScreenInput"] = inputNode;

	YAML::Node outputNode;
	outputNode["Name"] = camera.screenOutputNode.name;
	outputNode["NodePos"] = camera.screenOutputNode.nodePos;
	outputNode["InputPins"] = camera.screenOutputNode.inputPins;
	node["ScreenOutput"] = outputNode;

	return node;
}

inline bool Decode(
	CameraComponent& camera,
	SceneContext* context,
	const YAML::Node& node
){
	if(!node.IsMap()){
		return false;
	}

	camera.context = context;
	camera.postEffects.clear();
	camera.postEffectLinks.clear();
	camera.cachedSortedPostEffectIndices.clear();
	camera.cachedResolvedPostEffectInputs.clear();

	if(node["isLock"]) camera.isLock = node["isLock"].as<bool>();
	if(node["Target"]) camera.Target = node["Target"].as<Vector3>();
	if(node["NearClip"]) camera.NearClip = node["NearClip"].as<float>();
	if(node["FarClip"]) camera.FarClip = node["FarClip"].as<float>();
	if(node["FOV"]) camera.FOV = node["FOV"].as<float>();
	if(node["viewMatrix"]){
		camera.viewMatrix = node["viewMatrix"].as<DirectX::XMMATRIX>();
	}
	if(node["nextLinkId"]) camera.nextLinkId = node["nextLinkId"].as<int>();
	if(node["nextPinId"]) camera.nextPinId = node["nextPinId"].as<int>();
	if(node["PostEffectEditorZoom"]){
		camera.postEffectEditorZoom = std::clamp(
			node["PostEffectEditorZoom"].as<float>(),
			0.35f,
			2.0f
		);
	}

	ResourceService* resources =
		context && context->manager ? context->manager->resource : nullptr;

	if(node["PostEffects"]){
		for(const YAML::Node& effectNode : node["PostEffects"]){
			CameraPostEffect effect;
			if(effectNode["Name"]) effect.name = effectNode["Name"].as<std::string>();
			if(effectNode["Enabled"]) effect.enabled = effectNode["Enabled"].as<bool>();
			if(effectNode["NodePos"]) effect.nodePos = effectNode["NodePos"].as<Vector2>();
			if(effectNode["InputPins"]){
				effect.inputPins = effectNode["InputPins"].as<std::vector<int>>();
			}
			if(effect.inputPins.empty()) effect.inputPins.push_back(camera.nextPinId++);
			if(effectNode["OutputPin"]) effect.outputPin = effectNode["OutputPin"].as<int>();
			if(effect.outputPin <= 0) effect.outputPin = camera.nextPinId++;
			if(effectNode["Param"]) effect.Param = effectNode["Param"].as<DirectX::XMFLOAT4>();
			if(effectNode["ResolutionScale"]){
				effect.resolutionScale = effectNode["ResolutionScale"].as<float>();
			}
			if(effectNode["MipLevels"]) effect.mipLevels = effectNode["MipLevels"].as<int>();

			if(resources && effectNode["VS"]){
				effect.vs = resources->Load<VertexShaderData>(
					effectNode["VS"].as<std::string>()
				);
			}
			if(resources && effectNode["PS"]){
				effect.ps = resources->Load<PixelShaderData>(
					effectNode["PS"].as<std::string>()
				);
			}

			effect.initialized = false;
			camera.postEffects.push_back(std::move(effect));
		}
	}

	if(node["PostEffectLinks"]){
		for(const YAML::Node& linkNode : node["PostEffectLinks"]){
			CameraPostEffectLink link;
			if(linkNode["ID"]) link.id = linkNode["ID"].as<int>();
			if(linkNode["StartNode"]) link.startNode = linkNode["StartNode"].as<int>();
			if(linkNode["EndNode"]) link.endNode = linkNode["EndNode"].as<int>();
			if(linkNode["StartAttr"]) link.startAttr = linkNode["StartAttr"].as<int>();
			if(linkNode["EndAttr"]) link.endAttr = linkNode["EndAttr"].as<int>();
			camera.postEffectLinks.push_back(link);
			if(link.id >= camera.nextLinkId){
				camera.nextLinkId = link.id + 1;
			}
		}
	}

	if(node["ScreenInput"]){
		const YAML::Node inputNode = node["ScreenInput"];
		if(inputNode["Name"]) camera.screenInputNode.name = inputNode["Name"].as<std::string>();
		if(inputNode["NodePos"]){
			camera.screenInputNode.nodePos = inputNode["NodePos"].as<Vector2>();
		}
		if(inputNode["OutputPin"]){
			camera.screenInputNode.outputPin = inputNode["OutputPin"].as<int>();
		}
	} else {
		camera.screenInputNode.name.clear();
		camera.screenInputNode.outputPin = camera.nextPinId++;
		camera.screenInputNode.nodePos = Vector2(50.0f, 50.0f);
	}
	camera.screenInputNode.initialized = false;

	if(node["ScreenOutput"]){
		const YAML::Node outputNode = node["ScreenOutput"];
		if(outputNode["Name"]) camera.screenOutputNode.name = outputNode["Name"].as<std::string>();
		if(outputNode["NodePos"]){
			camera.screenOutputNode.nodePos = outputNode["NodePos"].as<Vector2>();
		}
		if(outputNode["InputPins"]){
			camera.screenOutputNode.inputPins =
				outputNode["InputPins"].as<std::vector<int>>();
		}
	} else {
		camera.screenOutputNode.name.clear();
		camera.screenOutputNode.nodePos = Vector2(500.0f, 50.0f);
	}
	if(camera.screenOutputNode.inputPins.empty()){
		camera.screenOutputNode.inputPins.push_back(camera.nextPinId++);
	}
	camera.screenOutputNode.initialized = false;

	camera.initialized = true;
	camera.InvalidatePostEffectGraphCache();
	return true;
}

} // namespace CameraComponentSerialization

inline YAML::Node CameraComponent::encode(){
	return CameraComponentSerialization::Encode(*this);
}

inline bool CameraComponent::decode(
	SceneContext* context,
	const YAML::Node& node
){
	return CameraComponentSerialization::Decode(*this, context, node);
}
