//========================================================================
// Header file for file extension checking functions
//========================================================================
#pragma once
#include <iostream>
#include <string>

std::string GetFileExtension(const std::string& filePath); 

bool HasExtension(const std::string& filePath, const std::string& extension);

std::wstring GetFileExtension(const std::wstring& filePath);

bool HasExtension(const std::wstring& filePath, const std::wstring& extension);
