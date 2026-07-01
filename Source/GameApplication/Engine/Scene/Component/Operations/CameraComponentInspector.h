// =======================================================================
//
// CameraComponentInspector.h
//
// CameraComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "buildSetting.h"
#include "Backends/ImGui/Imnodes.h"
#include "Backends/ImGuiFunc.h"
#include "Resources/resourceService.h"
#include "Scene/scene.h"

namespace CameraComponentInspector {

inline constexpr float PostEffectNodeContentWidth = 150.0f;
inline constexpr float PostEffectNodeNameWidth = 150.0f;
inline constexpr float PostEffectNodePreviewWidth = 120.0f;
inline constexpr float PostEffectNodeOutputColumnX = 128.0f;
inline constexpr float PostEffectEditorZoomMin = 0.35f;
inline constexpr float PostEffectEditorZoomMax = 2.0f;

inline float ResolveEditorZoom(CameraComponent& camera){
	camera.postEffectEditorZoom = std::clamp(
		camera.postEffectEditorZoom,
		PostEffectEditorZoomMin,
		PostEffectEditorZoomMax
	);
	return camera.postEffectEditorZoom;
}

inline float Zoomed(CameraComponent& camera, float value){
	return value * ResolveEditorZoom(camera);
}

inline ImVec2 ToZoomedEditorPos(CameraComponent& camera, const Vector2& position){
	const float zoom = ResolveEditorZoom(camera);
	return ImVec2(position.x * zoom, position.y * zoom);
}

inline Vector2 FromZoomedEditorPos(CameraComponent& camera, const ImVec2& position){
	const float zoom = ResolveEditorZoom(camera);
	const float safeZoom = (std::max)(zoom, 0.0001f);
	return Vector2(position.x / safeZoom, position.y / safeZoom);
}

inline void EnsureGraphInitialized(CameraComponent& camera){
	if(camera.initialized){
		return;
	}

	camera.postEffectLinks.clear();
	if(camera.screenInputNode.outputPin <= 0){
		camera.screenInputNode.outputPin = camera.nextPinId++;
	}
	if(camera.screenOutputNode.inputPins.empty()){
		camera.screenOutputNode.inputPins.push_back(camera.nextPinId++);
	}

	camera.postEffectLinks.push_back({
		camera.nextLinkId++,
		-1,
		-2,
		camera.screenInputNode.outputPin,
		camera.screenOutputNode.inputPins.front()
	});
	camera.InvalidatePostEffectGraphCache();
	camera.initialized = true;
}

inline void DrawCameraParameters(CameraComponent& camera){
	ImGui::Text("NearClip");
	ImGui::SameLine(100.0f);
	ImGui::UndoDragFloat(
		"##NearClip",
		&camera.NearClip,
		0.01f,
		0.01f,
		camera.FarClip - 0.01f
	);

	ImGui::Text("FarClip");
	ImGui::SameLine(100.0f);
	ImGui::UndoDragFloat(
		"##FarClip",
		&camera.FarClip,
		0.01f,
		camera.NearClip + 0.01f,
		4096.0f
	);

	camera.NearClip = (std::max)(camera.NearClip, 0.01f);
	camera.FarClip = (std::max)(camera.FarClip, camera.NearClip + 0.01f);

	ImGui::Text("FOV");
	ImGui::SameLine(100.0f);
	ImGui::UndoDragFloat("##FOV", &camera.FOV, 0.01f, 0.01f);
	camera.FOV = (std::max)(camera.FOV, 0.01f);

	ImGui::Text("isLock");
	ImGui::SameLine(100.0f);
	if(ImGui::Button(camera.isLock ? "On" : "Off")){
		camera.isLock = !camera.isLock;
	}

	if(camera.isLock){
		ImGui::Text("Target Position");
		ImGui::SameLine(100.0f);
		ImGui::UndoDragFloat3("##Target", &camera.Target.x, 0.01f);
	}
}

inline void DrawScreenInputNode(CameraComponent& camera){
	ImNodes::BeginNode(-1);
	ImNodes::BeginNodeTitleBar();
	ImGui::TextUnformatted("ScreenInput");
	ImNodes::EndNodeTitleBar();
	ImNodes::BeginOutputAttribute(camera.screenInputNode.outputPin);
	ImGui::TextUnformatted("Output");
	ImNodes::EndOutputAttribute();
	ImNodes::EndNode();

	if(!camera.screenInputNode.initialized){
		ImNodes::SetNodeEditorSpacePos(
			-1,
			ToZoomedEditorPos(camera, camera.screenInputNode.nodePos)
		);
		camera.screenInputNode.initialized = true;
	}

	camera.screenInputNode.nodePos = FromZoomedEditorPos(
		camera,
		ImNodes::GetNodeEditorSpacePos(-1)
	);
}

inline void DrawScreenOutputNode(CameraComponent& camera){
	if(camera.screenOutputNode.inputPins.empty()){
		camera.screenOutputNode.inputPins.push_back(camera.nextPinId++);
		camera.InvalidatePostEffectGraphCache();
	}

	ImNodes::BeginNode(-2);
	ImNodes::BeginNodeTitleBar();
	ImGui::TextUnformatted("ScreenOutput");
	ImNodes::EndNodeTitleBar();
	ImNodes::BeginInputAttribute(camera.screenOutputNode.inputPins.front());
	ImGui::TextUnformatted("Input");
	ImNodes::EndInputAttribute();
	ImNodes::EndNode();

	if(!camera.screenOutputNode.initialized){
		ImNodes::SetNodeEditorSpacePos(
			-2,
			ToZoomedEditorPos(camera, camera.screenOutputNode.nodePos)
		);
		camera.screenOutputNode.initialized = true;
	}

	camera.screenOutputNode.nodePos = FromZoomedEditorPos(
		camera,
		ImNodes::GetNodeEditorSpacePos(-2)
	);
}

inline void LoadShaderFromPath(
	CameraPostEffect& effect,
	SceneContext* context,
	const std::string& path,
	bool pixelShader
){
	if(!context || !context->manager || !context->manager->resource || path.empty()){
		return;
	}

	if(pixelShader){
		effect.ps = context->manager->resource->Load<PixelShaderData>(path);
	} else {
		effect.vs = context->manager->resource->Load<VertexShaderData>(path);
	}
}

inline void DrawShaderField(
	CameraComponent& camera,
	CameraPostEffect& effect,
	SceneContext* context,
	bool pixelShader,
	int nodeId
){
	char pathBuffer[512]{};
	const std::string currentPath = pixelShader
		? (effect.ps ? effect.ps->FilePath : std::string{})
		: (effect.vs ? effect.vs->FilePath : std::string{});
	std::strncpy(pathBuffer, currentPath.c_str(), sizeof(pathBuffer) - 1);

	const std::string label =
		std::string(pixelShader ? "PS##" : "VS##") + std::to_string(nodeId);
	ImGui::SetNextItemWidth(Zoomed(camera, PostEffectNodeContentWidth));
	if(ImGui::InputText(label.c_str(), pathBuffer, sizeof(pathBuffer))){
		LoadShaderFromPath(effect, context, pathBuffer, pixelShader);
	}

	if(ImGui::BeginDragDropTarget()){
		if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
			const char* droppedPath = static_cast<const char*>(payload->Data);
			if(droppedPath){
				const std::string path(droppedPath);
				if(path.find(".cso") != std::string::npos){
					LoadShaderFromPath(effect, context, path, pixelShader);
				}
			}
		}
		ImGui::EndDragDropTarget();
	}
}

inline void DrawEffectNode(
	CameraComponent& camera,
	CameraPostEffect& effect,
	SceneContext* context,
	int effectIndex
){
	const int nodeId = effectIndex + 2;
	ImNodes::BeginNode(nodeId);

	if(!effect.initialized){
		ImNodes::SetNodeEditorSpacePos(
			nodeId,
			ToZoomedEditorPos(camera, effect.nodePos)
		);
		effect.initialized = true;
	} else {
		effect.nodePos = FromZoomedEditorPos(
			camera,
			ImNodes::GetNodeEditorSpacePos(nodeId)
		);
	}

	ImNodes::BeginNodeTitleBar();
	char nameBuffer[128]{};
	std::strncpy(nameBuffer, effect.name.c_str(), sizeof(nameBuffer) - 1);
	ImGui::SetNextItemWidth(Zoomed(camera, PostEffectNodeNameWidth));
	const std::string nameLabel = "##NodeName" + std::to_string(nodeId);
	if(ImGui::InputText(nameLabel.c_str(), nameBuffer, sizeof(nameBuffer))){
		effect.name = nameBuffer;
	}
	ImNodes::EndNodeTitleBar();

	ImGui::PushItemWidth(Zoomed(camera, PostEffectNodeContentWidth));
	ImGui::UndoCheckbox(("Enabled##" + std::to_string(nodeId)).c_str(), &effect.enabled);
	ImGui::UndoDragFloat4(
		("Param##" + std::to_string(nodeId)).c_str(),
		&effect.Param.x,
		0.01f
	);
	ImGui::UndoDragFloat(
		("Scale##" + std::to_string(nodeId)).c_str(),
		&effect.resolutionScale,
		0.01f,
		0.1f,
		1.0f
	);
	ImGui::UndoDragInt(
		("MipLevels##" + std::to_string(nodeId)).c_str(),
		&effect.mipLevels,
		1.0f,
		1,
		8
	);
	ImGui::PopItemWidth();

	DrawShaderField(camera, effect, context, true, nodeId);
	DrawShaderField(camera, effect, context, false, nodeId);

	if(effect.srv){
		const float previewWidth = Zoomed(camera, PostEffectNodePreviewWidth);
		ImGui::Image(
			reinterpret_cast<ImTextureID>(effect.srv.Get()),
			ImVec2(previewWidth, previewWidth / 16.0f * 9.0f)
		);
	} else {
		ImGui::TextDisabled("No preview");
	}

	if(effect.inputPins.empty()){
		effect.inputPins.push_back(camera.nextPinId++);
		camera.InvalidatePostEffectGraphCache();
	}

	bool firstInput = true;
	int inputIndex = 0;
	for(int pinId : effect.inputPins){
		ImNodes::BeginInputAttribute(pinId);
		ImGui::Text("Input[%d]", inputIndex++);
		ImNodes::EndInputAttribute();

		if(firstInput){
			ImGui::SameLine(Zoomed(camera, PostEffectNodeOutputColumnX));
			ImNodes::BeginOutputAttribute(effect.outputPin);
			ImGui::TextUnformatted("Output");
			ImNodes::EndOutputAttribute();
			firstInput = false;
		}
	}

	bool allInputsConnected = true;
	for(int pinId : effect.inputPins){
		const auto iterator = std::find_if(
			camera.postEffectLinks.begin(),
			camera.postEffectLinks.end(),
			[pinId](const CameraPostEffectLink& link){
				return link.endAttr == pinId;
			}
		);
		if(iterator == camera.postEffectLinks.end()){
			allInputsConnected = false;
			break;
		}
	}

	if(allInputsConnected){
		effect.inputPins.push_back(camera.nextPinId++);
		camera.InvalidatePostEffectGraphCache();
	}

	ImNodes::EndNode();
}

inline void DrawLinks(CameraComponent& camera){
	for(auto iterator = camera.postEffectLinks.begin();
		iterator != camera.postEffectLinks.end();){
		ImNodes::Link(iterator->id, iterator->startAttr, iterator->endAttr);
		if(ImNodes::IsLinkSelected(iterator->id) &&
			ImGui::GetMouseClickedCount(ImGuiMouseButton_Left) > 1){
			iterator = camera.postEffectLinks.erase(iterator);
			camera.InvalidatePostEffectGraphCache();
		} else {
			++iterator;
		}
	}
}

inline void DeleteEffectNode(CameraComponent& camera, int effectIndex){
	if(effectIndex < 0 || effectIndex >= static_cast<int>(camera.postEffects.size())){
		return;
	}

	camera.postEffectLinks.erase(
		std::remove_if(
			camera.postEffectLinks.begin(),
			camera.postEffectLinks.end(),
			[effectIndex](const CameraPostEffectLink& link){
				return link.startNode == effectIndex || link.endNode == effectIndex;
			}
		),
		camera.postEffectLinks.end()
	);

	camera.postEffects.erase(camera.postEffects.begin() + effectIndex);
	for(CameraPostEffectLink& link : camera.postEffectLinks){
		if(link.startNode > effectIndex) --link.startNode;
		if(link.endNode > effectIndex) --link.endNode;
	}
	camera.InvalidatePostEffectGraphCache();
}

inline void DrawNodeContextMenu(CameraComponent& camera){
	static int hoveredNode = -1000;

	if(ImGui::BeginPopup("CameraPostEffectNodeContextMenu")){
		if(hoveredNode >= 2){
			if(ImGui::MenuItem("Delete Node")){
				DeleteEffectNode(camera, hoveredNode - 2);
			}
		} else {
			ImGui::TextDisabled("Cannot delete this node");
		}
		ImGui::EndPopup();
	} else if(ImNodes::IsNodeHovered(&hoveredNode) &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Right)){
		ImGui::OpenPopup("CameraPostEffectNodeContextMenu");
	}
}

inline void AcceptNewLink(CameraComponent& camera){
	int startAttribute = 0;
	int endAttribute = 0;
	if(!ImNodes::IsLinkCreated(&startAttribute, &endAttribute)){
		return;
	}

	CameraPostEffectLink link;
	link.id = camera.nextLinkId++;
	link.startAttr = startAttribute;
	link.endAttr = endAttribute;
	link.startNode = -1;
	link.endNode = -1;

	if(startAttribute == camera.screenInputNode.outputPin){
		link.startNode = -1;
	}
	if(std::find(
		camera.screenOutputNode.inputPins.begin(),
		camera.screenOutputNode.inputPins.end(),
		endAttribute
	) != camera.screenOutputNode.inputPins.end()){
		link.endNode = -2;
	}

	for(int index = 0; index < static_cast<int>(camera.postEffects.size()); ++index){
		const CameraPostEffect& effect = camera.postEffects[index];
		if(effect.outputPin == startAttribute){
			link.startNode = index;
		}
		if(std::find(
			effect.inputPins.begin(),
			effect.inputPins.end(),
			endAttribute
		) != effect.inputPins.end()){
			link.endNode = index;
		}
	}

	if(link.endNode != -1){
		camera.postEffectLinks.push_back(link);
		camera.InvalidatePostEffectGraphCache();
	}
}

inline void AddEffect(CameraComponent& camera, SceneContext* context){
	CameraPostEffect effect;
	effect.name = "NewEffect";
	effect.enabled = true;
	effect.initialized = false;
	effect.nodePos = Vector2(50.0f, 150.0f + 120.0f * camera.postEffects.size());
	effect.inputPins.push_back(camera.nextPinId++);
	effect.outputPin = camera.nextPinId++;

	if(context && context->manager && context->manager->resource){
		effect.vs = context->manager->resource->Load<VertexShaderData>(
			DEFAULT_WINDOW_POSTEFFECT_VS_PATH
		);
		effect.ps = context->manager->resource->Load<PixelShaderData>(
			DEFAULT_WINDOW_POSTEFFECT_PS_PATH
		);
	}

	camera.postEffects.push_back(std::move(effect));
	camera.InvalidatePostEffectGraphCache();
}

inline void DrawPostEffectZoomControls(CameraComponent& camera){
	ResolveEditorZoom(camera);
	ImGui::TextUnformatted("Canvas Zoom");
	ImGui::SameLine(100.0f);
	ImGui::PushItemWidth(180.0f);
	ImGui::SliderFloat(
		"##PostEffectCanvasZoom",
		&camera.postEffectEditorZoom,
		PostEffectEditorZoomMin,
		PostEffectEditorZoomMax,
		"%.2fx"
	);
	ImGui::PopItemWidth();
	ImGui::SameLine();
	if(ImGui::SmallButton("Reset##PostEffectCanvasZoom")){
		camera.postEffectEditorZoom = 1.0f;
	}
	ResolveEditorZoom(camera);
}

inline void Inspect(CameraComponent& camera, SceneContext* context){
	EnsureGraphInitialized(camera);
	camera.context = context;

	ImGui::PushID(&camera);
	DrawCameraParameters(camera);

	ImGui::Separator();
	ImGui::TextUnformatted("Post-Process Node Editor");
	DrawPostEffectZoomControls(camera);
	ImGui::BeginChild("NodeEditorRegion", ImVec2(0.0f, 400.0f), true);

	const float zoom = ResolveEditorZoom(camera);
	ImNodes::PushStyleVar(ImNodesStyleVar_NodePadding, ImVec2(4.0f * zoom, 4.0f * zoom));
	ImNodes::PushStyleVar(ImNodesStyleVar_PinCircleRadius, 3.0f * zoom);
	ImNodes::PushStyleVar(ImNodesStyleVar_PinHoverRadius, 10.0f * zoom);
	ImNodes::PushStyleVar(ImNodesStyleVar_LinkThickness, 1.0f * zoom);
	ImNodes::PushStyleVar(ImNodesStyleVar_GridSpacing, 16.0f * zoom);
	ImNodes::BeginNodeEditor();

	DrawScreenInputNode(camera);
	DrawScreenOutputNode(camera);
	for(int index = 0; index < static_cast<int>(camera.postEffects.size()); ++index){
		DrawEffectNode(camera, camera.postEffects[index], context, index);
	}
	DrawLinks(camera);

	ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomRight);
	ImNodes::EndNodeEditor();
	DrawNodeContextMenu(camera);
	AcceptNewLink(camera);

	ImNodes::PopStyleVar(5);
	ImGui::EndChild();

	if(ImGui::Button("Add Node")){
		AddEffect(camera, context);
	}

	ImGui::PopID();
}

} // namespace CameraComponentInspector

inline void CameraComponent::inspector(SceneContext* context){
	CameraComponentInspector::Inspect(*this, context);
}
