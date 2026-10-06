#pragma once

#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>
#include <windows.h>

#include "Service/IService.h"

template<typename T>
class ServiceRef {
public:
	ServiceRef() = default;
	explicit ServiceRef(T* value) noexcept : m_value(value){}

	T* get() const noexcept { return m_value; }
	T* operator->() const noexcept { return m_value; }
	explicit operator bool() const noexcept { return m_value != nullptr; }

private:
	T* m_value = nullptr;
};

class EngineContext {
public:
	EngineContext() = default;
	~EngineContext() = default;

	EngineContext(const EngineContext&) = delete;
	EngineContext& operator=(const EngineContext&) = delete;

	void Shutdown();

	template<typename T>
	T* Register(std::unique_ptr<T> instance){
		static_assert(std::is_base_of_v<IService, T>);
		if(!instance) return nullptr;

		const std::type_index type(typeid(T));
		if(m_services.contains(type)) return nullptr;

		T* result = instance.get();
		m_services.emplace(type, std::move(instance));
		m_serviceOrder.push_back(type);
		return result;
	}

	template<typename T, typename... Args>
	T* Emplace(Args&&... args){
		return Register<T>(std::make_unique<T>(std::forward<Args>(args)...));
	}

	template<typename T>
	ServiceRef<T> Get() const {
		auto iterator = m_services.find(std::type_index(typeid(T)));
		if(iterator == m_services.end()){
			OutputDebugStringA("EngineContext: service not registered.\n");
			return {};
		}
		return ServiceRef<T>(static_cast<T*>(iterator->second.get()));
	}

private:
	std::unordered_map<std::type_index, std::unique_ptr<IService>> m_services;
	std::vector<std::type_index> m_serviceOrder;
};

class EngineContextBuilder {
public:
	std::unique_ptr<EngineContext> Build();
};
