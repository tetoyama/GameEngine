
#pragma once
#include <memory>
#include <windows.h>
#include <unordered_map>
#include <typeindex>
#include <vector>
#include "Service/IService.h"

class EngineContext{

public:
	void Shutdown();

	template<typename T>
	void Register(std::shared_ptr<T> instance) {
		static_assert(std::is_base_of<IService, T>::value, "サービスはIServiceを継承してください");
		if (!instance) return;
		auto type = std::type_index(typeid(T));
		if (m_Services.count(type)) return;
		m_Services[type] = instance;
		m_ServiceOrder.push_back(type);
	}


	template <typename T>
	std::shared_ptr<T> Get() const;

private:
    std::unordered_map<std::type_index, std::shared_ptr<void>> m_Services;
    std::vector<std::type_index> m_ServiceOrder;
};

class EngineContextBuilder
{
public:
	std::shared_ptr<EngineContext> Build();
};

template<typename T>
inline std::shared_ptr<T> EngineContext::Get() const {
	auto it = m_Services.find(std::type_index(typeid(T)));
	if (it != m_Services.end()) {
		return std::static_pointer_cast<T>(it->second);
	}
	OutputDebugStringA("EngineContext:取得に失敗しました。\n");
	return nullptr;
}
