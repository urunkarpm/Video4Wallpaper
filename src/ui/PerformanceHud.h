#pragma once
#include <windows.h>
#include <string>

class RenderPipeline;
class PerformanceManager;

// ponytail: [Direct2D/GDI text overlay rendered via GDI/DirectWrite] -> [Full ImGui / Direct2D overlay with real-time frame time graph]

struct PerformanceStats {
    double currentFPS = 0.0;
    double targetFPS = 60.0;
    uint64_t frameCount = 0;
    uint64_t loopCount = 0;
    bool isPaused = false;
    bool isFullscreen = false;
    bool isOnBattery = false;
    std::string scalingModeStr = "Fill";
    size_t workingSetMB = 0;
};

class PerformanceHud {
public:
    PerformanceHud();
    ~PerformanceHud();

    void Render(HWND hWnd, const RenderPipeline* pipeline, const PerformanceManager* perfManager, int scalingMode);
    void SetVisible(bool visible) { m_visible = visible; }
    bool IsVisible() const { return m_visible; }

private:
    PerformanceStats GatherStats(const RenderPipeline* pipeline, const PerformanceManager* perfManager, int scalingMode);

    bool m_visible = false;
};
