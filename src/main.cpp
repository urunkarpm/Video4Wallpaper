#include <windows.h>
#include <string>
#include <filesystem>
#include "core/Logger.h"
#include "renderer/DesktopHost.h"
#include "renderer/D3D11Renderer.h"
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
    if (!videoPath.empty()) {
        if (decoder.OpenFile(videoPath)) {
            hasVideo = true;
            Logger::LogInfo("Video playback engine initialized.");
        } else {
            Logger::LogWarning("Failed to open video file. Falling back to test renderer.");
        }
    } else {
        Logger::LogInfo("No video file path specified. Running in fallback test pattern mode.");
    }

    Logger::LogInfo("Running video render loop for 150 frames...");

    MSG msg = {};
    int totalFramesRendered = 0;
    int loopCount = 0;

    for (int frame = 0; frame < 150; ++frame) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (hasVideo) {
            DecodedFrame videoFrame = decoder.GetNextFrame();
            if (videoFrame.isEndOfStream || frame == 75) {
                loopCount++;
                Logger::LogInfo("Loop boundary reached (Loop #" + std::to_string(loopCount) + "). Rewinding seamlessly...");
                if (decoder.Rewind()) {
                    Logger::LogInfo("VideoDecoder::Rewind succeeded.");
                } else {
                    Logger::LogError("VideoDecoder::Rewind failed.");
                }
                videoFrame = decoder.GetNextFrame();
            }

            if (videoFrame.isHardwareAccelerated) {
                if (frame == 1 || frame == 76) {
                    Logger::LogInfo("Rendering hardware-accelerated DecodedFrame (PTS: " + std::to_string(videoFrame.timestamp) + ").");
                }
            }

            renderer.RenderVideoFrame(videoFrame);
        } else {
            renderer.RenderTestFrame();
        }

        totalFramesRendered++;
        Sleep(16); // ~60 FPS simulation
    }

    Logger::LogInfo("Video render loop complete. Total frames rendered: " + std::to_string(totalFramesRendered));
    
    decoder.Cleanup();
    renderer.Cleanup();

    if (hWnd) {
        DestroyWindow(hWnd);
    }

    Logger::LogInfo("WallpaperEngine shutdown clean.");
    return 0;
}
