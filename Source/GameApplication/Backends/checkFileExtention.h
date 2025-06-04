#pragma once
#include <iostream>
#include <string>

std::string GetFileExtension(const std::string& filePath){
	size_t dotPos = filePath.find_last_of('.');
	if(dotPos == std::string::npos) return "";
	return filePath.substr(dotPos);
}

bool HasExtension(const std::string& filePath, const std::string& extension){
	std::string ext = GetFileExtension(filePath);
	return ext == extension || ext == ("." + extension);
}

std::wstring GetFileExtension(const std::wstring& filePath){
	size_t dotPos = filePath.find_last_of('.');
	if(dotPos == std::wstring::npos) return L"";
	return filePath.substr(dotPos);
}

bool HasExtension(const std::wstring& filePath, const std::wstring& extension){
	std::wstring ext = GetFileExtension(filePath);
	return ext == extension || ext == (L"." + extension);
}
