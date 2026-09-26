#pragma once
#include <windows.h>
#include <thread>
#include <atomic>
#include <chrono>
#include "renderer/D3D11Renderer.h"
#include "video/VideoDecoder.h"

// ponytail: [High-precision waitable timer pacing] -> [DXGI Present flip-model with IDXGISwapChain3::GetFrameLatencyWaitableObject]
// ponytail: [Synchronous render thread decoding] -> [Dual-ring asynchronous decoder/presenter pipeline with frame interpolation]

class PerformanceHud;
class PerformanceManager;

class RenderPipeline {
public:
    RenderPipeline(D3D11Renderer* renderer, VideoDecoder* decoder);
    ~RenderPipeline();

    bool Start();
    void Stop();
    void Pause();
    void Resume();

    bool IsRunning() const { return m_running.load(); }
    bool IsPaused() const { return m_paused.load(); }
    uint64_t GetFrameCount() const { return m_frameCount.load(); }
    uint64_t GetLoopCount() const { return m_loopCount.load(); }
    double GetCurrentFPS() const { return m_currentFPS.load(); }
    void SetTargetFPS(double fps) { m_targetFPS = fps; }

    void SetPerformanceHud(PerformanceHud* hud) { m_hud = hud; }
    void SetPerformanceManager(const PerformanceManager* perfManager) { m_perfManager = perfManager; }

private:
    void RenderThreadProc();

    D3D11Renderer* m_renderer = nullptr;
    VideoDecoder* m_decoder = nullptr;
    PerformanceHud* m_hud = nullptr;
    const PerformanceManager* m_perfManager = nullptr;

    std::thread m_renderThread;
    std::atomic<bool> m_running{ false };
    std::atomic<bool> m_paused{ false };

    HANDLE m_timerHandle = NULL;
    HANDLE m_pauseEvent = NULL;

    std::atomic<uint64_t> m_frameCount{ 0 };
    std::atomic<uint64_t> m_loopCount{ 0 };
    std::atomic<double> m_currentFPS{ 0.0 };
    double m_targetFPS = 60.0;
};
