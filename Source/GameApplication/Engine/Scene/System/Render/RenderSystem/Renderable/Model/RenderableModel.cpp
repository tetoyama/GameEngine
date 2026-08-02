// =======================================================================
// 
// RenderableModel.cpp
// 
// =======================================================================
#include "RenderableModel.h"

#include <d3d11.h>
#include "../../RenderPass/RenderPassContext.h"
#include "../../RenderPacket/RenderPacketTransformDX11.h"

#include "DebugTools/DebugSystem.h"

#include "Scene/scene.h"
#include "Scene/sceneManager.h"
#include "Scene/Registry/systemRegistry.h"

#include "Scene/Component/modelRendererComponent.h"
#include "Scene/Component/transformComponent.h"
#include "Scene/Component/textureComponent.h"
#include "System/Render/Model/ModelMaterialLegacyD3D11Bridge.h"
#include "System/Render/RenderSystem/renderSystem.h"
#include "Service/Graphics/RHI/RHIService.h"
#include "Service/Graphics/RHI/D3D11/D3D11RHIDevice.h"

#include "Backends/Assimp/scene.h"
#include <Component/materialComponent.h>

namespace {

CullMode ResolveLegacyCullMode(
	const MaterialDescriptor* descriptor
) noexcept {
	if(!descriptor) return CullMode::Back;
	switch(descriptor->renderState.cullMode){
		case MaterialCullMode::None:
			return CullMode::None;
		case MaterialCullMode::Front:
			return CullMode::Front;
		case MaterialCullMode::Back:
		default:
			return CullMode::Back;
	}
}

void BindTexture(
	ID3D11DeviceContext& context,
	UINT slot,
	ID3D11ShaderResourceView* texture
) noexcept {
	if(!texture) return;
	context.PSSetShaderResources(slot, 1, &texture);
}

} // namespace

void RenderableModel::Execute(
	const RenderPassContext& ctx,
	const RenderPacket& packet){
	SceneContext* sceneContext = packet.bindings.sceneContext;
	if(!sceneContext) return;

	ModelRendererComponent* modelRenderer = packet.bindings.modelRenderer;
	TransformComponent* transform = packet.bindings.transform;
	if(!modelRenderer || !transform){
		return;
	}

	ModelData* model = packet.modelResource
		? packet.modelResource.get()
		: modelRenderer->model.get();
	if(!model || !model->AiScene){
		return;
	}

	GraphicsContext* graphicsContext = sceneContext->manager->graphics;
	if(!graphicsContext) return;
	ID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();
	if(!deviceContext) return;

	const ModelRendererGpuRuntime* modelGpuRuntime = nullptr;
	const ModelGeometryRuntime* modelGeometryRuntime = nullptr;
	if(sceneContext->manager && sceneContext->manager->systemRegistry){
		if(RenderSystem* renderSystem =
			sceneContext->manager->systemRegistry->GetSystem<RenderSystem>()){
			ModelRendererGpuRuntimeKey runtimeKey;
			runtimeKey.sceneContextID = packet.sceneContextID;
			runtimeKey.entity = packet.entity.GetPackedValue();
			modelGpuRuntime =
				renderSystem->GetModelRendererGpuRuntime().Find(runtimeKey);
			modelGeometryRuntime =
				renderSystem->GetModelGeometryRuntime().Find(model);
		}
	}

	RHI::D3D11RHIDevice* d3d11RhiDevice = nullptr;
	if(RHI::RenderHardwareInterfaceService* service =
		graphicsContext->GetRHIService()){
		d3d11RhiDevice = dynamic_cast<RHI::D3D11RHIDevice*>(
			service->GetDevice()
		);
	}

	TextureComponent* textureComponent = packet.bindings.texture;
	MaterialComponent* materialComponent = packet.bindings.material;
	const MaterialDescriptor* resolvedDescriptor =
		packet.modelMaterial.GetDescriptor();

	//----------------------------------------------------------------------
	// Resolved Material -> Legacy D3D11 Bridge
	//----------------------------------------------------------------------
	MATERIAL material{};
	material.BaseColor = {1.0f, 1.0f, 1.0f, 1.0f};
	ModelMaterialLegacyD3D11Binding resolvedBinding;
	if(resolvedDescriptor){
		resolvedBinding = ModelMaterialLegacyD3D11Bridge::Resolve(
			*model,
			*resolvedDescriptor
		);
		material = resolvedBinding.material;
	}else if(materialComponent){
		// Packet Snapshot未生成の移行経路だけ旧単一Materialを使用する。
		material = materialComponent->Material;
		material.MaterialFlags &= MATERIAL_FLAG_USE_ENVIRONMENT_MAP;
	}

	//----------------------------------------------------------------------
	// Texture / UV Animation
	//----------------------------------------------------------------------
	UVMatrixBuffer uv{};
	uv.UVStart = float2(0.0f, 0.0f);
	uv.UVEnd = float2(1.0f, 1.0f);

	const bool hasOverrideTexture = textureComponent &&
		textureComponent->m_TextureData;
	if(textureComponent){
		uv = textureComponent->ResolveUVMatrixBuffer();
	}

	if(hasOverrideTexture){
		if(textureComponent->m_TextureData->pTexture){
			material.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;
			BindTexture(
				*deviceContext,
				TextureSlot_Albedo,
				textureComponent->m_TextureData->pTexture.Get()
			);
		}
	}else if(resolvedDescriptor){
		// Shadow PassでもBaseColor Alpha Cutoutへ必要なAlbedoだけはBindする。
		BindTexture(
			*deviceContext,
			TextureSlot_Albedo,
			resolvedBinding.baseColor.texture
		);
		if(ctx.passPhase != RenderPhase::PHASE_SHADOW){
			BindTexture(
				*deviceContext,
				TextureSlot_Normal,
				resolvedBinding.normal.texture
			);
			BindTexture(
				*deviceContext,
				TextureSlot_Roughness,
				resolvedBinding.roughness.texture
			);
			BindTexture(
				*deviceContext,
				TextureSlot_Metallic,
				resolvedBinding.metallic.texture
			);
			BindTexture(
				*deviceContext,
				TextureSlot_AO,
				resolvedBinding.ambientOcclusion.texture
			);
			BindTexture(
				*deviceContext,
				TextureSlot_HeightMap,
				resolvedBinding.height.texture
			);
			BindTexture(
				*deviceContext,
				TextureSlot_EmissiveMap,
				resolvedBinding.emissive.texture
			);
		}
	}

	//----------------------------------------------------------------------
	// Render State / Per-object Constants
	//----------------------------------------------------------------------
	deviceContext->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	);

	// ShadowMapPass owns the complete rasterizer-state contract for the
	// current light type. Replacing it here would discard Point/Spot depth
	// clipping and any shadow-specific depth-bias settings.
	if(ctx.passPhase != RenderPhase::PHASE_SHADOW){
		graphicsContext->SetCullMode(
			ResolveLegacyCullMode(resolvedDescriptor)
		);
	}

	const DirectX::XMMATRIX world =
		LoadRenderPacketMatrix(packet.transform.worldMatrix);
	graphicsContext->SetPerObjectConstants(world, material, uv);

	//----------------------------------------------------------------------
	// Draw Mesh
	//----------------------------------------------------------------------
	UINT firstMeshIndex = 0;
	UINT meshEndIndex = model->AiScene->mNumMeshes;
	if(!packet.TargetsAllSubMeshes()){
		if(packet.subMeshIndex >= model->AiScene->mNumMeshes){
			return;
		}
		firstMeshIndex = packet.subMeshIndex;
		meshEndIndex = firstMeshIndex + 1;
	}

	for(UINT meshIndex = firstMeshIndex;
		meshIndex < meshEndIndex;
		++meshIndex){
		UINT stride = sizeof(VERTEX_3D);
		UINT offset = 0;

		const ModelGeometryRuntimeMesh* sharedGeometry =
			modelGeometryRuntime ? modelGeometryRuntime->Mesh(meshIndex) : nullptr;
		ID3D11Buffer* sharedVertexBuffer =
			d3d11RhiDevice && sharedGeometry
				? d3d11RhiDevice->NativeBuffer(sharedGeometry->vertexBuffer)
				: nullptr;
		ID3D11Buffer* sharedIndexBuffer =
			d3d11RhiDevice && sharedGeometry
				? d3d11RhiDevice->NativeBuffer(sharedGeometry->indexBuffer)
				: nullptr;

		const bool hasDynamicVertexBuffer =
			!modelRenderer->blendedAnimations.empty() &&
			modelGpuRuntime &&
			modelGpuRuntime->ModelRevision() ==
				modelRenderer->modelRuntimeRevision &&
			modelGpuRuntime->Buffer(meshIndex) != nullptr;
		const bool hasLegacyStaticVertexBuffer =
			meshIndex < model->VertexBuffer.size() &&
			model->VertexBuffer[meshIndex] != nullptr;
		const bool hasLegacyIndexBuffer =
			meshIndex < model->IndexBuffer.size() &&
			model->IndexBuffer[meshIndex] != nullptr;
		const bool hasStaticVertexBuffer =
			sharedVertexBuffer != nullptr || hasLegacyStaticVertexBuffer;
		const bool hasIndexBuffer =
			sharedIndexBuffer != nullptr || hasLegacyIndexBuffer;

		if(!hasIndexBuffer || (!hasDynamicVertexBuffer && !hasStaticVertexBuffer)){
			continue;
		}

		ID3D11Buffer* vertexBuffer = hasDynamicVertexBuffer
			? modelGpuRuntime->Buffer(meshIndex)
			: (sharedVertexBuffer
				? sharedVertexBuffer
				: model->VertexBuffer[meshIndex]);
		ID3D11Buffer* indexBuffer = sharedIndexBuffer
			? sharedIndexBuffer
			: model->IndexBuffer[meshIndex];
		deviceContext->IASetVertexBuffers(
			0,
			1,
			&vertexBuffer,
			&stride,
			&offset
		);

		deviceContext->IASetIndexBuffer(
			indexBuffer,
			DXGI_FORMAT_R32_UINT,
			0
		);
		const UINT indexCount = sharedGeometry && sharedIndexBuffer
			? sharedGeometry->indexCount
			: model->AiScene->mMeshes[meshIndex]->mNumFaces * 3;
		deviceContext->DrawIndexed(
			indexCount,
			0,
			0
		);
	}
}