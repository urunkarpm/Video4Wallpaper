#include "renderer/DesktopHost.h"
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

    HWND hWnd = CreateWindowExW(
        0,
        L"WallpaperEngineClass",
        L"Live Wallpaper Host",
        WS_POPUP,
        0, 0, cx, cy,
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
