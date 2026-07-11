#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <d3d11.h>

// M-5: D3D11 constant-buffer upload policy shared by the legacy
// UpdateSubresource path and the planned DYNAMIC + Map path.
//
// A constant buffer is always uploaded as one complete CPU mirror. Partial
// writes are deliberately rejected so both strategies have the same contract.
enum class D3D11ConstantBufferUploadStrategy : std::uint8_t {
	UpdateSubresource,
	DynamicMap
};

class D3D11ConstantBufferUpload final {
public:
	static constexpr UINT Alignment = 16u;
	static constexpr UINT MaximumByteWidth =
		D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * Alignment;

	static constexpr bool IsValidByteWidth(UINT byteWidth) noexcept {
		return byteWidth != 0u &&
			(byteWidth % Alignment) == 0u &&
			byteWidth <= MaximumByteWidth;
	}

	static constexpr D3D11_BUFFER_DESC MakeBufferDesc(
		UINT byteWidth,
		D3D11ConstantBufferUploadStrategy strategy
	) noexcept {
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = byteWidth;
		desc.Usage = strategy == D3D11ConstantBufferUploadStrategy::DynamicMap
			? D3D11_USAGE_DYNAMIC
			: D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags =
			strategy == D3D11ConstantBufferUploadStrategy::DynamicMap
			? D3D11_CPU_ACCESS_WRITE
			: 0u;
		return desc;
	}

	static bool Create(
		ID3D11Device* device,
		UINT byteWidth,
		D3D11ConstantBufferUploadStrategy strategy,
		ID3D11Buffer** output,
		const void* initialData = nullptr
	) noexcept {
		if(!device || !output || !IsValidByteWidth(byteWidth)){
			return false;
		}

		*output = nullptr;
		const D3D11_BUFFER_DESC desc = MakeBufferDesc(byteWidth, strategy);
		D3D11_SUBRESOURCE_DATA resourceData{};
		resourceData.pSysMem = initialData;

		const HRESULT result = device->CreateBuffer(
			&desc,
			initialData ? &resourceData : nullptr,
			output
		);
		return SUCCEEDED(result) && *output;
	}

	static bool UploadBytes(
		ID3D11DeviceContext* context,
		ID3D11Buffer* buffer,
		const void* data,
		UINT dataByteWidth,
		D3D11ConstantBufferUploadStrategy strategy
	) noexcept {
		if(!context || !buffer || !data || !IsValidByteWidth(dataByteWidth)){
			return false;
		}

		if(strategy == D3D11ConstantBufferUploadStrategy::UpdateSubresource){
			context->UpdateSubresource(buffer, 0, nullptr, data, 0, 0);
			return true;
		}

		D3D11_MAPPED_SUBRESOURCE mapped{};
		const HRESULT result = context->Map(
			buffer,
			0,
			D3D11_MAP_WRITE_DISCARD,
			0,
			&mapped
		);
		if(FAILED(result) || !mapped.pData){
			return false;
		}

		std::memcpy(mapped.pData, data, dataByteWidth);
		context->Unmap(buffer, 0);
		return true;
	}

	template<class T>
	static bool Upload(
		ID3D11DeviceContext* context,
		ID3D11Buffer* buffer,
		const T& data,
		D3D11ConstantBufferUploadStrategy strategy
	) noexcept {
		static_assert(
			std::is_trivially_copyable_v<T>,
			"Constant-buffer CPU mirrors must be trivially copyable."
		);
		static_assert(
			(sizeof(T) % Alignment) == 0u,
			"Constant-buffer CPU mirrors must be 16-byte aligned in size."
		);
		static_assert(
			sizeof(T) <= MaximumByteWidth,
			"Constant-buffer CPU mirror exceeds the D3D11 size limit."
		);

		return UploadBytes(
			context,
			buffer,
			&data,
			static_cast<UINT>(sizeof(T)),
			strategy
		);
	}
};
