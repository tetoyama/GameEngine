
#pragma once
#include <memory>
#include <windows.h>
#include <unordered_map>
#include <typeindex>

class EngineContext{

public:
	template<typename T>
	void Register(std::shared_ptr<T> instance){
		if(!instance){
			OutputDebugStringA("ƒT[ƒrƒX“o˜^‚Énullptr‚ª“n‚³‚ê‚Ü‚µ‚½B\n");
			return;
		}
		auto type = std::type_index(typeid(T));
		if(m_Services.count(type)){
			OutputDebugStringA("“¯‚¶Œ^‚ÌƒT[ƒrƒX‚ªŠù‚É“o˜^‚³‚ê‚Ä‚¢‚Ü‚·B\n");
			return;
		}
		m_Services[type] = instance;
	}

	template <typename T>
	std::shared_ptr<T> Get() const{
		auto it = m_Services.find(std::type_index(typeid(T)));
		if(it != m_Services.end()){
			return std::static_pointer_cast<T>(it->second);
		}
		return nullptr;
	}

private:
	std::unordered_map<std::type_index, std::shared_ptr<void>> m_Services;
};

class EngineContextBuilder
{
public:
	std::shared_ptr<EngineContext> Build(HINSTANCE hInstance, int nCmdShow);
};