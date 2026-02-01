/// Description: This file contains functions to check and retrieve file extensions from file paths.

#include "checkFileExtention.h"

// Function to get the file extension from a given file path (std::string version)
std::string GetFileExtension(const std::string& filePath){
	size_t dotPos = filePath.find_last_of('.');
	if(dotPos == std::string::npos) return "";
	return filePath.substr(dotPos);
}

// Function to check if a file has a specific extension (std::string version)
bool HasExtension(const std::string& filePath, const std::string& extension){
	std::string ext = GetFileExtension(filePath);
	return ext == extension || ext == ("." + extension);
}

// Function to get the file extension from a given file path (std::wstring version)
std::wstring GetFileExtension(const std::wstring& filePath){
	size_t dotPos = filePath.find_last_of('.');
	if(dotPos == std::wstring::npos) return L"";
	return filePath.substr(dotPos);
}

// Function to check if a file has a specific extension (std::wstring version)
bool HasExtension(const std::wstring& filePath, const std::wstring& extension){
	std::wstring ext = GetFileExtension(filePath);
	return ext == extension || ext == (L"." + extension);
}
