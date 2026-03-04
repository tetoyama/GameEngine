// =======================================================================
// 
// cameraComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include "Backends/YAMLConverters.h"
#include "Backends/myVector2.h"
#include "Backends/myVector3.h"
#include "Backends/ImGui/Imnodes.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <queue>
#include "Resources/ResourceService.h"
#include "Resources/Data/vertexShaderData.h"
#include "Resources/Data/pixelShaderData.h"
#include "Resources/Loader/shaderLoader.h"
#include "Graphics/graphicsContext.h"
#include "scene.h"
#include "sceneManager.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>


struct CameraPostEffect {
    std::shared_ptr<PixelShaderData> ps;
    std::shared_ptr<VertexShaderData> vs;
    std::string name;
    bool enabled = true;
    Vector2 nodePos{ -1, -1 };
	DirectX::XMFLOAT4 Param{0,0,0,0};
    bool initialized = false;
    std::vector<int> inputPins;
    int outputPin = -1;

	float resolutionScale = 1.0f;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	Vector2 resolution{1280, 720};

	void CreateTexture(ID3D11Device* device, const Vector2& screenSize){
		if (tex) return;
		resolution = screenSize;
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = static_cast<UINT>(resolution.x);
		desc.Height = static_cast<UINT>(resolution.y);
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		// HDR float16: エミッシブ / ブルームの HDR 値を保持する
		desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		HRESULT hr = device->CreateTexture2D(&desc, nullptr, &tex);
		if (FAILED(hr)) {
			OutputDebugStringA("Failed to create post effect texture\n");
			return;
		}
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = desc.Format;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		hr = device->CreateRenderTargetView(tex.Get(), &rtvDesc, &rtv);
		if (FAILED(hr)) {
			OutputDebugStringA("Failed to create post effect RTV\n");
			return;
		}
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = desc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		hr = device->CreateShaderResourceView(tex.Get(), &srvDesc, &srv);
		if (FAILED(hr)) {
			OutputDebugStringA("Failed to create post effect SRV\n");
			return;
		}
	}

	void ResizeTexture(ID3D11Device* device, const Vector2& screenSize){
		if (resolution.x == screenSize.x && resolution.y == screenSize.y && tex && rtv && srv) return;
		tex = nullptr;
		rtv = nullptr;
		srv = nullptr;
		CreateTexture(device, screenSize);
	}

	void Clear(ID3D11DeviceContext* context, const float clearColor[4]){
		if (rtv) {
			context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
			context->ClearRenderTargetView(rtv.Get(), clearColor);
		}
	}
};

struct CameraPostEffectLink {
    int id = -1;
    int startNode = -1;
    int endNode = -1;
    int startAttr = -1;
    int endAttr = -1;
};

class CameraComponent : public IComponent {
public:
    SceneContext* context = nullptr;

    std::vector<CameraPostEffect> postEffects;
    std::vector<CameraPostEffectLink> postEffectLinks;

    CameraPostEffect screenInputNode;
    CameraPostEffect screenOutputNode;

    int nextLinkId = 1;
    int nextPinId = 1;

	bool initialized = false;

    YAML::Node encode() override {
        YAML::Node node;
        node["isLock"] = isLock;
        node["Target"] = Target;
        node["NearClip"] = NearClip;
        node["FarClip"] = FarClip;
        node["FOV"] = FOV;
        node["viewMatrix"] = viewMatrix;
        node["nextLinkId"] = nextLinkId;
		node["nextPinId"] = nextPinId;

        for (auto& effect : postEffects) {
            YAML::Node e;
            e["Name"] = effect.name;
            e["Enabled"] = effect.enabled;
            if (effect.vs) e["VS"] = effect.vs->FilePath;
            if (effect.ps) e["PS"] = effect.ps->FilePath;
            e["NodePos"] = effect.nodePos;
            e["InputPins"] = effect.inputPins;
            e["OutputPin"] = effect.outputPin;
			e["Param"] = effect.Param;
			e["ResolutionScale"] = effect.resolutionScale;

            node["PostEffects"].push_back(e);
        }

        for (auto& link : postEffectLinks) {
            YAML::Node l;
            l["ID"] = link.id;
            l["StartNode"] = link.startNode;
            l["EndNode"] = link.endNode;
            l["StartAttr"] = link.startAttr;
            l["EndAttr"] = link.endAttr;
            node["PostEffectLinks"].push_back(l);
        }

        {
            YAML::Node e;
            e["Name"] = screenInputNode.name;
            e["NodePos"] = screenInputNode.nodePos;
            e["OutputPin"] = screenInputNode.outputPin;
            node["ScreenInput"] = e;
        }
        {
            YAML::Node e;
            e["Name"] = screenOutputNode.name;
            e["NodePos"] = screenOutputNode.nodePos;
            e["InputPins"] = screenOutputNode.inputPins;
            node["ScreenOutput"] = e;
        }

        return node;
    }

    bool decode(SceneContext* _context, const YAML::Node& node) override {
        context = _context;

		initialized = true;
		screenInputNode.initialized = true;
		screenOutputNode.initialized = true;

        if (node["isLock"]) isLock = node["isLock"].as<bool>();
        if (node["Target"]) Target = node["Target"].as<Vector3>();
        if (node["NearClip"]) NearClip = node["NearClip"].as<float>();
        if (node["FarClip"]) FarClip = node["FarClip"].as<float>();
        if (node["FOV"]) FOV = node["FOV"].as<float>();
        if (node["viewMatrix"]) viewMatrix = node["viewMatrix"].as<DirectX::XMMATRIX>();

        if (node["nextLinkId"]) nextLinkId = node["nextLinkId"].as<int>();
        if (node["nextPinId"]) nextPinId = node["nextPinId"].as<int>();


        if (node["PostEffects"]) {
			int idx = 2;
            for (auto eNode : node["PostEffects"]) {
                CameraPostEffect effect;
                effect.name = eNode["Name"].as<std::string>();
                effect.enabled = eNode["Enabled"].as<bool>();
                if (eNode["VS"]) effect.vs = context->manager->resource->Load<VertexShaderData>(eNode["VS"].as<std::string>());
                if (eNode["PS"]) effect.ps = context->manager->resource->Load<PixelShaderData>(eNode["PS"].as<std::string>());
				if(eNode["NodePos"]){
					effect.nodePos = eNode["NodePos"].as<Vector2>();
					ImNodes::SetNodeEditorSpacePos(idx, ImVec2(effect.nodePos.x, effect.nodePos.y));
				}
                if (eNode["InputPins"]) effect.inputPins = eNode["InputPins"].as<std::vector<int>>();
                if (effect.inputPins.empty()) effect.inputPins.push_back(nextPinId++);
				if(eNode["OutputPin"]) effect.outputPin = eNode["OutputPin"].as<int>();
				if(eNode["Param"]) effect.Param = eNode["Param"].as < DirectX::XMFLOAT4 > ();
				if(eNode["ResolutionScale"]) effect.resolutionScale = eNode["ResolutionScale"].as<float>();
				if (effect.outputPin <= 0) effect.outputPin = nextPinId++;
                effect.initialized = true;
                postEffects.push_back(effect);
				idx++;
            }
        }

        if (node["PostEffectLinks"]) {
            for (auto lNode : node["PostEffectLinks"]) {
                CameraPostEffectLink link;
                link.id = lNode["ID"].as<int>();
                link.startNode = lNode["StartNode"].as<int>();
                link.endNode = lNode["EndNode"].as<int>();
                link.startAttr = lNode["StartAttr"].as<int>();
                link.endAttr = lNode["EndAttr"].as<int>();
                postEffectLinks.push_back(link);
                if (link.id >= nextLinkId) nextLinkId = link.id + 1;
            }
        }

        if (node["ScreenInput"]) {
            auto e = node["ScreenInput"];
            screenInputNode.name = e["Name"].as<std::string>();
            if (e["NodePos"]) screenInputNode.nodePos = e["NodePos"].as<Vector2>();
            if (e["OutputPin"]) screenInputNode.outputPin = e["OutputPin"].as<int>();
            //if (screenInputNode.outputPin <= 0) screenInputNode.outputPin = nextPinId++;
			ImNodes::SetNodeEditorSpacePos(-1, ImVec2(screenInputNode.nodePos.x, screenInputNode.nodePos.y));

            screenInputNode.initialized = true;
        } else {
            screenInputNode.name = "";
            screenInputNode.outputPin = nextPinId++;
            screenInputNode.nodePos = Vector2(50, 50);
            screenInputNode.initialized = false;
        }

        if (node["ScreenOutput"]) {
            auto e = node["ScreenOutput"];
            screenOutputNode.name = e["Name"].as<std::string>();
            if (e["NodePos"]) screenOutputNode.nodePos = e["NodePos"].as<Vector2>();
            if (e["InputPins"]) screenOutputNode.inputPins = e["InputPins"].as<std::vector<int>>();
            //if (screenOutputNode.inputPins.empty()) screenOutputNode.inputPins.push_back(nextPinId++);
			ImNodes::SetNodeEditorSpacePos(-2, ImVec2(screenOutputNode.nodePos.x, screenOutputNode.nodePos.y));

            screenOutputNode.initialized = true;
        } else {
            screenOutputNode.name = "";
            screenOutputNode.inputPins.push_back(nextPinId++);
            screenOutputNode.nodePos = Vector2(500, 50);
            screenOutputNode.initialized = false;
        }

        return true;
    }

    void inspector(SceneContext* ctx) {

		if(!initialized){
			postEffectLinks.clear();
			postEffectLinks.push_back({nextLinkId++, -1, -2, -1, 1});
			initialized = true;
		}

        context = ctx;
        ImGui::PushID(this);

        ImGui::Text("NearClip"); ImGui::SameLine(100);
        ImGui::UndoDragFloat("##NearClip", &NearClip, 0.01f, 0.01f, FarClip - 0.01f);
        ImGui::Text("FarClip"); ImGui::SameLine(100);
        ImGui::UndoDragFloat("##FarClip", &FarClip, 0.01f, NearClip + 0.01f, 4096.0f);
        if (NearClip <= 0.0f) NearClip = 0.01f;
        if (FarClip <= NearClip) FarClip = NearClip + 0.01f;

        ImGui::Text("FOV"); ImGui::SameLine(100);
        ImGui::UndoDragFloat("##FOV", &FOV, 0.01f, 0.01f);
        if (FOV <= 0.0f) FOV = 0.01f;

        ImGui::Text("isLock"); ImGui::SameLine(100);
        if (ImGui::Button(isLock ? "On" : "Off")) isLock = !isLock;
        if (isLock) {
            ImGui::Text("Target Position"); ImGui::SameLine(100);
            ImGui::UndoDragFloat3("##Target", &Target.x, 0.01f);
        }

        ImGui::Separator();
        ImGui::Text("Post-Process Node Editor");
        ImGui::BeginChild("NodeEditorRegion", ImVec2(0, 400), true);

        ImNodes::PushStyleVar(ImNodesStyleVar_NodePadding, ImVec2(4, 4));
        ImNodes::PushStyleVar(ImNodesStyleVar_PinCircleRadius, 3);
        ImNodes::PushStyleVar(ImNodesStyleVar_GridSpacing, 16.0f);

        ImNodes::BeginNodeEditor();

        // ScreenInput
        {
            ImNodes::BeginNode(-1);
            ImNodes::BeginNodeTitleBar();
            ImGui::Text("%s", "ScreenInput");
            ImNodes::EndNodeTitleBar();
            ImNodes::BeginOutputAttribute(screenInputNode.outputPin);
            ImGui::Text("Output");
            ImNodes::EndOutputAttribute();
            ImNodes::EndNode();

            if (!screenInputNode.initialized) {
                screenInputNode.nodePos = Vector2(0.0f, 0.0f);
                ImNodes::SetNodeEditorSpacePos(-1, ImVec2(screenInputNode.nodePos.x, screenInputNode.nodePos.y));
                screenInputNode.initialized = true;
            }
            ImVec2 pos = ImNodes::GetNodeEditorSpacePos(-1);
            screenInputNode.nodePos = Vector2(pos.x, pos.y);
        }

        // ScreenOutput
        {
            ImNodes::BeginNode(-2);
            ImNodes::BeginNodeTitleBar();
            ImGui::Text("%s", "ScreenOutput");
            ImNodes::EndNodeTitleBar();
            if (screenOutputNode.inputPins.empty()) screenOutputNode.inputPins.push_back(nextPinId++);
            ImNodes::BeginInputAttribute(screenOutputNode.inputPins[0]);
            ImGui::Text("Input");
            ImNodes::EndInputAttribute();
            ImNodes::EndNode();

            if (!screenOutputNode.initialized) {
                screenOutputNode.nodePos = Vector2(100.0f, 0.0f);
                ImNodes::SetNodeEditorSpacePos(-2, ImVec2(screenOutputNode.nodePos.x, screenOutputNode.nodePos.y));
                screenOutputNode.initialized = true;
            }
            ImVec2 pos = ImNodes::GetNodeEditorSpacePos(-2);
            screenOutputNode.nodePos = Vector2(pos.x, pos.y);
        }

        // Effects
        int idx = 2;
        char filepathBuffer[512];
        float startX = 50.0f, startY = 150.0f, spacingX = 200.0f, spacingY = 120.0f;

        for (auto& effect : postEffects) {

            ImNodes::BeginNode(idx);

            if (!effect.initialized) {

                ImNodes::SetNodeEditorSpacePos(idx, ImVec2(0, 0));
                effect.initialized = true;
            } else {
                ImVec2 currentPos = ImNodes::GetNodeEditorSpacePos(idx);
                effect.nodePos = Vector2(currentPos.x, currentPos.y);
            }

            ImNodes::BeginNodeTitleBar();
            char nameBuffer[128];
            strncpy_s(nameBuffer, sizeof(nameBuffer), effect.name.c_str(), _TRUNCATE);
            ImGui::PushItemWidth(200.0f);
            if (ImGui::InputText(("##NodeName" + std::to_string(idx)).c_str(), nameBuffer, sizeof(nameBuffer)))
                effect.name = nameBuffer;
            ImNodes::EndNodeTitleBar();

            ImGui::UndoCheckbox("Enabled", &effect.enabled);

			ImGui::UndoDragFloat4("##Param", &effect.Param.x, 0.01f);
			ImGui::UndoDragFloat("Scale", &effect.resolutionScale, 0.01f, 0.1f, 1.0f);

            if (effect.ps) strncpy_s(filepathBuffer, sizeof(filepathBuffer), effect.ps->FilePath.c_str(), _TRUNCATE);
            else filepathBuffer[0] = '\0';
            ImGui::PushItemWidth(150.0f);
            if (ImGui::InputText("PS", filepathBuffer, sizeof(filepathBuffer)) && context)
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
            ImGui::PopItemWidth();

            if (effect.vs) strncpy_s(filepathBuffer, sizeof(filepathBuffer), effect.vs->FilePath.c_str(), _TRUNCATE);
            else filepathBuffer[0] = '\0';
            ImGui::PushItemWidth(150.0f);
            if (ImGui::InputText("VS", filepathBuffer, sizeof(filepathBuffer)) && context)
                effect.vs = context->manager->resource->Load<VertexShaderData>(filepathBuffer);
			// Drag & Drop(VS)
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
            ImGui::PopItemWidth();

			if(effect.srv && effect.enabled){
				// サイズは適宜調整
				float availWidth = ImGui::GetContentRegionAvail().x;
				availWidth = 150.0f;
				ImGui::Image(
					(ImTextureID)effect.srv.Get(),
					ImVec2(availWidth, availWidth / 16.0f * 9.0f)  // 表示サイズ
				);
			}

            // Input Pins
            int inputnum = 0;
			bool isFirst = true;
            for (int pinId : effect.inputPins) {
                ImNodes::BeginInputAttribute(pinId);
                ImGui::Text(("Input[" + std::to_string(inputnum) + "]").c_str());
                ImNodes::EndInputAttribute();
                inputnum++;

				if(isFirst){
					ImGui::SameLine(150.0f);
					ImNodes::BeginOutputAttribute(effect.outputPin);
					ImGui::Text("Output");
					ImNodes::EndOutputAttribute();
					isFirst = false;
				}
            }

            // 未接続なら追加
            bool allConnected = true;
            for (int pinId : effect.inputPins) {
                auto it = std::find_if(postEffectLinks.begin(), postEffectLinks.end(),
                                       [&](const CameraPostEffectLink& link) { return link.endAttr == pinId; });
                if (it == postEffectLinks.end()) { allConnected = false; break; }
            }
            if (allConnected) effect.inputPins.push_back(nextPinId++);


            ImNodes::EndNode();
            idx++;
        }

		// Links
		for(auto it = postEffectLinks.begin(); it != postEffectLinks.end();){
			ImNodes::Link(it->id, it->startAttr, it->endAttr);
			if(ImNodes::IsLinkSelected(it->id) && ImGui::GetMouseClickedCount(0) > 1)
				it = postEffectLinks.erase(it);
			else ++it;
		}

        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomRight);
        ImNodes::EndNodeEditor();

		// --- ノード右クリックメニュー ---
		static int hoveredNode = -1000;


		if(ImGui::BeginPopup("NodeContextMenu")){
			if(hoveredNode >= 2){ // postEffects のみ削除可能 (-1, -2 は特別ノード)
				if(ImGui::MenuItem("Delete Node")){
					int effectIndex = hoveredNode - 2;

					if(effectIndex >= 0 && effectIndex < (int)postEffects.size()){
						// --- 関連リンク削除 ---
						postEffectLinks.erase(
							std::remove_if(postEffectLinks.begin(), postEffectLinks.end(),
										   [&](const CameraPostEffectLink& link){
											   return (link.startNode == effectIndex || link.endNode == effectIndex);
										   }),
							postEffectLinks.end()
						);

						// --- ノード削除 ---
						postEffects.erase(postEffects.begin() + effectIndex);

						// --- ID補正 ---
						for(auto& link : postEffectLinks){
							if(link.startNode > effectIndex) link.startNode--;
							if(link.endNode > effectIndex) link.endNode--;
						}
					}
				}
			} else{
				ImGui::TextDisabled("Cannot delete this node");
			}
			ImGui::EndPopup();
		} else{
			if(ImNodes::IsNodeHovered(&hoveredNode)){
				if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)){
					ImGui::OpenPopup("NodeContextMenu");
				}
			}
		}

        int startAttr, endAttr;
        if (ImNodes::IsLinkCreated(&startAttr, &endAttr)) {
            CameraPostEffectLink newLink;
            newLink.id = nextLinkId++;
            newLink.startAttr = startAttr;
            newLink.endAttr = endAttr;

            newLink.startNode = -1;
            newLink.endNode = -1;
            if (startAttr == screenInputNode.outputPin) newLink.startNode = -1;
            if (std::find(screenOutputNode.inputPins.begin(), screenOutputNode.inputPins.end(), endAttr) != screenOutputNode.inputPins.end())
                newLink.endNode = -2;

            for (int i = 0; i < (int)postEffects.size(); i++) {
                CameraPostEffect& effect = postEffects[i];
                if (effect.outputPin == startAttr) newLink.startNode = i;
                if (std::find(effect.inputPins.begin(), effect.inputPins.end(), endAttr) != effect.inputPins.end()) newLink.endNode = i;
            }
            postEffectLinks.push_back(newLink);
        }

        ImNodes::PopStyleVar(3);
        ImGui::EndChild();

        if (ImGui::Button("Add Node") && context) {
            CameraPostEffect newEffect;
            newEffect.name = "NewEffect";
            newEffect.enabled = true;
            newEffect.initialized = false;
            newEffect.inputPins.push_back(nextPinId++);
            newEffect.outputPin = nextPinId++;

			newEffect.vs = context->manager->resource->Load<VertexShaderData>(DEFAULT_WINDOW_POSTEFFECT_VS_PATH);
			newEffect.ps = context->manager->resource->Load<PixelShaderData>(DEFAULT_WINDOW_POSTEFFECT_PS_PATH);

            postEffects.push_back(newEffect);
        }

        ImGui::PopID();
    }

    bool isLock = false;
    Vector3 Target{ 0,0,0 };
    float NearClip = 0.01f;
    float FarClip = 1024.0f;
    float FOV = 1.0f;
    DirectX::XMMATRIX viewMatrix{};


    std::vector<int> TopologicalSortPostEffects() {
        std::vector<int> sortedIndices;

        int n = static_cast<int>(postEffects.size());
        int INPUT_NODE = n;     // -1 の代わり
        int OUTPUT_NODE = n + 1;   // -2 の代わり

        std::unordered_map<int, std::vector<int>> adj;
        std::unordered_map<int, int> indegree;
        std::unordered_set<int> nodes;

        for (const auto& link : postEffectLinks) {
            int start = link.startNode;
            int end = link.endNode;

            if (start == -1) start = INPUT_NODE;
            if (end == -2) end = OUTPUT_NODE;

            if (start >= 0 && start <= OUTPUT_NODE && end >= 0 && end <= OUTPUT_NODE) {
                adj[start].push_back(end);
                indegree[end]++;
                nodes.insert(start);
                nodes.insert(end);
            }
        }

        std::queue<int> q;
        for (int node : nodes) {
            if (indegree.find(node) == indegree.end()) q.push(node);
        }

        while (!q.empty()) {
            int node = q.front(); q.pop();
            sortedIndices.push_back(node);

            for (int neighbor : adj[node]) {
                indegree[neighbor]--;
                if (indegree[neighbor] == 0) q.push(neighbor);
            }
        }

        // サイクル検出
        if (sortedIndices.size() != nodes.size()) {
            OutputDebugStringA("Warning: post effect graph has cycles!\n");
            for (int node : nodes) {
                if (std::find(sortedIndices.begin(), sortedIndices.end(), node) == sortedIndices.end()) {
                    sortedIndices.push_back(node);
                }
            }
        }

        // 仮IDを元に戻す
        for (auto& idx : sortedIndices) {
            if (idx == INPUT_NODE) idx = -1;
            if (idx == OUTPUT_NODE) idx = -2;
        }
        return sortedIndices;
    }

};
