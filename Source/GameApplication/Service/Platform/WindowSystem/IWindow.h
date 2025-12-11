#pragma once  
#include <Windows.h>  

class IWindow  
{  
public:  
   virtual ~IWindow() = default;  

   virtual bool Create(HINSTANCE hInstance, int nCmdShow) = 0;  
   virtual void PollEvents() = 0;  
   virtual HWND GetHWND() const = 0;  
   virtual bool ShouldClose() const = 0;  
   virtual UINT GetHeight() const = 0;  
   virtual UINT GetWidth() const = 0;  
};
