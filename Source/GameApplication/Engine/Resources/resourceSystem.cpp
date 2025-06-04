#include "resourceSystem.h"

#include "Engine/Resources/Data/modelData.h"
#include "Engine/Resources/Data/textureData.h"

#include "Engine/Resources/Loader/modelLoader.h"
#include "Engine/Resources/Loader/textureLoader.h"
#include "Engine/Resources/Loader/shaderLoader.h"

void ResourceSystem::Initialize(GraphicsContext* graphics){
	m_Graphics = graphics;

	m_ModelLoader = std::make_shared<ModelLoader>(m_Graphics);
	m_TextureLoader = std::make_shared<TextureLoader>(m_Graphics);
	m_ShaderLoader = std::make_shared<ShaderLoader>(m_Graphics);
}

void ResourceSystem::Finalize(){
}
