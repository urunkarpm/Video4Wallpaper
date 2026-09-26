#pragma once
#include <windows.h>

// ponytail: [Basic Win32 Desktop Window Injection] -> [Multi-monitor virtual screen positioning and DPI-aware scaling manager]

class DesktopHost {
public:
    static HWND GetWorkerWHandle();
    static HWND CreateWallpaperWindow(HINSTANCE hInstance, HWND hWorkerW);
};
