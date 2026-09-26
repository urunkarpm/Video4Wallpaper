#include <windows.h>
#include <string>
#include <filesystem>
#include "core/Logger.h"
#include "renderer/DesktopHost.h"
#include "renderer/D3D11Renderer.h"
#include "renderer/RenderPipeline.h"
#include "video/VideoDecoder.h"

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

    VideoDecoder decoder;
    if (!decoder.Initialize(renderer.GetDevice())) {
        Logger::LogError("Failed to initialize VideoDecoder.");
        return 1;
    }

    std::wstring videoPath;
    if (__argc > 1 && __wargv[1]) {
        videoPath = __wargv[1];
    } else {
        std::wstring samplePath = L"C:\\Users\\uprasenjeet\\Videos\\Screen Recordings\\Screen Recording 2026-09-06 123705.mp4";
        if (std::filesystem::exists(samplePath)) {
            videoPath = samplePath;
        }
    }

    bool hasVideo = false;
    if (!videoPath.empty() && std::filesystem::exists(videoPath)) {
        if (decoder.OpenFile(videoPath)) {
            hasVideo = true;
            Logger::LogInfo("Video playback engine initialized.");
        } else {
            Logger::LogWarning("Failed to open video file. Falling back to test renderer.");
        }
    } else {
        Logger::LogInfo("No valid video file path specified or found. Running in fallback test pattern mode.");
    }

    RenderPipeline pipeline(&renderer, &decoder);
    if (!pipeline.Start()) {
        Logger::LogError("Failed to start RenderPipeline.");
        return 1;
    }

    Logger::LogInfo("RenderPipeline started. Running high-precision waitable timer loop for 350+ frames...");

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

        // Automated pause/resume validation around frame 100
        if (!pauseTested && frameCount >= 100) {
            pauseTested = true;
            Logger::LogInfo("Testing RenderPipeline Pause mechanism at frame count: " + std::to_string(frameCount));
            pipeline.Pause();
            
            // Sleep for 300ms in WinMain to verify zero-waste idle behavior on render thread
            Sleep(300);

            Logger::LogInfo("Testing RenderPipeline Resume mechanism.");
            pipeline.Resume();
        }

        // Run until at least 350 frames rendered
        if (frameCount >= 350) {
            Logger::LogInfo("Target frame count reached (350+ frames). Initiating clean shutdown.");
            break;
        }

        Sleep(5); // Main thread yield to stay responsive to UI/win32 messages
    }

    uint64_t totalFrames = pipeline.GetFrameCount();
    uint64_t totalLoops = pipeline.GetLoopCount();
    double finalFPS = pipeline.GetCurrentFPS();

    Logger::LogInfo("RenderPipeline complete summary: Total Frames = " + std::to_string(totalFrames) + 
                    ", Total Loops = " + std::to_string(totalLoops) + 
                    ", Final FPS = " + std::to_string(finalFPS));

    pipeline.Stop();
    decoder.Cleanup();
    renderer.Cleanup();

    if (hWnd) {
        DestroyWindow(hWnd);
    }

    Logger::LogInfo("WallpaperEngine shutdown clean.");
    return 0;
}
