#include "monitor/MonitorManager.h"
#include "core/Logger.h"
#include <shellscalingapi.h>
#include <algorithm>

// ponytail: [Win32 EnumDisplayMonitors API] -> [Per-monitor virtual desktop placement with DXGI desktop duplication and HDR color space metadata]

namespace {
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], sizeNeeded, NULL, NULL);
    return strTo;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    auto* monitors = reinterpret_cast<std::vector<MonitorInfo>*>(dwData);

    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMonitor, &mi)) {
        MonitorInfo info;
        info.hMonitor = hMonitor;
        info.deviceName = mi.szDevice;
        info.rect = mi.rcMonitor;
        info.width = static_cast<UINT>(mi.rcMonitor.right - mi.rcMonitor.left);
        info.height = static_cast<UINT>(mi.rcMonitor.bottom - mi.rcMonitor.top);
        info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

        DEVMODEW devMode = {};
        devMode.dmSize = sizeof(devMode);
        if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &devMode)) {
            if (devMode.dmDisplayFrequency > 0) {
                info.refreshRate = devMode.dmDisplayFrequency;
            }
        }

        info.dpi = 96;
        typedef HRESULT(WINAPI* GetDpiForMonitorProc)(HMONITOR, int, UINT*, UINT*);
        HMODULE hShcore = LoadLibraryW(L"Shcore.dll");
        if (hShcore) {
            auto proc = reinterpret_cast<GetDpiForMonitorProc>(GetProcAddress(hShcore, "GetDpiForMonitor"));
            if (proc) {
                UINT dpiX = 96, dpiY = 96;
                if (SUCCEEDED(proc(hMonitor, 0, &dpiX, &dpiY))) {
                    info.dpi = dpiX;
                }
            }
            FreeLibrary(hShcore);
        }

        monitors->push_back(info);
    }
    return TRUE;
}
}

std::vector<MonitorInfo> MonitorManager::EnumerateMonitors() {
    std::vector<MonitorInfo> monitors;
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));

    if (monitors.empty()) {
        Logger::LogWarning("EnumDisplayMonitors returned 0 displays. Falling back to primary system metrics.");
        MonitorInfo info;
        info.hMonitor = NULL;
        info.deviceName = L"Primary Display (Fallback)";
        info.rect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        info.width = static_cast<UINT>(info.rect.right);
        info.height = static_cast<UINT>(info.rect.bottom);
        info.refreshRate = 60;
        info.dpi = 96;
        info.isPrimary = true;
        monitors.push_back(info);
    }

    for (const auto& mon : monitors) {
        Logger::LogInfo("Monitor Enumerated: Device=" + WStringToString(mon.deviceName) +
                        ", Primary=" + (mon.isPrimary ? "true" : "false") +
                        ", Bounds=[" + std::to_string(mon.rect.left) + "," + std::to_string(mon.rect.top) +
                        " - " + std::to_string(mon.width) + "x" + std::to_string(mon.height) + "]" +
                        ", RefreshRate=" + std::to_string(mon.refreshRate) + "Hz" +
                        ", DPI=" + std::to_string(mon.dpi));
    }

    return monitors;
}

RECT MonitorManager::GetVirtualScreenBounds() {
    auto monitors = EnumerateMonitors();
    if (!monitors.empty()) {
        LONG left = monitors[0].rect.left;
        LONG top = monitors[0].rect.top;
        LONG right = monitors[0].rect.right;
        LONG bottom = monitors[0].rect.bottom;

        for (size_t i = 1; i < monitors.size(); ++i) {
            left = (std::min)(left, monitors[i].rect.left);
            top = (std::min)(top, monitors[i].rect.top);
            right = (std::max)(right, monitors[i].rect.right);
            bottom = (std::max)(bottom, monitors[i].rect.bottom);
        }
        return RECT{ left, top, right, bottom };
    }

    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int cx = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int cy = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (cx <= 0 || cy <= 0) {
        cx = GetSystemMetrics(SM_CXSCREEN);
        cy = GetSystemMetrics(SM_CYSCREEN);
        x = 0;
        y = 0;
    }
    if (cx <= 0 || cy <= 0) {
        cx = 1920;
        cy = 1080;
    }

    return RECT{ x, y, x + cx, y + cy };
}

MonitorInfo MonitorManager::GetPrimaryMonitor() {
    auto monitors = EnumerateMonitors();
    for (const auto& mon : monitors) {
        if (mon.isPrimary) {
            return mon;
        }
    }
    if (!monitors.empty()) {
        return monitors[0];
    }

    MonitorInfo fallback;
    fallback.hMonitor = NULL;
    fallback.deviceName = L"Primary Display (Fallback)";
    fallback.rect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    fallback.width = static_cast<UINT>(fallback.rect.right);
    fallback.height = static_cast<UINT>(fallback.rect.bottom);
    fallback.refreshRate = 60;
    fallback.dpi = 96;
    fallback.isPrimary = true;
    return fallback;
}
