// PrefabLoader.h
#pragma once
class PrefabLoader {
public:
    PrefabLoader();
    ~PrefabLoader();
    bool LoadPrefab(const std::string& prefabPath);
};
