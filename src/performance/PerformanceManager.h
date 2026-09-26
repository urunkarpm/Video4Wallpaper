#pragma once
#include <windows.h>
#include <wtsapi32.h>
#include <functional>
#include <string>
#include <mutex>
#include <atomic>

// ponytail: [Polling Win32 foreground window and system power state] -> [RawInput + D3D11 swapchain occlusion state queries]

using PauseCallback = std::function<void(bool pause, const std::string& reason)>;

class PerformanceManager {
public:
    PerformanceManager();
    ~PerformanceManager();

    bool Initialize(HWND hWnd, PauseCallback callback);
    void Shutdown();

    void SetPauseOnFullscreen(bool enable);
    void SetPauseOnBattery(bool enable);
    bool GetPauseOnFullscreen() const { return m_pauseOnFullscreen.load(); }
    bool GetPauseOnBattery() const { return m_pauseOnBattery.load(); }

    bool IsPaused() const { return m_isPaused.load(); }
    bool IsFullscreen() const { return m_isFullscreen.load(); }
    bool IsOnBattery() const { return m_onBattery.load(); }
    bool IsMonitorPowerOff() const { return m_monitorPowerOff.load(); }
    bool IsSessionLocked() const { return m_sessionLocked.load(); }
    std::string GetPauseReason() const;

    void CheckFullscreenState();
    static LRESULT HandleWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    static PerformanceManager* GetInstance() { return s_instance; }

private:
    static void CALLBACK WinEventProc(
        HWINEVENTHOOK hWinEventHook,
        DWORD event,
        HWND hwnd,
        LONG idObject,
        LONG idChild,
        DWORD dwEventThread,
        DWORD dwmsEventTime
    );

    void EvaluatePauseState();
    void SetFullscreenState(bool fullscreen);
    void SetBatteryState(bool onBattery);
    void SetMonitorPowerOffState(bool powerOff);
    void SetSessionLockedState(bool locked);

    HWND m_hWnd = NULL;
    PauseCallback m_callback;
    HWINEVENTHOOK m_hWinEventHook = NULL;
    HPOWERNOTIFY m_hPowerNotifySession = NULL;
    HPOWERNOTIFY m_hPowerNotifyMonitor = NULL;

    std::atomic<bool> m_pauseOnFullscreen{ true };
    std::atomic<bool> m_pauseOnBattery{ true };

    std::atomic<bool> m_isFullscreen{ false };
    std::atomic<bool> m_onBattery{ false };
    std::atomic<bool> m_monitorPowerOff{ false };
    std::atomic<bool> m_sessionLocked{ false };
    std::atomic<bool> m_isPaused{ false };

    mutable std::mutex m_reasonMutex;
    std::string m_currentReason;

    static PerformanceManager* s_instance;
};
