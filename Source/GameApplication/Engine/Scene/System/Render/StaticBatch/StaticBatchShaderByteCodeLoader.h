#pragma once

#include <cstddef>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <vector>

class StaticBatchShaderByteCodeLoader {
public:
	bool Load(
		const std::string& vertexShaderPath,
		const std::string& pixelShaderPath
	){
		Reset();
		if(vertexShaderPath.empty() || pixelShaderPath.empty()){
			return false;
		}
		if(!LoadFile(vertexShaderPath, m_vertexShader)){
			Reset();
			return false;
		}
		if(!LoadFile(pixelShaderPath, m_pixelShader)){
			Reset();
			return false;
		}
		return IsValid();
	}

	void Reset() noexcept {
		m_vertexShader.clear();
		m_pixelShader.clear();
	}

	bool IsValid() const noexcept {
		return !m_vertexShader.empty() && !m_pixelShader.empty();
	}

	std::span<const std::byte> VertexShader() const noexcept {
		return m_vertexShader;
	}

	std::span<const std::byte> PixelShader() const noexcept {
		return m_pixelShader;
	}

private:
	static bool LoadFile(
		const std::string& path,
		std::vector<std::byte>& output
	){
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if(!file) return false;

		const std::streampos endPosition = file.tellg();
		if(endPosition <= std::streampos{0}) return false;

		const std::streamoff fileSize =
			static_cast<std::streamoff>(endPosition);
		if(fileSize <= 0 ||
			fileSize > (std::numeric_limits<std::streamsize>::max)()){
			return false;
		}

		const std::size_t byteCount = static_cast<std::size_t>(fileSize);
		output.resize(byteCount);

		file.seekg(0, std::ios::beg);
		if(!file.read(
			reinterpret_cast<char*>(output.data()),
			static_cast<std::streamsize>(fileSize)
		)){
			output.clear();
			return false;
		}
		return true;
	}

	std::vector<std::byte> m_vertexShader;
	std::vector<std::byte> m_pixelShader;
};
