// =======================================================================
// 
// engineContext.h
// 
// =======================================================================

#pragma once
#include <memory>
#include <windows.h>
#include <unordered_map>
#include <typeindex>
#include <vector>
#include "Service/IService.h"

// サービスの依存注入コンテナ（DIコンテナ）
class EngineContext{

public:
	void Shutdown();

	// 指定した型のサービスをコンテナに登録する
	template<typename T>
	void Register(std::shared_ptr<T> instance) {
		static_assert(std::is_base_of<IService, T>::value, "サービスはIServiceを継承してください");
		if (!instance) return;
		auto type = std::type_index(typeid(T));
		if (m_Services.count(type)) return;
		m_Services[type] = instance;
		m_ServiceOrder.push_back(type);
	}

	// 指定した型のサービスをコンテナから取得する
	template<typename T>
	inline std::shared_ptr<T> Get() const{
		auto it = m_Services.find(std::type_index(typeid(T)));
		if(it != m_Services.end()){
			return std::static_pointer_cast<T>(it->second);
		}
		OutputDebugStringA("EngineContext:取得に失敗しました。\n");
		return nullptr;
	}

private:
    std::unordered_map<std::type_index, std::shared_ptr<IService>> m_Services;
    std::vector<std::type_index> m_ServiceOrder;
};

// EngineContextの構築を行うビルダークラス
class EngineContextBuilder
{
public:
	std::shared_ptr<EngineContext> Build();
};


