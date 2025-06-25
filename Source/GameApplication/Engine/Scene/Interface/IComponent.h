// Engine/Scene/Interface/IComponent.h
#pragma once
#include <backends/yaml-cpp/yaml.h>
#include <backends/ImGui/imgui.h>

struct SceneContext;

class IComponent {
public:
	IComponent() = default;
	virtual ~IComponent() = default;

	virtual YAML::Node encode() = 0;

	virtual bool decode(const YAML::Node& node) = 0;

	virtual void inspector(SceneContext* context) = 0;
};
