#pragma once
#include <shobjidl_core.h>
void InitTaskBar(HWND hwnd);
void SetTaskBarState(TBPFLAG tbpFlags, HWND hwnd = nullptr);
void SetTaskBarProgress(int Now, int Max = 100, HWND hwnd = nullptr);
