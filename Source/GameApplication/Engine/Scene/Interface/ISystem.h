#pragma once
#include <backends/yaml-cpp/yaml.h>

class ISystem
{
public:
	ISystem(){}
	virtual ~ISystem() {}

	virtual void Initialize() {}
	virtual void Finalize() {}
	
	virtual void Start() {}
	virtual void Update(float deltaTime) {}
	virtual void FixedUpdate(float fixedDeltaTime) {}
	virtual void Draw() {}
	virtual void SystemSetting() {}

	// SystemSetting() に有効な UI 実装があるかどうかを返す
	// オーバーライドしてシステム設定 UI を提供するシステムは true を返す
	virtual bool HasSystemSetting() const { return false; }

	virtual void EditorUpdate(float deltaTime){}
	
	virtual void Stop(){}

	virtual const char* GetSystemName() const{
		return nullptr;
	}

	virtual YAML::Node encode() { return YAML::Node(); }

	virtual bool decode(const YAML::Node& node) { return true; }
};
