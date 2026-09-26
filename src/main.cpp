#include <windows.h>
#include <string>
#include <filesystem>
#include "core/Config.h"
#include "core/Logger.h"
#include "monitor/MonitorManager.h"
#include "performance/PerformanceManager.h"
#include "renderer/DesktopHost.h"
#include "renderer/D3D11Renderer.h"
#include "renderer/RenderPipeline.h"
#include "tray/SystemTray.h"
#include "ui/PerformanceHud.h"
#include "video/VideoDecoder.h"

namespace {
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], sizeNeeded, NULL, NULL);
    return strTo;
}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"WallpaperEngine_SingleInstance_Mutex_987654");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"WallpaperEngineClass", NULL);
        if (hExisting) {
            PostMessageW(hExisting, WM_COMMAND, ID_TRAY_SELECT_VIDEO, 0);
        }
        Logger::LogInfo("WallpaperEngine is already running. Signaled existing instance.");
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    Logger::LogInfo("WallpaperEngine initializing...");

    AppSettings settings;
    std::string configPath = "config.json";
    if (Config::Load(configPath, settings)) {
        Logger::LogInfo("Loaded existing configuration from " + configPath);
    } else {
        Logger::LogInfo("No existing config file found or load failed. Initializing defaults.");
    }

    if (__argc > 1 && __wargv[1]) {
        settings.wallpaperPath = __wargv[1];
        Logger::LogInfo("Wallpaper path specified via command-line argument.");
    }

    bool isTestRun = false;
    for (int i = 1; i < __argc; ++i) {
        if (__wargv[i] && std::wstring(__wargv[i]) == L"--test-run") {
            isTestRun = true;
            break;
        }
    }

    Config::Save(configPath, settings);

    auto monitors = MonitorManager::EnumerateMonitors();
    Logger::LogInfo("Monitor Enumeration Complete. Total active monitors: " + std::to_string(monitors.size()));

    RECT virtualBounds = MonitorManager::GetVirtualScreenBounds();
    Logger::LogInfo("Virtual Screen Bounds: [" + std::to_string(virtualBounds.left) + ", " + std::to_string(virtualBounds.top) +
                    " - " + std::to_string(virtualBounds.right - virtualBounds.left) + "x" + std::to_string(virtualBounds.bottom - virtualBounds.top) + "]");

    MonitorInfo primary = MonitorManager::GetPrimaryMonitor();
    Logger::LogInfo("Primary Monitor: Device=" + WStringToString(primary.deviceName) +
                    ", Resolution=" + std::to_string(primary.width) + "x" + std::to_string(primary.height) +
                    " @ " + std::to_string(primary.refreshRate) + "Hz, DPI=" + std::to_string(primary.dpi));

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

    SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&renderer));
    renderer.SetScalingMode(static_cast<ScalingMode>(settings.scalingMode));

    VideoDecoder decoder;
    if (!decoder.Initialize(renderer.GetDevice())) {
        Logger::LogError("Failed to initialize VideoDecoder.");
        return 1;
    }

    if (!settings.wallpaperPath.empty() && std::filesystem::exists(settings.wallpaperPath)) {
        if (decoder.OpenFile(settings.wallpaperPath)) {
            Logger::LogInfo("Opened configured video wallpaper: " + WStringToString(settings.wallpaperPath));
        } else {
            Logger::LogWarning("Failed to open video file. Running in procedural background mode.");
        }
    } else {
        Logger::LogInfo("No video specified or file missing. Click 'Select Video Wallpaper...' from System Tray to pick a video.");
    }

    RenderPipeline pipeline(&renderer, &decoder);
    pipeline.SetTargetFPS(settings.targetFPS);

    PerformanceManager perfManager;
    perfManager.SetPauseOnFullscreen(settings.pauseOnFullscreen);
    perfManager.SetPauseOnBattery(settings.pauseOnBattery);

    if (!perfManager.Initialize(hWnd, [&pipeline](bool pause, const std::string& reason) {
        if (pause) {
            Logger::LogInfo("PerformanceManager Callback: Auto-pausing RenderPipeline. Reason: " + reason);
            pipeline.Pause();
        } else {
            Logger::LogInfo("PerformanceManager Callback: Auto-resuming RenderPipeline.");
            pipeline.Resume();
        }
    })) {
        Logger::LogWarning("Failed to initialize PerformanceManager.");
    }

    PerformanceHud hud;
    hud.SetVisible(settings.showPerformanceHud);

    pipeline.SetPerformanceHud(&hud);
    pipeline.SetPerformanceManager(&perfManager);

    SystemTray tray;
    SystemTrayCallbacks callbacks;

    callbacks.onSelectVideo = [&decoder, &settings, &configPath, &pipeline, &tray](const std::wstring& selectedPath) {
        if (!selectedPath.empty() && std::filesystem::exists(selectedPath)) {
            Logger::LogInfo("User selected new video wallpaper: " + WStringToString(selectedPath));
            pipeline.Pause();
            if (decoder.OpenFile(selectedPath)) {
                settings.wallpaperPath = selectedPath;
                Config::Save(configPath, settings);
                pipeline.Resume();
                tray.SetIsPaused(false);
                Logger::LogInfo("New video wallpaper loaded and playing.");
            } else {
                Logger::LogError("Failed to load selected video wallpaper.");
                pipeline.Resume();
            }
        }
    };

    callbacks.onTogglePause = [&pipeline, &tray]() {
        if (pipeline.IsPaused()) {
            Logger::LogInfo("SystemTray Action: Resuming pipeline.");
            pipeline.Resume();
            tray.SetIsPaused(false);
        } else {
            Logger::LogInfo("SystemTray Action: Pausing pipeline.");
            pipeline.Pause();
            tray.SetIsPaused(true);
        }
    };

    callbacks.onToggleHud = [&hud, &settings, &tray, configPath]() {
        bool newVisible = !hud.IsVisible();
        hud.SetVisible(newVisible);
        settings.showPerformanceHud = newVisible;
        tray.SetIsHudVisible(newVisible);
        Config::Save(configPath, settings);
        Logger::LogInfo("SystemTray Action: Toggled Performance HUD to " + std::string(newVisible ? "ON" : "OFF"));
    };

    callbacks.onTogglePauseOnBattery = [&perfManager, &settings, &tray, configPath, &pipeline]() {
        bool newPauseOnBattery = !settings.pauseOnBattery;
        settings.pauseOnBattery = newPauseOnBattery;
        perfManager.SetPauseOnBattery(newPauseOnBattery);
        tray.SetPauseOnBattery(newPauseOnBattery);
        Config::Save(configPath, settings);
        if (!newPauseOnBattery && pipeline.IsPaused()) {
            pipeline.Resume();
            tray.SetIsPaused(false);
        }
        Logger::LogInfo("SystemTray Action: Toggled Pause on Battery to " + std::string(newPauseOnBattery ? "ON" : "OFF"));
    };

    callbacks.onChangeScalingMode = [&renderer, &settings, &tray, configPath](int mode) {
        renderer.SetScalingMode(static_cast<ScalingMode>(mode));
        settings.scalingMode = mode;
        tray.SetScalingMode(mode);
        Config::Save(configPath, settings);
        Logger::LogInfo("SystemTray Action: Changed scaling mode to " + std::to_string(mode));
    };

    callbacks.onExit = [&pipeline]() {
        Logger::LogInfo("SystemTray Action: Exit requested.");
        PostQuitMessage(0);
    };

    if (!tray.Initialize(hWnd, callbacks)) {
        Logger::LogWarning("Failed to initialize SystemTray.");
    }
    tray.SetIsPaused(pipeline.IsPaused());
    tray.SetIsHudVisible(hud.IsVisible());
    tray.SetPauseOnBattery(settings.pauseOnBattery);
    tray.SetScalingMode(settings.scalingMode);

    if (!pipeline.Start()) {
        Logger::LogError("Failed to start RenderPipeline.");
        return 1;
    }

    Logger::LogInfo("WallpaperEngine is running! Use System Tray icon to select video or adjust settings.");

    MSG msg = {};
    if (isTestRun) {
        bool pauseTested = false;
        while (pipeline.IsRunning()) {
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    pipeline.Stop();
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            uint64_t frameCount = pipeline.GetFrameCount();
            if (!pauseTested && frameCount >= 100) {
                pauseTested = true;
                pipeline.Pause();
                tray.SetIsPaused(true);
                Sleep(200);
                pipeline.Resume();
                tray.SetIsPaused(false);
            }

            if (frameCount >= 350) {
                break;
            }
            Sleep(5);
        }
    } else {
        // Continuous background operation until Exit
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    uint64_t totalFrames = pipeline.GetFrameCount();
    uint64_t totalLoops = pipeline.GetLoopCount();
    double finalFPS = pipeline.GetCurrentFPS();

    Logger::LogInfo("RenderPipeline complete summary: Total Frames = " + std::to_string(totalFrames) + 
                    ", Total Loops = " + std::to_string(totalLoops) + 
                    ", Final FPS = " + std::to_string(finalFPS));

    tray.Shutdown();
    perfManager.Shutdown();
    pipeline.Stop();
    decoder.Cleanup();
    renderer.Cleanup();

    if (hWnd) {
        DestroyWindow(hWnd);
    }

    if (hMutex) {
        CloseHandle(hMutex);
    }

    Logger::LogInfo("WallpaperEngine shutdown clean.");
    return 0;
}
