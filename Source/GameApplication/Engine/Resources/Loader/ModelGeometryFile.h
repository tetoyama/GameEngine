#pragma once
#include "ModelGeometryImport.h"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <memory>
#include <stdexcept>
#include <string>

namespace ModelGeometryImport {
using SceneOwner=std::unique_ptr<const aiScene,decltype(&aiReleaseImport)>;
inline SceneOwner ImportScene(const std::string& path) {
    return SceneOwner(aiImportFile(path.c_str(),
        aiProcessPreset_TargetRealtime_MaxQuality|aiProcess_ConvertToLeftHanded),&aiReleaseImport);
}
inline std::vector<Geometry> LoadFile(const std::string& path,bool isBlender=false) {
    // Match the existing ModelLoader's import flags. Assimp owns the temporary
    // scene; the shared extraction produces owning CPU arrays before release.
    auto scene=ImportScene(path);
    if(!scene) throw std::runtime_error("Model import failed: "+std::string(aiGetErrorString()));
    std::vector<Geometry> geometry(scene->mNumMeshes);
    for(unsigned int index=0;index<scene->mNumMeshes;++index) {
        if(!scene->mMeshes[index] || scene->mMeshes[index]->HasBones())
            throw std::runtime_error("Native static geometry import does not support skinning");
        if(!ExtractMesh(*scene->mMeshes[index],isBlender,geometry[index]))
            throw std::runtime_error("Model contains invalid or non-triangle geometry");
    }
    if(geometry.empty()) throw std::runtime_error("Model contains no drawable geometry");
    return geometry;
}
}
