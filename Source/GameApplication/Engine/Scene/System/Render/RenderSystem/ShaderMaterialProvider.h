#pragma once

#include <span>
#include <string>

struct ShaderMaterial {
	std::string filePath;
	std::string entryPoint;
};

class IShaderMaterialProvider {
public:
	virtual ~IShaderMaterialProvider() = default;

	virtual std::span<const ShaderMaterial> GetShaderMaterials() const noexcept = 0;
};
