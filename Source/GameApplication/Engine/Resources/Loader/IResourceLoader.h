// =======================================================================
// 
// IResourceLoader.h
// 
// =======================================================================
#pragma once
#include <typeinfo>
#include <string>

class IResourceLoader {
public:
	virtual ~IResourceLoader() = default;

	virtual const std::type_info& GetType() const = 0;
	virtual void SetupLoadFunc(void* contextPtr) = 0;

	virtual void Unload(const std::string& path) = 0;
	virtual void ClearUnused() = 0;
	virtual void DumpCacheState() const = 0;
};
