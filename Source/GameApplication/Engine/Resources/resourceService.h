// ResourceService.h
#pragma once
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <string>

#include "Service/IService.h"
#include "Loader/IResourceLoader.h"
#include "Loader/ResourceLoader.h"  // テンプレート版

#include "Data/textureData.h"
#include "Data/modelData.h"
#include "Data/shaderData.h"

#include "Loader/textureLoader.h"
#include "Loader/shaderLoader.h"
#include "Loader/modelLoader.h"

class GraphicsContext;

class ResourceService : public IService {
public:
    void Initialize(GraphicsContext* graphics) {
        m_Graphics = graphics;
        RegisterLoader<TextureData>();
        RegisterLoader<ModelData>();
        RegisterLoader<VertexShaderData>();
        RegisterLoader<PixelShaderData>();
    }

    void Shutdown() override {

		ClearAllUnused();

		for(auto iLoader : m_Loaders){
			iLoader.second->DumpCacheState();
		}

        m_Loaders.clear();
        m_Graphics = nullptr;
    }

    template<typename T, typename... Args>
    std::shared_ptr<T> Load(const std::string& path, Args&&... args) {
        auto it = m_Loaders.find(std::type_index(typeid(T)));
        if (it != m_Loaders.end()) {
            auto loader = static_cast<ResourceLoader<T>*>(it->second.get());
            return loader->Load(path, std::forward<Args>(args)...);
        }
        return nullptr;
    }


    template<typename T>
    void Unload(const std::string& path) {
        auto it = m_Loaders.find(std::type_index(typeid(T)));
        if (it != m_Loaders.end()) {
            it->second->Unload(path);
        }
    }

    template<typename T>
    void ClearUnused() {
        auto it = m_Loaders.find(std::type_index(typeid(T)));
        if (it != m_Loaders.end()) {
            it->second->ClearUnused();
        }
    }

    void ClearAllUnused() {
        for (auto& [type, loader] : m_Loaders) {
            loader->ClearUnused();
        }
    }

    GraphicsContext* GetGraphicsContext() const {
        return m_Graphics;
    }

private:
    template<typename T>
    void RegisterLoader() {
		OutputDebugStringA("RegisterLoader called\n");

        auto loader = std::make_shared<ResourceLoader<T>>();
        loader->SetupLoadFunc(static_cast<void*>(m_Graphics));
        m_Loaders[std::type_index(typeid(T))] = loader;
    }

    GraphicsContext* m_Graphics = nullptr;
    std::unordered_map<std::type_index, std::shared_ptr<IResourceLoader>> m_Loaders;
};
