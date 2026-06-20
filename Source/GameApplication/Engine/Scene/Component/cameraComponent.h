#pragma once

#include <memory>
#include <string>
#include <vector>

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include "Interface/IComponent.h"
#include "Backends/myVector2.h"
#include "Backends/myVector3.h"
#include "Resources/Data/pixelShaderData.h"
#include "Resources/Data/vertexShaderData.h"

struct CameraPostEffect {
	std::shared_ptr<PixelShaderData> ps;
	std::shared_ptr<VertexShaderData> vs;
	std::string name;
	bool enabled = true;
	Vector2 nodePos{-1.0f, -1.0f};
	DirectX::XMFLOAT4 Param{0.0f, 0.0f, 0.0f, 0.0f};
	bool initialized = false;
	std::vector<int> inputPins;
	int outputPin = -1;
	float resolutionScale = 1.0f;
	int mipLevels = 1;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	Vector2 resolution{1280.0f, 720.0f};

	void CreateTexture(ID3D11Device* device, const Vector2& screenSize);
	void ResizeTexture(ID3D11Device* device, const Vector2& screenSize);
	void Clear(ID3D11DeviceContext* context, const float clearColor[4]);
};

struct CameraPostEffectLink {
	int id = -1;
	int startNode = -1;
	int endNode = -1;
	int startAttr = -1;
	int endAttr = -1;
};

class CameraComponent: public IComponent {
public:
	SceneContext* context = nullptr;
	std::vector<CameraPostEffect> postEffects;
	std::vector<CameraPostEffectLink> postEffectLinks;
	CameraPostEffect screenInputNode;
	CameraPostEffect screenOutputNode;
	int nextLinkId = 1;
	int nextPinId = 1;
	bool initialized = false;
	bool postEffectGraphDirty = true;
	std::vector<int> cachedSortedPostEffectIndices;
	std::vector<std::vector<int>> cachedResolvedPostEffectInputs;
	bool isLock = false;
	Vector3 Target{0.0f, 0.0f, 0.0f};
	float NearClip = 0.01f;
	float FarClip = 1024.0f;
	float FOV = 1.0f;
	DirectX::XMMATRIX viewMatrix{};

	YAML::Node encode() override;
	bool decode(SceneContext* context, const YAML::Node& node) override;
	void inspector(SceneContext* context) override;
	void InvalidatePostEffectGraphCache();
	void RebuildPostEffectGraphCache();
	const std::vector<int>& TopologicalSortPostEffects();
	const std::vector<int>& GetResolvedPostEffectInputs(int effectIndex);
};

#include "Operations/CameraPostEffectRuntime.h"
#include "Operations/CameraComponentSerialization.h"
#include "Operations/CameraPostEffectGraph.h"
#include "Operations/CameraComponentInspector.h"
