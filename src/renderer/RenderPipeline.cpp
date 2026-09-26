#include "renderer/RenderPipeline.h"
#include "core/Logger.h"
#include <sstream>
#include <iomanip>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

// ponytail: [High-precision waitable timer pacing] -> [DXGI Present flip-model with IDXGISwapChain3::GetFrameLatencyWaitableObject]
// ponytail: [Synchronous render thread decoding] -> [Dual-ring asynchronous decoder/presenter pipeline with frame interpolation]

RenderPipeline::RenderPipeline(D3D11Renderer* renderer, VideoDecoder* decoder)
    : m_renderer(renderer), m_decoder(decoder) {
}

RenderPipeline::~RenderPipeline() {
    Stop();
}

bool RenderPipeline::Start() {
    if (m_running.load()) {
        Logger::LogWarning("RenderPipeline::Start called while already running.");
        return true;
    }

    // High-precision waitable timer creation
    m_timerHandle = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!m_timerHandle) {
        DWORD err = GetLastError();
        Logger::LogWarning("CreateWaitableTimerExW with HIGH_RESOLUTION failed (err " + std::to_string(err) + "). Falling back to standard waitable timer.");
        m_timerHandle = CreateWaitableTimerW(NULL, FALSE, NULL);
        if (!m_timerHandle) {
            Logger::LogError("CreateWaitableTimerW failed.");
            return false;
        }
    } else {
        Logger::LogInfo("High-resolution waitable timer created successfully.");
    }

    m_pauseEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!m_pauseEvent) {
        Logger::LogError("CreateEventW failed for m_pauseEvent.");
        if (m_timerHandle) {
            CloseHandle(m_timerHandle);
            m_timerHandle = NULL;
        }
        return false;
    }

    m_running.store(true);
    m_paused.store(false);
    m_frameCount.store(0);
    m_loopCount.store(0);

    m_renderThread = std::thread(&RenderPipeline::RenderThreadProc, this);
    Logger::LogInfo("RenderPipeline background thread started successfully.");
    return true;
}

void RenderPipeline::Stop() {
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);
    if (m_pauseEvent) {
        SetEvent(m_pauseEvent);
    }
    if (m_timerHandle) {
        CancelWaitableTimer(m_timerHandle);
    }

    if (m_renderThread.joinable()) {
        m_renderThread.join();
    }

    if (m_pauseEvent) {
        CloseHandle(m_pauseEvent);
        m_pauseEvent = NULL;
    }
    if (m_timerHandle) {
        CloseHandle(m_timerHandle);
        m_timerHandle = NULL;
    }

    Logger::LogInfo("RenderPipeline background thread stopped cleanly.");
}

void RenderPipeline::Pause() {
    if (!m_paused.load()) {
        m_paused.store(true);
        if (m_pauseEvent) {
            SetEvent(m_pauseEvent);
        }
        Logger::LogInfo("RenderPipeline state changed to PAUSED.");
    }
}

void RenderPipeline::Resume() {
    if (m_paused.load()) {
        m_paused.store(false);
        if (m_pauseEvent) {
            SetEvent(m_pauseEvent);
        }
        Logger::LogInfo("RenderPipeline state changed to RESUMED.");
    }
}

void RenderPipeline::RenderThreadProc() {
    Logger::LogInfo("Render thread procedure active.");

    auto lastFpsTime = std::chrono::high_resolution_clock::now();
    uint64_t fpsFrameCounter = 0;
    LONGLONG lastVideoTimestamp = -1;

    HANDLE waitHandles[2] = { m_pauseEvent, m_timerHandle };

    while (m_running.load()) {
        // Zero-waste presentation loop pause check
        if (m_paused.load()) {
            WaitForSingleObject(m_pauseEvent, 50);
            continue;
        }

        auto frameStartTime = std::chrono::high_resolution_clock::now();

        bool renderedFrame = false;
        LONGLONG frameDuration100ns = 0;

        if (m_decoder && m_decoder->IsFileOpen()) {
            DecodedFrame frame = m_decoder->GetNextFrame();

            if (frame.isEndOfStream) {
                m_loopCount++;
                Logger::LogInfo("End of stream reached. Rewinding video (Loop #" + std::to_string(m_loopCount.load()) + ").");
                if (m_decoder->Rewind()) {
                    lastVideoTimestamp = -1;
                    frame = m_decoder->GetNextFrame();
                }
            }

            if (frame.texture && frame.isHardwareAccelerated) {
                if (frame.timestamp != lastVideoTimestamp || frame.timestamp == 0) {
                    m_renderer->RenderVideoFrame(frame);
                    lastVideoTimestamp = frame.timestamp;
                    renderedFrame = true;
                }
                frameDuration100ns = frame.duration;
            } else {
                m_renderer->RenderTestFrame();
                renderedFrame = true;
            }
        } else {
            m_renderer->RenderTestFrame();
            renderedFrame = true;
        }

        if (renderedFrame) {
            m_frameCount++;
            fpsFrameCounter++;
        }

        // FPS calculation
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> fpsElapsed = now - lastFpsTime;
        if (fpsElapsed.count() >= 1.0) {
            double fps = static_cast<double>(fpsFrameCounter) / fpsElapsed.count();
            m_currentFPS.store(fps);
            Logger::LogInfo("RenderPipeline Stats: FPS = " + std::to_string(fps) + 
                            ", Total Frames = " + std::to_string(m_frameCount.load()) + 
                            ", Loops = " + std::to_string(m_loopCount.load()));
            fpsFrameCounter = 0;
            lastFpsTime = now;
        }

        // High-precision frame pacing via waitable timer
        double targetFrameSec = 1.0 / m_targetFPS;
        if (frameDuration100ns > 0) {
            targetFrameSec = static_cast<double>(frameDuration100ns) / 10000000.0;
        }

        auto frameWorkTime = std::chrono::high_resolution_clock::now() - frameStartTime;
        double frameWorkSec = std::chrono::duration<double>(frameWorkTime).count();

        double sleepSec = targetFrameSec - frameWorkSec;
        if (sleepSec > 0.0005) { // sleep if > 0.5 ms remaining
            LARGE_INTEGER liDueTime;
            liDueTime.QuadPart = -static_cast<LONGLONG>(sleepSec * 10000000.0);
            
            SetWaitableTimer(m_timerHandle, &liDueTime, 0, NULL, NULL, FALSE);
            WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        }
    }

    Logger::LogInfo("Render thread procedure exiting.");
}
