#pragma once
#include <array>
#include <cstring>
#include <vector>
#include <DirectXMath.h>
#include "Graphics/graphicsContext.h"
#include "Resources/Data/modelData.h"

namespace AnimationSkinningUpload {
inline bool DispatchGPU(
	GraphicsContext& graphics,
	ModelData& model,
	const std::vector<BONE>& bones,
	const std::vector<ID3D11Buffer*>& dynamicBuffers
){
	ID3D11DeviceContext* context=graphics.GetDeviceContext();
	if(!context || !model.AiScene || !model.m_BoneCB || !model.m_InfoCB || bones.empty()) return false;

	std::array<DirectX::XMMATRIX,BONE_MAX_COUNT> palette;
	for(auto& matrix:palette) matrix=DirectX::XMMatrixIdentity();
	for(size_t i=0;i<bones.size() && i<palette.size();++i){
		const aiMatrix4x4& source=bones[i].Matrix;
		palette[i]=DirectX::XMMATRIX(
			source.a1,source.a2,source.a3,source.a4,
			source.b1,source.b2,source.b3,source.b4,
			source.c1,source.c2,source.c3,source.c4,
			source.d1,source.d2,source.d3,source.d4
		);
	}

	D3D11_MAPPED_SUBRESOURCE mappedBones{};
	if(FAILED(context->Map(model.m_BoneCB,0,D3D11_MAP_WRITE_DISCARD,0,&mappedBones))) return false;
	std::memcpy(mappedBones.pData,palette.data(),sizeof(palette));
	context->Unmap(model.m_BoneCB,0);

	for(uint32_t meshIndex=0;meshIndex<model.AiScene->mNumMeshes;++meshIndex){
		if(meshIndex>=dynamicBuffers.size() || meshIndex>=model.m_SkinInputSRV.size() ||
			meshIndex>=model.m_SkinOutputUAV.size() || meshIndex>=model.m_SkinOutputUAVBuffer.size() ||
			!dynamicBuffers[meshIndex] || !model.m_SkinInputSRV[meshIndex] ||
			!model.m_SkinOutputUAV[meshIndex] || !model.m_SkinOutputUAVBuffer[meshIndex]){
			return false;
		}

		const uint32_t vertexCount=model.AiScene->mMeshes[meshIndex]->mNumVertices;
		D3D11_MAPPED_SUBRESOURCE mappedInfo{};
		if(FAILED(context->Map(model.m_InfoCB,0,D3D11_MAP_WRITE_DISCARD,0,&mappedInfo))) return false;
		reinterpret_cast<uint32_t*>(mappedInfo.pData)[0]=vertexCount;
		context->Unmap(model.m_InfoCB,0);

		context->CSSetShaderResources(0,1,&model.m_SkinInputSRV[meshIndex]);
		context->CSSetUnorderedAccessViews(0,1,&model.m_SkinOutputUAV[meshIndex],nullptr);
		context->CSSetConstantBuffers(0,1,&model.m_BoneCB);
		context->CSSetConstantBuffers(1,1,&model.m_InfoCB);
		context->CSSetShader(graphics.GetSkinningShader(),nullptr,0);
		context->Dispatch((vertexCount+63)/64,1,1);

		ID3D11UnorderedAccessView* nullUAV=nullptr;
		ID3D11ShaderResourceView* nullSRV=nullptr;
		context->CSSetUnorderedAccessViews(0,1,&nullUAV,nullptr);
		context->CSSetShaderResources(0,1,&nullSRV);
		context->CSSetShader(nullptr,nullptr,0);
		context->CopyResource(dynamicBuffers[meshIndex],model.m_SkinOutputUAVBuffer[meshIndex]);
	}
	return true;
}

inline bool UploadCPU(
	ID3D11DeviceContext& context,
	const std::vector<std::vector<VERTEX_3D>>& meshes,
	const std::vector<ID3D11Buffer*>& dynamicBuffers
){
	if(meshes.empty() || meshes.size()>dynamicBuffers.size()) return false;
	for(size_t meshIndex=0;meshIndex<meshes.size();++meshIndex){
		if(meshes[meshIndex].empty()) continue;
		ID3D11Buffer* destination=dynamicBuffers[meshIndex];
		if(!destination) return false;
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if(FAILED(context.Map(destination,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))) return false;
		std::memcpy(mapped.pData,meshes[meshIndex].data(),meshes[meshIndex].size()*sizeof(VERTEX_3D));
		context.Unmap(destination,0);
	}
	return true;
}
}
