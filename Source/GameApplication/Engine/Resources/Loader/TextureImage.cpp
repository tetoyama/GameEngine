#include "TextureImage.h"
#include <fstream>
#include <memory>
#include <stdexcept>
#include <cstring>

// Reuse the decoder already vendored with the engine. Private linkage avoids
// interfering with llama's decoder or its global image loading options.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_TGA
#define STBI_ONLY_BMP
#define STBI_MAX_DIMENSIONS 8192
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include "Backends/llama/vendor/stb/stb_image.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

TextureImage LoadTextureImage(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file) throw std::runtime_error("Cannot open texture image");
    const auto length=file.tellg();
    if(length<=0 || length>128*1024*1024) throw std::runtime_error("Invalid texture file size");
    std::vector<stbi_uc> bytes(static_cast<size_t>(length));
    file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),length)) throw std::runtime_error("Cannot read texture image");
    int width=0,height=0,channels=0;
    if(!stbi_info_from_memory(bytes.data(),static_cast<int>(bytes.size()),&width,&height,&channels) ||
        width<=0 || height<=0 || width>8192 || height>8192)
        throw std::runtime_error("Invalid or unsupported texture (PNG/JPEG/TGA/BMP required)");
    if(stbi_is_16_bit_from_memory(bytes.data(),static_cast<int>(bytes.size())))
        throw std::runtime_error("16-bit texture images are not supported by RGBA8 import");
    std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> decoded(
        stbi_load_from_memory(bytes.data(),static_cast<int>(bytes.size()),&width,&height,&channels,4),stbi_image_free);
    if(!decoded) throw std::runtime_error("Texture image decode failed");
    TextureImage image; image.width=static_cast<uint32_t>(width); image.height=static_cast<uint32_t>(height);
    image.pixels.resize(size_t(width)*height*4);
    std::memcpy(image.pixels.data(),decoded.get(),image.pixels.size());
    return image;
}
