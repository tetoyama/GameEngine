#include "resourceSystem.h"

#include "Engine/Resources/Data/modelData.h"
#include "Engine/Resources/Data/textureData.h"

#include "Engine/Resources/Loader/modelLoader.h"
#include "Engine/Resources/Loader/textureLoader.h"

void ResourceSystem::Initialize(GraphicsContext* graphics){
	m_Graphics = graphics;

	m_ModelLoader = std::make_shared<ModelLoader>(m_Graphics);
	m_TextureLoader = std::make_shared<TextureLoader>(m_Graphics);
}

void ResourceSystem::Finalize(){
}

ModelData* ResourceSystem::LoadModel(const std::string& filePath){
	return m_ModelLoader->LoadModel(filePath);
}

ModelData* ResourceSystem::GetModel(const std::string& filePath){
	return m_ModelLoader->GetModel(filePath);
}

TextureData* ResourceSystem::LoadTexture(const std::wstring& filePath){
	return m_TextureLoader->LoadTexture(filePath);
}

TextureData* ResourceSystem::GetTexture(const std::wstring& filePath){
	return m_TextureLoader->GetTexture(filePath);
}
