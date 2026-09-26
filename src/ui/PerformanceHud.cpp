#include "ui/PerformanceHud.h"
#include "renderer/RenderPipeline.h"
#include "performance/PerformanceManager.h"
#include "core/Logger.h"
#include <psapi.h>

// ponytail: [Direct2D/GDI text overlay rendered via GDI/DirectWrite] -> [Full ImGui / Direct2D overlay with real-time frame time graph]

PerformanceHud::PerformanceHud() = default;
PerformanceHud::~PerformanceHud() = default;

PerformanceStats PerformanceHud::GatherStats(const RenderPipeline* pipeline, const PerformanceManager* perfManager, int scalingMode) {
    PerformanceStats stats;

    if (pipeline) {
        stats.currentFPS = pipeline->GetCurrentFPS();
        stats.frameCount = pipeline->GetFrameCount();
        stats.loopCount = pipeline->GetLoopCount();
        stats.isPaused = pipeline->IsPaused();
    }

    if (perfManager) {
        stats.isFullscreen = perfManager->IsFullscreen();
        stats.isOnBattery = perfManager->IsOnBattery();
    }

    switch (scalingMode) {
    case 0: stats.scalingModeStr = "Fill"; break;
    case 1: stats.scalingModeStr = "Fit"; break;
    case 2: stats.scalingModeStr = "Stretch"; break;
    case 3: stats.scalingModeStr = "Crop"; break;
    case 4: stats.scalingModeStr = "Original"; break;
    default: stats.scalingModeStr = "Fill"; break;
    }

    PROCESS_MEMORY_COUNTERS pmc = {};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        stats.workingSetMB = pmc.WorkingSetSize / (1024 * 1024);
    }

    return stats;
}

void PerformanceHud::Render(HWND hWnd, const RenderPipeline* pipeline, const PerformanceManager* perfManager, int scalingMode) {
    if (!m_visible || !hWnd) return;

    PerformanceStats stats = GatherStats(pipeline, perfManager, scalingMode);

    HDC hdc = GetDC(hWnd);
    if (!hdc) return;

    SetBkMode(hdc, TRANSPARENT);

    RECT rect = { 15, 15, 330, 135 };
    HBRUSH bgBrush = CreateSolidBrush(RGB(15, 15, 20));
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);

    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 255, 128));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);

    std::wstring line1 = L"FPS: " + std::to_wstring(static_cast<int>(stats.currentFPS * 10.0) / 10.0) + L" / " + std::to_wstring(static_cast<int>(stats.targetFPS));
    std::wstring line2 = L"Frames: " + std::to_wstring(stats.frameCount) + L" | Loops: " + std::to_wstring(stats.loopCount);
    std::wstring line3 = L"Status: " + std::wstring(stats.isPaused ? L"PAUSED" : L"RUNNING") +
                         L" (FS: " + (stats.isFullscreen ? L"YES" : L"NO") +
                         L" | Bat: " + (stats.isOnBattery ? L"YES" : L"NO") + L")";
    std::wstring line4 = L"Scaling: " + std::wstring(stats.scalingModeStr.begin(), stats.scalingModeStr.end());
    std::wstring line5 = L"RAM: " + std::to_wstring(stats.workingSetMB) + L" MB";

    std::wstring hudText = line1 + L"\n" + line2 + L"\n" + line3 + L"\n" + line4 + L"\n" + line5;

    SetTextColor(hdc, RGB(0, 255, 128));
    RECT textRect = { rect.left + 10, rect.top + 8, rect.right - 10, rect.bottom - 8 };
    DrawTextW(hdc, hudText.c_str(), -1, &textRect, DT_LEFT | DT_TOP);

    ReleaseDC(hWnd, hdc);
}
