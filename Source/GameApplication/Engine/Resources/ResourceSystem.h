// Engine/Resources/resourceSystem.h
#pragma once
#include <string>
#include <unordered_map>
#include <memory>

#include "Service/IService.h"
#include "Engine/Graphics/graphicsContext.h"

struct ModelData;
struct TextureData;

class ModelLoader;
class TextureLoader;


class ResourceSystem : IService{
public:
	void Initialize(GraphicsContext* graphics);
	void Finalize();

	ModelData* LoadModel(const std::string& filePath);
	ModelData* GetModel(const std::string& filePath);

	TextureData* LoadTexture(const std::wstring& filePath);
	TextureData* GetTexture(const std::wstring& filePath);

private:
	GraphicsContext* m_Graphics = nullptr;

	std::shared_ptr<ModelLoader> m_ModelLoader;
	std::shared_ptr<TextureLoader> m_TextureLoader;
};
