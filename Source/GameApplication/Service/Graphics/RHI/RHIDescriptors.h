// =======================================================================
//
// RHIDescriptors.h
//
// Backend非依存のResource / Pipeline / RenderPass記述。
//
// =======================================================================
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "RHIHandle.h"

namespace RHI {

enum class BackendType : uint8_t {
	Null,
	Direct3D11,
	Direct3D12,
	Vulkan
};

enum class Format : uint16_t {
	Unknown,
	R8_UNorm,
	RG8_UNorm,
	RGBA8_UNorm,
	BGRA8_UNorm,
	R16_UInt,
	R32_UInt,
	R16_Float,
	R32_Float,
	RG16_Float,
	RG32_Float,
	RGBA16_Float,
	RGBA32_Float,
	RGBA32_UInt,
	D24_UNorm_S8_UInt,
	D32_Float,
	D32_Float_S8X24_UInt
};

enum class ResourceUsage : uint8_t {
	Default,
	Immutable,
	Dynamic,
	Staging
};

enum class CpuAccessFlags : uint8_t {
	None = 0,
	Read = 1u << 0u,
	Write = 1u << 1u
};

enum class BufferBindFlags : uint16_t {
	None = 0,
	Vertex = 1u << 0u,
	Index = 1u << 1u,
	Constant = 1u << 2u,
	ShaderResource = 1u << 3u,
	UnorderedAccess = 1u << 4u,
	IndirectArguments = 1u << 5u
};

enum class TextureBindFlags : uint16_t {
	None = 0,
	ShaderResource = 1u << 0u,
	RenderTarget = 1u << 1u,
	DepthStencil = 1u << 2u,
	UnorderedAccess = 1u << 3u
};

template<typename Enum>
constexpr Enum operator|(Enum lhs, Enum rhs) noexcept {
	return static_cast<Enum>(
		static_cast<std::underlying_type_t<Enum>>(lhs) |
		static_cast<std::underlying_type_t<Enum>>(rhs)
	);
}

template<typename Enum>
constexpr Enum operator&(Enum lhs, Enum rhs) noexcept {
	return static_cast<Enum>(
		static_cast<std::underlying_type_t<Enum>>(lhs) &
		static_cast<std::underlying_type_t<Enum>>(rhs)
	);
}

template<typename Enum>
constexpr Enum& operator|=(Enum& lhs, Enum rhs) noexcept {
	lhs = lhs | rhs;
	return lhs;
}

template<typename Enum>
constexpr bool HasAnyFlag(Enum value, Enum flags) noexcept {
	return static_cast<std::underlying_type_t<Enum>>(value & flags) != 0;
}

enum class ShaderStage : uint8_t {
	Vertex,
	Pixel,
	Compute
};

enum class PrimitiveTopology : uint8_t {
	Undefined,
	PointList,
	LineList,
	LineStrip,
	TriangleList,
	TriangleStrip
};

enum class IndexFormat : uint8_t {
	UInt16,
	UInt32
};

enum class FillMode : uint8_t {
	Solid,
	Wireframe
};

enum class CullMode : uint8_t {
	None,
	Front,
	Back
};

enum class ComparisonFunc : uint8_t {
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always
};

enum class BlendFactor : uint8_t {
	Zero,
	One,
	SourceColor,
	InverseSourceColor,
	SourceAlpha,
	InverseSourceAlpha,
	DestinationColor,
	InverseDestinationColor,
	DestinationAlpha,
	InverseDestinationAlpha
};

enum class BlendOperation : uint8_t {
	Add,
	Subtract,
	ReverseSubtract,
	Minimum,
	Maximum
};

enum class LoadOperation : uint8_t {
	Load,
	Clear,
	Discard
};

enum class StoreOperation : uint8_t {
	Store,
	Discard
};

struct BufferDesc {
	uint32_t byteSize = 0;
	uint32_t stride = 0;
	ResourceUsage usage = ResourceUsage::Default;
	BufferBindFlags bindFlags = BufferBindFlags::None;
	CpuAccessFlags cpuAccess = CpuAccessFlags::None;
	bool structured = false;
	bool raw = false;
	std::string debugName;
};

struct TextureDesc {
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t depth = 1;
	uint16_t mipLevels = 1;
	uint16_t arraySize = 1;
	uint8_t sampleCount = 1;
	Format format = Format::RGBA8_UNorm;
	ResourceUsage usage = ResourceUsage::Default;
	TextureBindFlags bindFlags = TextureBindFlags::ShaderResource;
	CpuAccessFlags cpuAccess = CpuAccessFlags::None;
	bool generateMips = false;
	std::string debugName;
};

struct ShaderDesc {
	ShaderStage stage = ShaderStage::Vertex;
	std::string entryPoint;
	std::string debugName;
};

struct InputElementDesc {
	std::string semanticName;
	uint32_t semanticIndex = 0;
	Format format = Format::Unknown;
	uint32_t inputSlot = 0;
	uint32_t alignedByteOffset = 0;
	bool perInstance = false;
	uint32_t instanceStepRate = 0;
};

struct RasterizerStateDesc {
	FillMode fillMode = FillMode::Solid;
	CullMode cullMode = CullMode::Back;
	bool frontCounterClockwise = false;
	bool depthClipEnable = true;
	bool scissorEnable = false;
	bool multisampleEnable = false;
};

struct DepthStencilStateDesc {
	bool depthEnable = true;
	bool depthWriteEnable = true;
	ComparisonFunc depthFunction = ComparisonFunc::LessEqual;
};

struct BlendTargetDesc {
	bool blendEnable = false;
	BlendFactor sourceColor = BlendFactor::One;
	BlendFactor destinationColor = BlendFactor::Zero;
	BlendOperation colorOperation = BlendOperation::Add;
	BlendFactor sourceAlpha = BlendFactor::One;
	BlendFactor destinationAlpha = BlendFactor::Zero;
	BlendOperation alphaOperation = BlendOperation::Add;
	uint8_t writeMask = 0x0Fu;
};

struct BlendStateDesc {
	bool alphaToCoverageEnable = false;
	bool independentBlendEnable = false;
	std::array<BlendTargetDesc, 8> targets{};
};

struct PipelineStateDesc {
	ShaderHandle vertexShader;
	ShaderHandle pixelShader;
	ShaderHandle computeShader;
	std::vector<InputElementDesc> inputLayout;
	PrimitiveTopology topology = PrimitiveTopology::TriangleList;
	RasterizerStateDesc rasterizer;
	DepthStencilStateDesc depthStencil;
	BlendStateDesc blend;
	std::string debugName;
};

struct Viewport {
	float x = 0.0f;
	float y = 0.0f;
	float width = 1.0f;
	float height = 1.0f;
	float minDepth = 0.0f;
	float maxDepth = 1.0f;
};

struct SwapChainDesc {
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t bufferCount = 2;
	Format format = Format::RGBA8_UNorm;
	bool allowTearing = false;
	std::string debugName;
};

struct ColorAttachmentDesc {
	TextureHandle texture;
	LoadOperation loadOperation = LoadOperation::Load;
	StoreOperation storeOperation = StoreOperation::Store;
	std::array<float, 4> clearColor{0.0f, 0.0f, 0.0f, 0.0f};
};

struct DepthAttachmentDesc {
	TextureHandle texture;
	LoadOperation depthLoadOperation = LoadOperation::Load;
	StoreOperation depthStoreOperation = StoreOperation::Store;
	LoadOperation stencilLoadOperation = LoadOperation::Load;
	StoreOperation stencilStoreOperation = StoreOperation::Store;
	float clearDepth = 1.0f;
	uint8_t clearStencil = 0;
};

struct RenderPassDesc {
	std::vector<ColorAttachmentDesc> colorAttachments;
	bool hasDepthAttachment = false;
	DepthAttachmentDesc depthAttachment;
	std::string debugName;
};

} // namespace RHI
