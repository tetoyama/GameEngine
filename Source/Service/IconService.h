// IconService.h
#pragma once
class IconService {
public:
    IconService();
    ~IconService();
    bool LoadIcon(const std::wstring& path);
};
