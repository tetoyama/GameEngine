// =======================================================================
// D3D11RHIConversions.h
// =======================================================================
#pragma once

#include <d3d11.h>

#include "Service/Graphics/RHI/RHIDescriptors.h"

namespace RHI::D3D11Detail {

inline DXGI_FORMAT ToDXGIFormat(Format format){
	switch(format){
		case Format::R8_UNorm: return DXGI_FORMAT_R8_UNORM;
		case Format::RG8_UNorm: return DXGI_FORMAT_R8G8_UNORM;
		case Format::RGBA8_UNorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case Format::BGRA8_UNorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
		case Format::R16_UInt: return DXGI_FORMAT_R16_UINT;
		case Format::R32_UInt: return DXGI_FORMAT_R32_UINT;
		case Format::R16_Float: return DXGI_FORMAT_R16_FLOAT;
		case Format::R32_Float: return DXGI_FORMAT_R32_FLOAT;
		case Format::RG16_Float: return DXGI_FORMAT_R16G16_FLOAT;
		case Format::RG32_Float: return DXGI_FORMAT_R32G32_FLOAT;
		case Format::RGBA16_Float: return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case Format::RGBA32_Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case Format::RGBA32_UInt: return DXGI_FORMAT_R32G32B32A32_UINT;
		case Format::D24_UNorm_S8_UInt: return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case Format::D32_Float: return DXGI_FORMAT_D32_FLOAT;
		case Format::D32_Float_S8X24_UInt: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		default: return DXGI_FORMAT_UNKNOWN;
	}
}

inline D3D11_USAGE ToUsage(ResourceUsage usage){
	switch(usage){
		case ResourceUsage::Immutable: return D3D11_USAGE_IMMUTABLE;
		case ResourceUsage::Dynamic: return D3D11_USAGE_DYNAMIC;
		case ResourceUsage::Staging: return D3D11_USAGE_STAGING;
		default: return D3D11_USAGE_DEFAULT;
	}
}

inline UINT ToCpuAccess(CpuAccessFlags flags){
	UINT result = 0;
	if(HasAnyFlag(flags, CpuAccessFlags::Read)) result |= D3D11_CPU_ACCESS_READ;
	if(HasAnyFlag(flags, CpuAccessFlags::Write)) result |= D3D11_CPU_ACCESS_WRITE;
	return result;
}

inline UINT ToBufferBind(BufferBindFlags flags){
	UINT result = 0;
	if(HasAnyFlag(flags, BufferBindFlags::Vertex)) result |= D3D11_BIND_VERTEX_BUFFER;
	if(HasAnyFlag(flags, BufferBindFlags::Index)) result |= D3D11_BIND_INDEX_BUFFER;
	if(HasAnyFlag(flags, BufferBindFlags::Constant)) result |= D3D11_BIND_CONSTANT_BUFFER;
	if(HasAnyFlag(flags, BufferBindFlags::ShaderResource)) result |= D3D11_BIND_SHADER_RESOURCE;
	if(HasAnyFlag(flags, BufferBindFlags::UnorderedAccess)) result |= D3D11_BIND_UNORDERED_ACCESS;
	return result;
}

inline UINT ToTextureBind(TextureBindFlags flags){
	UINT result = 0;
	if(HasAnyFlag(flags, TextureBindFlags::ShaderResource)) result |= D3D11_BIND_SHADER_RESOURCE;
	if(HasAnyFlag(flags, TextureBindFlags::RenderTarget)) result |= D3D11_BIND_RENDER_TARGET;
	if(HasAnyFlag(flags, TextureBindFlags::DepthStencil)) result |= D3D11_BIND_DEPTH_STENCIL;
	if(HasAnyFlag(flags, TextureBindFlags::UnorderedAccess)) result |= D3D11_BIND_UNORDERED_ACCESS;
	return result;
}

inline D3D11_PRIMITIVE_TOPOLOGY ToTopology(PrimitiveTopology topology){
	switch(topology){
		case PrimitiveTopology::PointList: return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
		case PrimitiveTopology::LineList: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		case PrimitiveTopology::LineStrip: return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
		case PrimitiveTopology::TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case PrimitiveTopology::TriangleList: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		default: return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	}
}

inline D3D11_FILL_MODE ToFillMode(FillMode mode){
	return mode == FillMode::Wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
}

inline D3D11_CULL_MODE ToCullMode(CullMode mode){
	switch(mode){
		case CullMode::None: return D3D11_CULL_NONE;
		case CullMode::Front: return D3D11_CULL_FRONT;
		default: return D3D11_CULL_BACK;
	}
}

inline D3D11_COMPARISON_FUNC ToComparison(ComparisonFunc function){
	switch(function){
		case ComparisonFunc::Never: return D3D11_COMPARISON_NEVER;
		case ComparisonFunc::Less: return D3D11_COMPARISON_LESS;
		case ComparisonFunc::Equal: return D3D11_COMPARISON_EQUAL;
		case ComparisonFunc::LessEqual: return D3D11_COMPARISON_LESS_EQUAL;
		case ComparisonFunc::Greater: return D3D11_COMPARISON_GREATER;
		case ComparisonFunc::NotEqual: return D3D11_COMPARISON_NOT_EQUAL;
		case ComparisonFunc::GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
		default: return D3D11_COMPARISON_ALWAYS;
	}
}

inline D3D11_BLEND ToBlend(BlendFactor factor){
	switch(factor){
		case BlendFactor::Zero: return D3D11_BLEND_ZERO;
		case BlendFactor::One: return D3D11_BLEND_ONE;
		case BlendFactor::SourceColor: return D3D11_BLEND_SRC_COLOR;
		case BlendFactor::InverseSourceColor: return D3D11_BLEND_INV_SRC_COLOR;
		case BlendFactor::SourceAlpha: return D3D11_BLEND_SRC_ALPHA;
		case BlendFactor::InverseSourceAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
		case BlendFactor::DestinationColor: return D3D11_BLEND_DEST_COLOR;
		case BlendFactor::InverseDestinationColor: return D3D11_BLEND_INV_DEST_COLOR;
		case BlendFactor::DestinationAlpha: return D3D11_BLEND_DEST_ALPHA;
		case BlendFactor::InverseDestinationAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
		default: return D3D11_BLEND_ONE;
	}
}

inline D3D11_BLEND_OP ToBlendOperation(BlendOperation operation){
	switch(operation){
		case BlendOperation::Subtract: return D3D11_BLEND_OP_SUBTRACT;
		case BlendOperation::ReverseSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
		case BlendOperation::Minimum: return D3D11_BLEND_OP_MIN;
		case BlendOperation::Maximum: return D3D11_BLEND_OP_MAX;
		default: return D3D11_BLEND_OP_ADD;
	}
}

} // namespace RHI::D3D11Detail
