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

    if (settings.wallpaperPath.empty()) {
        std::wstring samplePath = L"C:\\Users\\uprasenjeet\\Videos\\Screen Recordings\\Screen Recording 2026-09-06 123705.mp4";
        if (std::filesystem::exists(samplePath)) {
            settings.wallpaperPath = samplePath;
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

    bool hasVideo = false;
    if (!settings.wallpaperPath.empty() && std::filesystem::exists(settings.wallpaperPath)) {
        if (decoder.OpenFile(settings.wallpaperPath)) {
            hasVideo = true;
            Logger::LogInfo("Video playback engine initialized.");
        } else {
            Logger::LogWarning("Failed to open video file. Falling back to test renderer.");
        }
    } else {
        Logger::LogInfo("No valid video file path specified or found. Running in fallback test pattern mode.");
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
    tray.SetScalingMode(settings.scalingMode);

    if (!pipeline.Start()) {
        Logger::LogError("Failed to start RenderPipeline.");
        return 1;
    }

    Logger::LogInfo("RenderPipeline, SystemTray, and PerformanceHud active. Running message loop for 350+ frames...");

    MSG msg = {};
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

        // Automated validation around frame 100
        if (!pauseTested && frameCount >= 100) {
            pauseTested = true;
            Logger::LogInfo("Testing RenderPipeline Pause & HUD toggle at frame count: " + std::to_string(frameCount));
            pipeline.Pause();
            tray.SetIsPaused(true);

            hud.SetVisible(true);
            settings.showPerformanceHud = true;
            tray.SetIsHudVisible(true);
            Config::Save(configPath, settings);

            Sleep(300);

            Logger::LogInfo("Testing RenderPipeline Resume mechanism.");
            pipeline.Resume();
            tray.SetIsPaused(false);
        }

        if (frameCount >= 350) {
            Logger::LogInfo("Target frame count reached (350+ frames). Initiating clean shutdown.");
            break;
        }

        Sleep(5);
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

    Logger::LogInfo("WallpaperEngine shutdown clean.");
    return 0;
}
