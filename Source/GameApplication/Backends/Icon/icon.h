#pragma once
#include <windows.h>
#include <string>

#define DEFAULT_ICON (L"asset\\texture\\icon.png")

HRESULT InitIcon(const HWND hWnd);
HRESULT SetIcon(const HWND hWnd, const std::wstring& FileName);
void UninitIcon();
