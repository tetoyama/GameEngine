// =======================================================================
// 
// RenderableWave.cpp
// 
// =======================================================================
#include "RenderableWave.h"

#include <d3d11.h>
#include <vector>
#include "../../RenderPass/RenderPassContext.h"

#include "DebugTools/DebugSystem.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/componentRegistry.h"

#include "Scene/Component/waveComponent.h"
#include "Scene/Component/meshRendererComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/textureComponent.h"
#include <Component/materialComponent.h>
#include "Shader/commonDefine.h"

namespace {
	constexpr float kTessellationMin = 2.0f;
	constexpr float kTessellationMax = 12.0f;
	constexpr float kTessellationMinDistance = 2.0f;
	constexpr float kTessellationMaxDistance = 35.0f;
	constexpr DirectX::XMFLOAT2 kFlowDir1 = DirectX::XMFLOAT2(0.96f, 0.28f);
	constexpr DirectX::XMFLOAT2 kFlowDir2 = DirectX::XMFLOAT2(-0.42f, 0.91f);
	constexpr float kFlowSpeedLayer2Rate = 0.63f;
	constexpr float kNormalDelta = 0.08f;
	constexpr float kFresnelPower = 5.0f;
	constexpr float kMinWaveLength = 0.01f;

	struct alignas(16) WaterTessellationCB {
		DirectX::XMFLOAT4X4 World;
		DirectX::XMFLOAT4X4 View;
		DirectX::XMFLOAT4X4 Projection;
		DirectX::XMFLOAT3 CameraPos;
		float Time;
		float TessellationMin;
		float TessellationMax;
		float TessellationMinDistance;
		float TessellationMaxDistance;
		float WaveHeight;
		float WaveScale;
		DirectX::XMFLOAT2 FlowDir1;
		DirectX::XMFLOAT2 FlowDir2;
		float FlowSpeed1;
		float FlowSpeed2;
		float NormalDelta;
		float FresnelPower;
	};

	struct WaterShaderCache {
		Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
		Microsoft::WRL::ComPtr<ID3D11HullShader> hs;
		Microsoft::WRL::ComPtr<ID3D11DomainShader> ds;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Buffer> waterCB;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
		bool initialized = false;
	};

	bool EnsureWaterShaderResources(GraphicsContext* graphicsContext, WaterShaderCache& cache){
		if (cache.initialized) {
			return true;
		}

		if (!graphicsContext->CreateVertexShader("Asset\\Shader\\WaterTessVS.cso", cache.vs.GetAddressOf(), cache.inputLayout.GetAddressOf())) {
			return false;
		}
		if (!graphicsContext->CreateHullShader("Asset\\Shader\\WaterTessHS.cso", cache.hs.GetAddressOf())) {
			return false;
		}
		if (!graphicsContext->CreateDomainShader("Asset\\Shader\\WaterTessDS.cso", cache.ds.GetAddressOf())) {
			return false;
		}
		if (!graphicsContext->CreatePixelShader("Asset\\Shader\\WaterTessPS.cso", cache.ps.GetAddressOf())) {
			return false;
		}

		ID3D11Device* device = graphicsContext->GetDevice();

		D3D11_BUFFER_DESC cbDesc{};
		cbDesc.Usage = D3D11_USAGE_DEFAULT;
		cbDesc.ByteWidth = sizeof(WaterTessellationCB);
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (FAILED(device->CreateBuffer(&cbDesc, nullptr, cache.waterCB.GetAddressOf()))) {
			return false;
		}

		D3D11_SAMPLER_DESC samplerDesc{};
		samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MaxAnisotropy = 4;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		if (FAILED(device->CreateSamplerState(&samplerDesc, cache.sampler.GetAddressOf()))) {
			return false;
		}

		cache.initialized = true;
		return true;
	}

	bool EnsurePatchIndexBuffer(WaveComponent* wave, GraphicsContext* graphicsContext){
		if (!wave) {
			return false;
		}
		if (wave->PatchIndexBuffer && wave->PatchResolution == wave->Resolution) {
			return true;
		}

		const int grid = wave->Resolution;
		if (grid <= 0) {
			return false;
		}

		std::vector<unsigned int> patchIndices;
		patchIndices.reserve(static_cast<size_t>(grid) * static_cast<size_t>(grid) * 4);

		for (int z = 0; z < grid; ++z) {
			for (int x = 0; x < grid; ++x) {
				const unsigned int topLeft = static_cast<unsigned int>(z * (grid + 1) + x);
				const unsigned int topRight = topLeft + 1;
				const unsigned int bottomLeft = static_cast<unsigned int>((z + 1) * (grid + 1) + x);
				const unsigned int bottomRight = bottomLeft + 1;
				patchIndices.push_back(topLeft);
				patchIndices.push_back(topRight);
				patchIndices.push_back(bottomLeft);
				patchIndices.push_back(bottomRight);
			}
		}

		D3D11_BUFFER_DESC ibDesc{};
		ibDesc.Usage = D3D11_USAGE_DEFAULT;
		ibDesc.ByteWidth = static_cast<UINT>(sizeof(unsigned int) * patchIndices.size());
		ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		D3D11_SUBRESOURCE_DATA ibData{};
		ibData.pSysMem = patchIndices.data();

		wave->PatchIndexBuffer.Reset();
		if (FAILED(graphicsContext->GetDevice()->CreateBuffer(&ibDesc, &ibData, wave->PatchIndexBuffer.GetAddressOf()))) {
			return false;
		}
		wave->PatchResolution = wave->Resolution;
		wave->PatchIndexCount = static_cast<int>(patchIndices.size());
		return true;
	}
}

void RenderableWave::Execute(const RenderPassContext& ctx, SceneContext* sceneContext, const Entity& entity) {
	auto componentRegistry = sceneContext->component;
	auto pTransform = componentRegistry->GetComponent<TransformComponent>(entity);
	auto pWave = componentRegistry->GetComponent<WaveComponent>(entity);

	if (!pTransform || !pWave || !pWave->meshRenderer) {
		return;
	}
	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	static WaterShaderCache shaderCache;
	if (!EnsureWaterShaderResources(graphicsContext, shaderCache)) {
		return;
	}
	if (!EnsurePatchIndexBuffer(pWave, graphicsContext)) {
		return;
	}

	TextureComponent* pTexture = sceneContext->component->GetComponent<TextureComponent>(entity);
	MaterialComponent* pMaterial = sceneContext->component->GetComponent<MaterialComponent>(entity);
	MATERIAL material{};
	if (pMaterial) {
		material = pMaterial->Material;
		material.MaterialFlags &= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
	}

	if(pTexture && pTexture->m_TextureData && pTexture->m_TextureData->pTexture){
		deviceContext->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());

		// sceneContext->manager->graphics->GetDeviceContext()->PSSetShaderResources(TextureSlot_Albedo, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
		material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;

	}

	sceneContext->manager->graphics->SetMaterial(material);

	auto meshRenderer = pWave->meshRenderer;
	auto transform = pTransform;


	DirectX::XMMATRIX World = transform->CalculateWorldMatrix(transform, componentRegistry);

	graphicsContext->SetCullMode(CullMode::Back);
	graphicsContext->SetWorldMatrix(World);
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

	deviceContext->IASetVertexBuffers(0, 1, meshRenderer->mesh.m_VertexBuffer.GetAddressOf(), &stride, &offset);
	deviceContext->IASetIndexBuffer(pWave->PatchIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	deviceContext->IASetInputLayout(shaderCache.inputLayout.Get());

	deviceContext->VSSetShader(shaderCache.vs.Get(), nullptr, 0);
	deviceContext->HSSetShader(shaderCache.hs.Get(), nullptr, 0);
	deviceContext->DSSetShader(shaderCache.ds.Get(), nullptr, 0);
	deviceContext->PSSetShader(shaderCache.ps.Get(), nullptr, 0);

	WaterTessellationCB waterCB{};
	DirectX::XMStoreFloat4x4(&waterCB.World, World);
	DirectX::XMStoreFloat4x4(&waterCB.View, ctx.viewMatrix);
	DirectX::XMStoreFloat4x4(&waterCB.Projection, ctx.projectionMatrix);
	waterCB.CameraPos = DirectX::XMFLOAT3(ctx.CameraPosition.x, ctx.CameraPosition.y, ctx.CameraPosition.z);
	waterCB.Time = pWave->Time;
	waterCB.TessellationMin = kTessellationMin;
	waterCB.TessellationMax = kTessellationMax;
	waterCB.TessellationMinDistance = kTessellationMinDistance;
	waterCB.TessellationMaxDistance = kTessellationMaxDistance;
	waterCB.WaveHeight = pWave->Amplitude;
	const float waveLength = (pWave->Wavelength > kMinWaveLength) ? pWave->Wavelength : kMinWaveLength;
	waterCB.WaveScale = 1.0f / waveLength;
	waterCB.FlowDir1 = kFlowDir1;
	waterCB.FlowDir2 = kFlowDir2;
	waterCB.FlowSpeed1 = pWave->Speed;
	waterCB.FlowSpeed2 = pWave->Speed * kFlowSpeedLayer2Rate;
	waterCB.NormalDelta = kNormalDelta;
	waterCB.FresnelPower = kFresnelPower;

	deviceContext->UpdateSubresource(shaderCache.waterCB.Get(), 0, nullptr, &waterCB, 0, 0);
	ID3D11Buffer* waveConstantBuffer = shaderCache.waterCB.Get();
	deviceContext->HSSetConstantBuffers(3, 1, &waveConstantBuffer);
	deviceContext->DSSetConstantBuffers(3, 1, &waveConstantBuffer);
	deviceContext->PSSetConstantBuffers(3, 1, &waveConstantBuffer);

	ID3D11SamplerState* waveSampler = shaderCache.sampler.Get();
	deviceContext->DSSetSamplers(0, 1, &waveSampler);
	deviceContext->PSSetSamplers(0, 1, &waveSampler);

	if (pTexture && pTexture->m_TextureData && pTexture->m_TextureData->pTexture) {
		// HLSL の HeightTexture : register(t5) と TextureSlot_HeightMap(=5) を対応させる
		deviceContext->DSSetShaderResources(TextureSlot_HeightMap, 1, pTexture->m_TextureData->pTexture.GetAddressOf());
	}

	deviceContext->DrawIndexed(pWave->PatchIndexCount, 0, 0);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	deviceContext->DSSetShaderResources(TextureSlot_HeightMap, 1, &nullSRV);

	deviceContext->HSSetShader(nullptr, nullptr, 0);
	deviceContext->DSSetShader(nullptr, nullptr, 0);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//graphicsContext->SetDepthMode(DepthMode::Write);
	//graphicsContext->SetViewMatrix(ctx.viewMatrix);
	//graphicsContext->SetProjectionMatrix(ctx.projectionMatrix);
}
