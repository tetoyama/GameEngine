// =======================================================================
// 
// shaderLoader.h
// 
// =======================================================================
#pragma once
#include <memory>
#include <string>
#include "Resources/Data/vertexShaderData.h"
#include "Resources/Data/pixelShaderData.h"
#include "Graphics/graphicsContext.h"
#include "Backends/checkFileExtention.h"

#include "ResourceLoader.h"
#include <d3dcompiler.h>

inline std::shared_ptr<VertexShaderData> LoadVertexShaderFromFile(const std::string& path, GraphicsContext* context) {
	if (!HasExtension(path, "cso")) return nullptr;

	auto m_Shader= std::make_shared<VertexShaderData>();
	if (!context->CreateVertexShader(path.c_str(), &shader->m_VertexShader, &shader->m_VertexLayout))
		return m_Nullptr;
	shader->FilePath = path;
	return m_Shader;
}


inline std::shared_ptr<PixelShaderData>
LoadPixelShaderFromFile(const std::string& path, GraphicsContext* context){
	if(!context) return nullptr;

	ID3D11Device* device = context->GetDevice();
	if(!device) return nullptr;

	auto m_Shader= std::make_shared<PixelShaderData>();
	shader->FilePath = path;

	// HLSL
	if(HasExtension(path, "hlsl")){
		Microsoft::WRL::ComPtr<ID3DBlob> m_PsBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> m_ErrorBlob;

		HRESULT m_Hr= D3DCompileFromFile(
			std::wstring(path.begin(), path.end()).c_str(),
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"main",
			"ps_5_0",
			D3DCOMPILE_ENABLE_STRICTNESS,
			0,
			psBlob.GetAddressOf(),
			errorBlob.GetAddressOf()
		);

		if(FAILED(hr)){
			if(errorBlob)
				OutputDebugStringA(
					static_cast<const char*>(errorBlob->GetBufferPointer()));
			return m_Nullptr;
		}

		hr = device->CreatePixelShader(
			psBlob->GetBufferPointer(),
			psBlob->GetBufferSize(),
			nullptr,
			shader->m_PixelShader.GetAddressOf()
		);

		if(FAILED(hr))
			return m_Nullptr;

		return m_Shader;
	}

	// CSO
	if(HasExtension(path, "cso")){
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if(!file) return nullptr;

		const size_t m_Size= static_cast<size_t>(file.tellg());
		file.seekg(0, std::ios::beg);

		std::vector<char> buffer(size);
		file.read(buffer.data(), size);
		file.close();

		HRESULT m_Hr= device->CreatePixelShader(
			buffer.data(),
			buffer.size(),
			nullptr,
			shader->m_PixelShader.GetAddressOf()
		);

		if(FAILED(hr))
			return m_Nullptr;

		return m_Shader;
	}

	return m_Nullptr;
}


template<>
inline void ResourceLoader<VertexShaderData>::SetupLoadFunc(void* contextPtr) {
	OutputDebugStringA("SetupLoadFunc VertexShaderData called\n");

	auto m_Context= static_cast<GraphicsContext*>(contextPtr);
	SetLoadFunction([=](const std::string& path, std::shared_ptr<void> /*args*/) {
		return LoadVertexShaderFromFile(path, context);
	});
}

template<>
inline void ResourceLoader<PixelShaderData>::SetupLoadFunc(void* contextPtr) {
	OutputDebugStringA("SetupLoadFunc PixelShaderData called\n");

	auto m_Context= static_cast<GraphicsContext*>(contextPtr);
	SetLoadFunction([=](const std::string& path, std::shared_ptr<void> /*args*/) {
		return LoadPixelShaderFromFile(path, context);
	});
}