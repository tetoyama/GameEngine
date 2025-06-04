#pragma once
#include "IComponent.h"
#include <memory>

struct ModelData;

class ModelRendererComponent: public IComponent {
public:
	ModelRendererComponent() = default;
	~ModelRendererComponent() = default;

	std::shared_ptr<ModelData> model;
};
