// Engine/Resources/resourceSystem.h
#pragma once
#include <string>
#include <unordered_map>
#include <memory>

#include "Service/IService.h"
#include "Engine/Graphics/graphicsContext.h"

struct ModelData;
struct TextureData;
struct VertexShaderData;
struct PixelShaderData;

class ModelLoader;
class TextureLoader;
class ShaderLoader;


class ResourceSystem : IService{
public:
	void Initialize(GraphicsContext* graphics);
	void Finalize();

	ModelLoader* GetModelLoader(){
		return m_ModelLoader.get();
	}
	TextureLoader* GetTextureLoader(){
		return m_TextureLoader.get();
	}
	ShaderLoader* GetShaderLoader(){
		return m_ShaderLoader.get();
	}
private:
	GraphicsContext* m_Graphics = nullptr;

	std::shared_ptr<ModelLoader> m_ModelLoader;
	std::shared_ptr<TextureLoader> m_TextureLoader;
	std::shared_ptr<ShaderLoader> m_ShaderLoader;
};
