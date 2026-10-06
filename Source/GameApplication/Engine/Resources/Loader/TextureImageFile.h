#pragma once
#include <memory>
#include <filesystem>
#include <stdexcept>
#include "Engine/Resources/Data/textureData.h"
#include "TextureImage.h"

// Keep native file loading in the existing TextureData ownership boundary.
inline std::shared_ptr<TextureData> LoadTextureFromFile(const std::filesystem::path& path,RHI::IRHIDevice& device) {
    auto tex=std::make_shared<TextureData>();
    const auto utf8=path.u8string(); tex->FilePath.assign(utf8.begin(),utf8.end());
    const auto image=LoadTextureImage(path);
    if(!tex->EnsureRHI(device,image)) throw std::runtime_error("Texture GPU upload failed");
    return tex;
}
