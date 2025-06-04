#pragma once

#include <string>
#include <unordered_map>
#include <memory>

class GraphicsContext;
struct TextureData;

class TextureLoader {
public:
	TextureLoader(GraphicsContext* set):m_GraphicContext(set){}

	~TextureLoader() = default;
	TextureData* LoadTexture(const std::wstring& filePath);
	TextureData* GetTexture(const std::wstring& filePath);
	void SetTexture(const std::wstring& filePath);
private:
	std::unordered_map<std::wstring, std::shared_ptr<TextureData>> m_Textures;
	GraphicsContext* m_GraphicContext;
};
