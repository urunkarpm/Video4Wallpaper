#pragma once
#include <windows.h>
#include <vector>
#include <string>

// ponytail: [Win32 EnumDisplayMonitors API] -> [Per-monitor virtual desktop placement with DXGI desktop duplication and HDR color space metadata]

struct MonitorInfo {
    HMONITOR hMonitor = NULL;
    std::wstring deviceName;
    RECT rect{};
    UINT width = 0;
    UINT height = 0;
    UINT refreshRate = 60;
    UINT dpi = 96;
    bool isPrimary = false;
};

class MonitorManager {
public:
    static std::vector<MonitorInfo> EnumerateMonitors();
    static RECT GetVirtualScreenBounds();
    static MonitorInfo GetPrimaryMonitor();
};
