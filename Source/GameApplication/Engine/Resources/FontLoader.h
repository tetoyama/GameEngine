// FontLoader.h
#pragma once
class FontLoader {
public:
    FontLoader();
    ~FontLoader();
    bool LoadFont(const std::wstring& fontPath);
};
