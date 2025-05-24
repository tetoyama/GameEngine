// ModelLoader.h
#pragma once
class ModelLoader {
public:
    ModelLoader();
    ~ModelLoader();
    bool LoadModel(const std::string& modelPath);
};
