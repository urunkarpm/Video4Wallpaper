#include <windows.h>
#include "core/Logger.h"
#include "renderer/DesktopHost.h"
#include "renderer/D3D11Renderer.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Logger::LogInfo("WallpaperEngine initializing...");

    HWND hWorkerW = DesktopHost::GetWorkerWHandle();
    if (!hWorkerW) {
        Logger::LogError("Failed to get WorkerW window handle.");
        return 1;
    }

    HWND hWnd = DesktopHost::CreateWallpaperWindow(hInstance, hWorkerW);
    if (!hWnd) {
        Logger::LogError("Failed to create wallpaper window.");
        return 1;
    }

    D3D11Renderer renderer;
    if (!renderer.Initialize(hWnd)) {
        Logger::LogError("Failed to initialize D3D11Renderer.");
        return 1;
    }

    Logger::LogInfo("Running test render loop for 100 frames...");

    MSG msg = {};
    for (int frame = 0; frame < 100; ++frame) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        renderer.RenderTestFrame();
        Sleep(16); // ~60 FPS simulation
    }

    Logger::LogInfo("Test render loop complete. Cleaning up.");
    renderer.Cleanup();
    if (hWnd) {
        DestroyWindow(hWnd);
    }

    Logger::LogInfo("WallpaperEngine shutdown clean.");
    return 0;
}
