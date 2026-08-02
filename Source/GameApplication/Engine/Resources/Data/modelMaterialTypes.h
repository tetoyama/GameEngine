#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using ModelSubMeshID = std::uint32_t;
using ImportedMaterialID = std::uint32_t;
using CustomMaterialID = std::uint32_t;

inline constexpr ModelSubMeshID InvalidModelSubMeshID = 0;
inline constexpr ImportedMaterialID InvalidImportedMaterialID = 0;
inline constexpr CustomMaterialID InvalidCustomMaterialID = 0;
inline constexpr std::uint32_t InvalidModelSourceIndex =
	(std::numeric_limits<std::uint32_t>::max)();

enum class MaterialTextureSemantic : std::uint8_t {
	BaseColor = 0,
	Normal,
	Bump,
	Height,
	Metallic,
	Roughness,
	AmbientOcclusion,
	Emissive,
	Opacity
};

enum class MaterialColorSpace : std::uint8_t {
	Linear = 0,
	SRGB
};

enum class MaterialAlphaMode : std::uint8_t {
	Opaque = 0,
	Masked,
	Blend
};

enum class MaterialCullMode : std::uint8_t {
	None = 0,
	Front,
	Back
};

struct MaterialTextureBinding {
	MaterialTextureSemantic semantic = MaterialTextureSemantic::BaseColor;
	MaterialColorSpace colorSpace = MaterialColorSpace::SRGB;
	// Source path is retained for diagnostics and reimport. assetPath is the
	// normalized engine-facing reference used by runtime lookup.
	std::string sourcePath;
	std::string assetPath;
	std::uint32_t sourceTextureIndex = InvalidModelSourceIndex;
	std::uint8_t uvChannel = 0;
	std::array<float, 2> uvScale{1.0f, 1.0f};
	std::array<float, 2> uvOffset{0.0f, 0.0f};
	float uvRotation = 0.0f;
	float strength = 1.0f;
	bool embedded = false;
};

struct MaterialParameters {
	std::array<float, 4> baseColor{1.0f, 1.0f, 1.0f, 1.0f};
	std::array<float, 3> emissiveColor{0.0f, 0.0f, 0.0f};
	float metallic = 0.0f;
	float roughness = 1.0f;
	float ambientOcclusion = 1.0f;
	float emissiveIntensity = 0.0f;
	float opacity = 1.0f;
	float normalScale = 1.0f;
	float heightScale = 0.0f;
};

struct MaterialRenderState {
	MaterialAlphaMode alphaMode = MaterialAlphaMode::Opaque;
	MaterialCullMode cullMode = MaterialCullMode::Back;
	float alphaCutoff = 0.5f;
	bool depthWrite = true;
	bool receiveShadow = true;
};

struct MaterialDescriptor {
	// Legacy ShaderIDと同じ識別空間を初期互換契約として使用する。
	std::int32_t shaderID = 0;
	MaterialParameters parameters;
	std::vector<MaterialTextureBinding> textures;
	MaterialRenderState renderState;
};

struct ModelLocalBounds {
	std::array<float, 3> minimum{0.0f, 0.0f, 0.0f};
	std::array<float, 3> maximum{0.0f, 0.0f, 0.0f};
	bool valid = false;
};

struct ModelSubMeshDefinition {
	ModelSubMeshID id = InvalidModelSubMeshID;
	std::string name;
	std::string nodePath;
	std::uint32_t geometryIndex = InvalidModelSourceIndex;
	ImportedMaterialID defaultMaterialID = InvalidImportedMaterialID;
	ModelLocalBounds localBounds;
};

struct ImportedMaterialDefinition {
	ImportedMaterialID id = InvalidImportedMaterialID;
	std::string name;
	std::uint32_t sourceMaterialIndex = InvalidModelSourceIndex;
	MaterialDescriptor descriptor;
};

enum class ModelMaterialImportDiagnosticSeverity : std::uint8_t {
	Info = 0,
	Warning,
	Error
};

enum class ModelMaterialImportDiagnosticCode : std::uint8_t {
	MissingMaterialName = 0,
	MissingMeshName,
	InvalidMaterialIndex,
	EmptyTexturePath,
	UnsupportedTextureMapping,
	StableLocalIDCollision
};

struct ModelMaterialImportDiagnostic {
	ModelMaterialImportDiagnosticSeverity severity =
		ModelMaterialImportDiagnosticSeverity::Info;
	ModelMaterialImportDiagnosticCode code =
		ModelMaterialImportDiagnosticCode::MissingMaterialName;
	std::string message;
	std::uint32_t sourceMaterialIndex = InvalidModelSourceIndex;
	std::uint32_t sourceMeshIndex = InvalidModelSourceIndex;
};

enum class SubMeshMaterialSource : std::uint8_t {
	ModelDefault = 0,
	CustomMaterial
};

struct SubMeshMaterialAssignment {
	SubMeshMaterialSource source = SubMeshMaterialSource::ModelDefault;
	CustomMaterialID customMaterialID = InvalidCustomMaterialID;
};

struct ModelSubMeshRenderState {
	ModelSubMeshID subMeshID = InvalidModelSubMeshID;
	bool visible = true;
	bool castShadow = true;
	SubMeshMaterialAssignment material;
};

struct CustomMaterialEntry {
	CustomMaterialID id = InvalidCustomMaterialID;
	std::string name;
	MaterialDescriptor inlineMaterial;
};

enum class ModelMaterialResolutionSource : std::uint8_t {
	ImportedMaterial = 0,
	CustomMaterial,
	ImportedMaterialFallback,
	EngineDefault
};

enum class ModelMaterialResolutionIssue : std::uint8_t {
	None = 0,
	MissingSubMeshDefinition,
	MissingImportedMaterial,
	MissingMaterialComponent,
	MissingCustomMaterial
};
