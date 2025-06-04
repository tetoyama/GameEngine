// ModelLoader.h
#pragma once

#include <string>
#include <unordered_map>
#include <memory>

class GraphicsContext;
struct ModelData;

class ModelLoader {
public:
	ModelLoader(GraphicsContext* set):m_GraphicContext(set){}

    ~ModelLoader() = default;
	ModelData* LoadModel(const std::string& modelPath);
	ModelData* GetModel(const std::string& modelPath);
private:
	std::unordered_map<std::string, std::shared_ptr<ModelData>> m_Models;
	GraphicsContext* m_GraphicContext;
};
