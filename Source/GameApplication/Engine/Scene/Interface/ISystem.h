#pragma once
#include <backends/yaml-cpp/yaml.h>

class ISystem
{
public:
	virtual ~ISystem() {}

	virtual void Initialize() {}
	virtual void Finalize() {}
	
	virtual void Start() {}
	virtual void Update(float deltaTime) {}
	virtual void FixedUpdate(float fixedDeltaTime) {}
	virtual void Draw() {}
	virtual void SystemSetting() {}

	virtual void EditorUpdate(float deltaTime){}
	
	virtual void Stop(){}

	virtual YAML::Node encode() { return YAML::Node(); }

	virtual bool decode(const YAML::Node& node) { return true; }

	virtual void systemSetting(){}
};
