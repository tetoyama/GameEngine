// =======================================================================
// 
// resourceService.h
// 
// =======================================================================
#pragma once

#include <unordered_map>
#include <typeindex>
#include <memory>
#include <string>
#include <sstream>

#include "Service/IService.h"
#include "Loader/IResourceLoader.h"
#include "Loader/ResourceLoader.h"

class GraphicsContext;
class AudioContext;

// リソースの読み込み・キャッシュ・解放を管理するサービス
class ResourceService : public IService {
public:
	// 描画・音声コンテキストを受け取り、各種ローダーを登録する
	void Initialize(GraphicsContext* graphics, AudioContext* audio);

	void Shutdown() override;

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

	// キャッシュ中の未使用リソースを全ローダーに対して一括解放する
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
        auto loader = std::make_shared<ResourceLoader<T>>();
        loader->SetupLoadFunc(static_cast<void*>(m_Graphics));
        m_Loaders[std::type_index(typeid(T))] = loader;
    }

    GraphicsContext* m_Graphics = nullptr;
	AudioContext* m_Audio = nullptr;

    std::unordered_map<std::type_index, std::shared_ptr<IResourceLoader>> m_Loaders;
};
