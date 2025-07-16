#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <typeinfo>
#include "IResourceLoader.h"

template<typename T>
class ResourceLoader : public IResourceLoader {
public:
	virtual ~ResourceLoader(){
		OutputDebugStringA("ResourceLoader destroyed\n");
		if(!m_Cache.empty()){
			DumpCacheState();
		}
	}

    // ロード関数型（引数付き）をテンプレートではなくSetLoadFunctionで設定
    using AnyLoadFunc = std::function<std::shared_ptr<T>(const std::string&, void*)>;

	template<typename... Args>
	std::shared_ptr<T> Load(const std::string& path, Args&&... args){
		using ArgsTuple = std::tuple<std::decay_t<Args>...>;
		ArgsTuple argsTuple(std::forward<Args>(args)...);

		// キーを path + 引数文字列に拡張
		std::string cacheKey = CreateCacheKey(path, argsTuple);

		auto it = m_Cache.find(cacheKey);
		if(it != m_Cache.end()) return it->second;

		if(!m_LoadFunc) return nullptr;

		void* extraArgs = static_cast<void*>(&argsTuple);

		auto result = m_LoadFunc(path, extraArgs);
		if(result){
			OutputDebugStringA("Load Succes: ");
			OutputDebugStringA(cacheKey.c_str());
			OutputDebugStringA("\n");
			m_Cache[cacheKey] = result;
		} else{
			OutputDebugStringA("Load FAILED: ");
			OutputDebugStringA(cacheKey.c_str());
			OutputDebugStringA("\n");
		}
		return result;
	}



    void SetLoadFunction(AnyLoadFunc func) {
        m_LoadFunc = func;
    }

    const std::type_info& GetType() const override {
        return typeid(T);
    }

    virtual void SetupLoadFunc(void* contextPtr) override {
        // 特殊化で実装
    }

	// path + args を使うアンロード（推奨）
	template<typename... Args>
	void Unload(const std::string& path, Args&&... args){
		using ArgsTuple = std::tuple<std::decay_t<Args>...>;
		ArgsTuple argsTuple(std::forward<Args>(args)...);

		std::string cacheKey = CreateCacheKey(path, argsTuple);

		auto it = m_Cache.find(cacheKey);
		if(it != m_Cache.end()){
			if(it->second.use_count() == 1){
				m_Cache.erase(it);
				OutputDebugStringA("Unloaded resource: ");
			} else{
				OutputDebugStringA("Cannot unload: still in use: ");
			}
			OutputDebugStringA(cacheKey.c_str());
			OutputDebugStringA("\n");
		}
	}

	// path だけで削除（関連すべて）
	void Unload(const std::string& path) override{
		for(auto it = m_Cache.begin(); it != m_Cache.end(); ){
			if(it->first.rfind(path + ":", 0) == 0){
				if(it->second.use_count() == 1){
					OutputDebugStringA(("Unloaded: " + it->first + "\n").c_str());
					it = m_Cache.erase(it);
				} else{
					OutputDebugStringA(("Still in use: " + it->first + "\n").c_str());
					++it;
				}
			} else{
				++it;
			}
		}
	}


    void ClearUnused() override {
        for (auto it = m_Cache.begin(); it != m_Cache.end();) {
            if (it->second.use_count() == 1)
                it = m_Cache.erase(it);
            else
                ++it;
        }
    }

	void DumpCacheState() const override {
		OutputDebugStringA("DumpCacheState\n");
		if(m_Cache.empty()){
			OutputDebugStringA("  Cache is empty\n");
		}
		for(const auto& [key, ptr] : m_Cache){
			std::string msg = key + ", use_count = " + std::to_string(ptr.use_count()) + "\n";
			OutputDebugStringA(msg.c_str());
		}
	}

private:
    std::unordered_map<std::string, std::shared_ptr<T>> m_Cache;
    AnyLoadFunc m_LoadFunc = nullptr;
    std::function<void(void*)> m_Deleter = nullptr;

	// 型特化せず汎用的に使えるユーティリティ
	template<typename... Args>
	std::string CreateCacheKey(const std::string& path, const std::tuple<Args...>& args){
		return path + ":" + TupleToString(args);
	}

	template<typename Tuple, std::size_t... I>
	std::string TupleToStringImpl(const Tuple& tuple, std::index_sequence<I...>){
		std::ostringstream oss;
		((oss << (I == 0 ? "" : ",") << std::get<I>(tuple)), ...);
		return oss.str();
	}

	template<typename... Args>
	std::string TupleToString(const std::tuple<Args...>& tuple){
		return TupleToStringImpl(tuple, std::index_sequence_for<Args...>{});
	}

};
