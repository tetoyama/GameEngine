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

} // namespace RHI
