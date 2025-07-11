#pragma once

#include <string>
#include <unordered_map>
#include <memory>

class GraphicsContext;
struct TextureData;

class TextureLoader {
public:
	TextureLoader(GraphicsContext* set):m_GraphicContext(set){}

	~TextureLoader() {
		for (auto& [key,data] : m_Textures) {
			data.reset();
		}
	}
	std::shared_ptr<TextureData> LoadTexture(const std::string& filePath);
	void UnLoadTexture(const std::string& filePath);
	std::shared_ptr<TextureData> GetTexture(const std::string& filePath);
	void SetTexture(const std::string& filePath);
	void SetBumpTexture(const std::string& filePath);
private:
	std::unordered_map<std::string, std::shared_ptr<TextureData>> m_Textures;
	GraphicsContext* m_GraphicContext;
};
