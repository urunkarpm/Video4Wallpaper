#include "renderer/DesktopHost.h"
#include "renderer/D3D11Renderer.h"
#include "monitor/MonitorManager.h"
#include "performance/PerformanceManager.h"
#include "tray/SystemTray.h"
#include "core/Logger.h"

// ponytail: [Basic Win32 Desktop Window Injection] -> [Multi-monitor virtual screen positioning and DPI-aware scaling manager]

HWND DesktopHost::GetWorkerWHandle() {
    HWND hProgman = FindWindowW(L"Progman", NULL);
    if (!hProgman) {
        hProgman = GetShellWindow();
    }

    if (hProgman) {
        DWORD_PTR result = 0;
        // Send 0x052C to Progman to spawn a WorkerW window behind desktop icons
        SendMessageTimeoutW(hProgman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result);
    } else {
        Logger::LogWarning("Progman/Shell window not found. Attempting window enumeration fallback.");
    }

    HWND hWorkerW = NULL;
    EnumWindows([](HWND topHWnd, LPARAM lParam) -> BOOL {
        HWND hDefView = FindWindowExW(topHWnd, NULL, L"SHELLDLL_DefView", NULL);
        if (hDefView != NULL) {
            // Find the WorkerW window that immediately follows the window containing SHELLDLL_DefView
            HWND* pWorkerW = reinterpret_cast<HWND*>(lParam);
            *pWorkerW = FindWindowExW(NULL, topHWnd, L"WorkerW", NULL);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&hWorkerW));

    if (!hWorkerW) {
        if (hProgman) {
            Logger::LogWarning("WorkerW window not found via EnumWindows, using Progman handle as fallback.");
            hWorkerW = hProgman;
        } else {
            Logger::LogWarning("WorkerW window not found via EnumWindows, using Desktop window as fallback.");
            hWorkerW = GetDesktopWindow();
        }
    } else {
        Logger::LogInfo("WorkerW window handle successfully acquired.");
    }

    return hWorkerW;
}

LRESULT CALLBACK WallpaperWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED: {
        Logger::LogInfo("Display geometry or DPI change detected (msg=" + std::to_string(message) + ")");
        RECT bounds = MonitorManager::GetVirtualScreenBounds();
        UINT width = bounds.right - bounds.left;
        UINT height = bounds.bottom - bounds.top;
        SetWindowPos(hWnd, NULL, bounds.left, bounds.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);

        auto* renderer = reinterpret_cast<D3D11Renderer*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (renderer) {
            renderer->OnResize(width, height);
        }
        return 0;
    }
    case WM_SIZE: {
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        if (width > 0 && height > 0) {
            auto* renderer = reinterpret_cast<D3D11Renderer*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
            if (renderer) {
                renderer->OnResize(width, height);
            }
        }
        return 0;
    }
    case WM_POWERBROADCAST:
    case WM_WTSSESSION_CHANGE:
        PerformanceManager::HandleWindowMessage(hWnd, message, wParam, lParam);
        return 0;
    case WM_TRAYICON:
    case WM_COMMAND:
        SystemTray::HandleWindowMessage(hWnd, message, wParam, lParam);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
}

HWND DesktopHost::CreateWallpaperWindow(HINSTANCE hInstance, HWND hWorkerW) {
    Logger::LogInfo("DesktopHost::CreateWallpaperWindow starting...");
    HINSTANCE hInst = hInstance ? hInstance : GetModuleHandleW(NULL);

    WNDCLASSEXW wc = {};
    if (!GetClassInfoExW(hInst, L"WallpaperEngineClass", &wc)) {
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WallpaperWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = L"WallpaperEngineClass";

        if (!RegisterClassExW(&wc)) {
            DWORD err = GetLastError();
            Logger::LogError("Failed to register WallpaperEngineClass window class. Error: " + std::to_string(err));
            return NULL;
        }
    }

    Logger::LogInfo("Window class registered. Creating window...");

    RECT bounds = MonitorManager::GetVirtualScreenBounds();
    int x = bounds.left;
    int y = bounds.top;
    int cx = bounds.right - bounds.left;
    int cy = bounds.bottom - bounds.top;

    Logger::LogInfo("Wallpaper window virtual screen placement: [" + std::to_string(x) + ", " + std::to_string(y) + 
                    " - " + std::to_string(cx) + "x" + std::to_string(cy) + "]");

    HWND hWnd = CreateWindowExW(
        0,
        L"WallpaperEngineClass",
        L"Live Wallpaper Host",
        WS_POPUP,
        x, y, cx, cy,
        NULL,
        NULL,
        hInst,
        NULL
    );

    if (!hWnd) {
        DWORD err = GetLastError();
        Logger::LogError("Failed to create wallpaper window. Error: " + std::to_string(err));
        return NULL;
    }

    if (hWorkerW && hWorkerW != GetDesktopWindow()) {
        SetParent(hWnd, hWorkerW);
    }

    ShowWindow(hWnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hWnd);

    Logger::LogInfo("Wallpaper window created successfully.");
    return hWnd;
}
