#pragma once

#include "D3D11RHIConversions.h"

namespace RHI {

inline BufferHandle D3D11RHIDevice::CreateBuffer(
	const BufferDesc& desc,
	std::span<const std::byte> initialData
){
	if(!m_device || desc.byteSize == 0) return {};
	if(desc.usage == ResourceUsage::Immutable && initialData.empty()) return {};
	if(initialData.size() > desc.byteSize) return {};

	D3D11_BUFFER_DESC nativeDesc{};
	nativeDesc.ByteWidth = desc.byteSize;
	if(HasAnyFlag(desc.bindFlags, BufferBindFlags::Constant)){
		nativeDesc.ByteWidth = (nativeDesc.ByteWidth + 15u) & ~15u;
	}
	nativeDesc.Usage = D3D11Detail::ToUsage(desc.usage);
	nativeDesc.BindFlags = D3D11Detail::ToBufferBind(desc.bindFlags);
	nativeDesc.CPUAccessFlags = D3D11Detail::ToCpuAccess(desc.cpuAccess);

	if(desc.structured){
		if(desc.stride == 0) return {};
		nativeDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		nativeDesc.StructureByteStride = desc.stride;
	}
	if(desc.raw){
		nativeDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	}
	if(HasAnyFlag(desc.bindFlags, BufferBindFlags::IndirectArguments)){
		nativeDesc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
	}

	D3D11_SUBRESOURCE_DATA upload{};
	const D3D11_SUBRESOURCE_DATA* uploadPtr = nullptr;
	if(!initialData.empty()){
		upload.pSysMem = initialData.data();
		uploadPtr = &upload;
	}

	D3D11BufferResource resource;
	resource.desc = desc;
	resource.state = desc.initialState;
	if(FAILED(m_device->CreateBuffer(
		&nativeDesc,
		uploadPtr,
		resource.object.GetAddressOf()
	))){
		return {};
	}

	return m_buffers.Create(std::move(resource));
}

inline BufferHandle D3D11RHIDevice::ImportNativeBuffer(
	ID3D11Buffer* nativeBuffer,
	uint32_t stride,
	ResourceState initialState,
	const char* debugName
){
	if(!m_device || !nativeBuffer || initialState == ResourceState::Undefined){
		return {};
	}

	D3D11_BUFFER_DESC nativeDesc{};
	nativeBuffer->GetDesc(&nativeDesc);
	if(nativeDesc.ByteWidth == 0) return {};

	BufferDesc desc;
	desc.byteSize = nativeDesc.ByteWidth;
	desc.stride = stride;
	switch(nativeDesc.Usage){
		case D3D11_USAGE_IMMUTABLE:
			desc.usage = ResourceUsage::Immutable;
			break;
		case D3D11_USAGE_DYNAMIC:
			desc.usage = ResourceUsage::Dynamic;
			break;
		case D3D11_USAGE_STAGING:
			desc.usage = ResourceUsage::Staging;
			break;
		default:
			desc.usage = ResourceUsage::Default;
			break;
	}

	if((nativeDesc.BindFlags & D3D11_BIND_VERTEX_BUFFER) != 0){
		desc.bindFlags |= BufferBindFlags::Vertex;
	}
	if((nativeDesc.BindFlags & D3D11_BIND_INDEX_BUFFER) != 0){
		desc.bindFlags |= BufferBindFlags::Index;
	}
	if((nativeDesc.BindFlags & D3D11_BIND_CONSTANT_BUFFER) != 0){
		desc.bindFlags |= BufferBindFlags::Constant;
	}
	if((nativeDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0){
		desc.bindFlags |= BufferBindFlags::ShaderResource;
	}
	if((nativeDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0){
		desc.bindFlags |= BufferBindFlags::UnorderedAccess;
	}
	if((nativeDesc.MiscFlags & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS) != 0){
		desc.bindFlags |= BufferBindFlags::IndirectArguments;
	}

	if((nativeDesc.CPUAccessFlags & D3D11_CPU_ACCESS_READ) != 0){
		desc.cpuAccess |= CpuAccessFlags::Read;
	}
	if((nativeDesc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) != 0){
		desc.cpuAccess |= CpuAccessFlags::Write;
	}

	desc.structured =
		(nativeDesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) != 0;
	desc.raw =
		(nativeDesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) != 0;
	if(desc.structured){
		if(nativeDesc.StructureByteStride == 0) return {};
		if(desc.stride == 0){
			desc.stride = nativeDesc.StructureByteStride;
		}else if(desc.stride != nativeDesc.StructureByteStride){
			return {};
		}
	}

	if(initialState == ResourceState::VertexBuffer &&
		!HasAnyFlag(desc.bindFlags, BufferBindFlags::Vertex)){
		return {};
	}
	if(initialState == ResourceState::IndexBuffer &&
		!HasAnyFlag(desc.bindFlags, BufferBindFlags::Index)){
		return {};
	}
	if(HasAnyFlag(desc.bindFlags, BufferBindFlags::Vertex) && desc.stride == 0){
		return {};
	}

	desc.initialState = initialState;
	if(debugName) desc.debugName = debugName;

	D3D11BufferResource resource;
	resource.desc = std::move(desc);
	resource.state = initialState;
	resource.object = nativeBuffer;
	return m_buffers.Create(std::move(resource));
}

} // namespace RHI
