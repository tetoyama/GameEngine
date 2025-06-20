// Engine/Scene/Interface/IComponent.h
#pragma once
#include <backends/yaml-cpp/yaml.h>
class IComponent {
public:
	IComponent() = default;
	virtual ~IComponent() = default;

	virtual YAML::Node encode() = 0;

	virtual bool decode(const YAML::Node& node) = 0;
};
