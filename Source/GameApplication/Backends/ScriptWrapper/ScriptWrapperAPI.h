#pragma once

#ifdef SCRIPTWRAPPER_EXPORTS
#define SCRIPTWRAPPER_API __declspec(dllexport)
#else
#define SCRIPTWRAPPER_API __declspec(dllimport)
#endif

#include <string>

typedef void(*LogCallbackFn)(const std::string&);