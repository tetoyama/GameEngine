// =======================================================================
// 
// resourceLoader.h
// 
// =======================================================================
#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <typeinfo>
#ifdef _WIN32
#include <Windows.h>
#else
#include <iostream>
#endif
#include <sstream>
#include <mutex>
#include <future>
#include <exception>
#include <tuple>
#include <utility>

#include "IResourceLoader.h"

namespace ResourceLoaderDetail {
inline void Trace(const char* message) {
#ifdef _WIN32
    OutputDebugStringA(message);
#else
    std::clog << message;
#endif
}
}

// 汎用リソースローダーのテンプレートクラス
template<typename T>
class ResourceLoader : public IResourceLoader {
public:
    virtual ~ResourceLoader() {
        ResourceLoaderDetail::Trace("ResourceLoader destroyed\n");
        if (!m_Cache.empty()) {
            DumpCacheState();
        }
    }

    using AnyLoadFunc = std::function<std::shared_ptr<T>(const std::string&, std::shared_ptr<void>)>;

    template<typename... Args>
    std::shared_ptr<T> Load(const std::string& path, Args&&... args) {
        try {
            using ArgsTuple = std::tuple<std::decay_t<Args>...>;
            auto argsTuple = std::make_shared<ArgsTuple>(std::forward<Args>(args)...);

            std::string cacheKey = CreateCacheKey(path, *argsTuple);

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_Cache.find(cacheKey);
                if (it != m_Cache.end()) return it->second;
            }

            if (!m_LoadFunc) return nullptr;

            auto result = m_LoadFunc(path, argsTuple);
            if (result) {
                ResourceLoaderDetail::Trace("Load Success: ");
                ResourceLoaderDetail::Trace(cacheKey.c_str());
                ResourceLoaderDetail::Trace("\n");

                std::lock_guard<std::mutex> lock(m_mutex);
                m_Cache[cacheKey] = result;
            } else {
                ResourceLoaderDetail::Trace("Load FAILED: ");
                ResourceLoaderDetail::Trace(cacheKey.c_str());
                ResourceLoaderDetail::Trace("\n");
            }

            return result;
        }
        catch (const std::exception& e) {
            ResourceLoaderDetail::Trace(("Load exception: " + std::string(e.what()) + "\n").c_str());
            return nullptr;
        }
        catch (...) {
            ResourceLoaderDetail::Trace("Load unknown exception\n");
            return nullptr;
        }
    }

    // 非同期ロード
    template<typename... Args>
    std::future<std::shared_ptr<T>> LoadAsync(const std::string& path, Args&&... args) {
        return std::async(std::launch::async, [this, path, argsTuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            // The loader must remain alive until the returned future finishes.
            return std::apply([this,&path](auto&&... unpacked) {
                return this->Load(path,std::forward<decltype(unpacked)>(unpacked)...);
            },std::move(argsTuple));
        });
    }

    void SetLoadFunction(AnyLoadFunc func) {
        m_LoadFunc = func;
    }

    const std::type_info& GetType() const override {
        return typeid(T);
    }

    virtual void SetupLoadFunc(void* /*contextPtr*/) override {
        // 特殊化で実装
    }

    // path + args を使うアンロード（推奨）
    template<typename... Args>
    void Unload(const std::string& path, Args&&... args) {
        using ArgsTuple = std::tuple<std::decay_t<Args>...>;
        ArgsTuple argsTuple(std::forward<Args>(args)...);

        std::string cacheKey = CreateCacheKey(path, argsTuple);

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_Cache.find(cacheKey);
        if (it != m_Cache.end()) {
            if (it->second.use_count() == 1) {
                m_Cache.erase(it);
                ResourceLoaderDetail::Trace("Unloaded resource: ");
            } else {
                ResourceLoaderDetail::Trace("Cannot unload: still in use: ");
            }
            ResourceLoaderDetail::Trace(cacheKey.c_str());
            ResourceLoaderDetail::Trace("\n");
        }
    }

    void Unload(const std::string& path) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_Cache.begin(); it != m_Cache.end(); ) {
            if (it->first.rfind(path + ":", 0) == 0) {
                if (it->second.use_count() == 1) {
                    ResourceLoaderDetail::Trace(("Unloaded: " + it->first + "\n").c_str());
                    it = m_Cache.erase(it);
                } else {
                    ResourceLoaderDetail::Trace(("Still in use: " + it->first + "\n").c_str());
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }

    void ClearUnused() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_Cache.begin(); it != m_Cache.end();) {
            if (it->second.use_count() == 1)
                it = m_Cache.erase(it);
            else
                ++it;
        }
    }

    void DumpCacheState() const override {
        ResourceLoaderDetail::Trace("DumpCacheState\n");
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_Cache.empty()) {
            ResourceLoaderDetail::Trace("  Cache is empty\n");
        }
        for (const auto& [key, ptr] : m_Cache) {
            std::string msg = key + ", use_count = " + std::to_string(ptr.use_count()) + "\n";
            ResourceLoaderDetail::Trace(msg.c_str());
        }
    }

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<T>> m_Cache;
    AnyLoadFunc m_LoadFunc = nullptr;

    template<typename... Args>
    std::string CreateCacheKey(const std::string& path, const std::tuple<Args...>& args) {
        return path + ":" + TupleToString(args);
    }

    template<typename Tuple, std::size_t... I>
    std::string TupleToStringImpl(const Tuple& tuple, std::index_sequence<I...>) {
        std::ostringstream oss;
        ((oss << (I == 0 ? "" : ",") << std::get<I>(tuple)), ...);
        return oss.str();
    }

    template<typename... Args>
    std::string TupleToString(const std::tuple<Args...>& tuple) {
        try {
            return TupleToStringImpl(tuple, std::index_sequence_for<Args...>{});
        }
        catch (...) {
            return "TupleToStringError";
        }
    }
};
