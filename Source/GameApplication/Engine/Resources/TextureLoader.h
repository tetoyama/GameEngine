// TextureLoader.h
#pragma once
class TextureLoader {
public:
    TextureLoader();
    ~TextureLoader();
    bool LoadTexture(const std::string& texturePath);
};
