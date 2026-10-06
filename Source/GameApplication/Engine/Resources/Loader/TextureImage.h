#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

// Owning CPU pixels, independent of a graphics API. Static 8-bit images are
// decoded in file order to RGBA; sampling/color-space policy stays with RHI.
struct TextureImage {
    uint32_t width=0,height=0;
    std::vector<std::byte> pixels;
    bool IsValid() const noexcept {
        return width && height && width<=8192 && height<=8192 &&
            pixels.size()==uint64_t(width)*height*4;
    }
};
TextureImage LoadTextureImage(const std::filesystem::path& path);
