#include "performance/PerformanceManager.h"
#include "monitor/MonitorManager.h"
#include "core/Logger.h"
#include <initguid.h>
#include <powersetting.h>
#include <powrprof.h>

#ifndef GUID_POWRMGMT_SESSION_CHANGE_STATUS
DEFINE_GUID(GUID_POWRMGMT_SESSION_CHANGE_STATUS, 0xf31b9fd1, 0x7ee2, 0x4972, 0xa3, 0xb7, 0x65, 0x1c, 0x64, 0x2e, 0x9c, 0x33);
#endif

// ponytail: [Polling Win32 foreground window and system power state] -> [RawInput + D3D11 swapchain occlusion state queries]

PerformanceManager* PerformanceManager::s_instance = nullptr;

PerformanceManager::PerformanceManager() {
}

PerformanceManager::~PerformanceManager() {
    Shutdown();
}

bool PerformanceManager::Initialize(HWND hWnd, PauseCallback callback) {
    if (!hWnd) {
        Logger::LogError("PerformanceManager::Initialize failed: Invalid HWND.");
        return false;
    }

    m_hWnd = hWnd;
    m_callback = callback;
    s_instance = this;

    // 1. Install event-based WinEvent hook for foreground window changes
    m_hWinEventHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        NULL,
        WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT
    );

    if (!m_hWinEventHook) {
        DWORD err = GetLastError();
        Logger::LogWarning("PerformanceManager: SetWinEventHook failed with error " + std::to_string(err));
    } else {
        Logger::LogInfo("PerformanceManager: Event-based WinEvent hook installed successfully.");
    }

    // 2. Register power setting notifications
    m_hPowerNotifySession = RegisterPowerSettingNotification(
        m_hWnd,
        &GUID_POWRMGMT_SESSION_CHANGE_STATUS,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );
    if (!m_hPowerNotifySession) {
        Logger::LogWarning("PerformanceManager: RegisterPowerSettingNotification for GUID_POWRMGMT_SESSION_CHANGE_STATUS failed.");
    }

    m_hPowerNotifyMonitor = RegisterPowerSettingNotification(
        m_hWnd,
        &GUID_MONITOR_POWER_ON,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );
    if (!m_hPowerNotifyMonitor) {
        Logger::LogWarning("PerformanceManager: RegisterPowerSettingNotification for GUID_MONITOR_POWER_ON failed.");
    }

    // 3. Register WTS session notification for lock/unlock
    if (!WTSRegisterSessionNotification(m_hWnd, NOTIFY_FOR_THIS_SESSION)) {
        Logger::LogWarning("PerformanceManager: WTSRegisterSessionNotification failed.");
    } else {
        Logger::LogInfo("PerformanceManager: WTS session notification registered successfully.");
    }

    // 4. Initial system power status check
    SYSTEM_POWER_STATUS sps = {};
    if (GetSystemPowerStatus(&sps)) {
        m_onBattery.store(sps.ACLineStatus == 0);
        Logger::LogInfo(std::string("PerformanceManager: Initial power state: ") + 
                        (m_onBattery.load() ? "Battery Power" : "AC Power"));
    }

    // 5. Initial foreground window fullscreen check
    CheckFullscreenState();

    // 6. Evaluate initial pause state
    EvaluatePauseState();

    Logger::LogInfo("PerformanceManager initialized successfully.");
    return true;
}

void PerformanceManager::Shutdown() {
    if (m_hWinEventHook) {
        UnhookWinEvent(m_hWinEventHook);
        m_hWinEventHook = NULL;
    }

    if (m_hPowerNotifySession) {
        UnregisterPowerSettingNotification(m_hPowerNotifySession);
        m_hPowerNotifySession = NULL;
    }

    if (m_hPowerNotifyMonitor) {
        UnregisterPowerSettingNotification(m_hPowerNotifyMonitor);
        m_hPowerNotifyMonitor = NULL;
    }

    if (m_hWnd) {
        WTSUnRegisterSessionNotification(m_hWnd);
        m_hWnd = NULL;
    }

    if (s_instance == this) {
        s_instance = nullptr;
    }

    m_callback = nullptr;
    Logger::LogInfo("PerformanceManager shutdown clean.");
}

void PerformanceManager::SetPauseOnFullscreen(bool enable) {
    m_pauseOnFullscreen.store(enable);
    EvaluatePauseState();
}

void PerformanceManager::SetPauseOnBattery(bool enable) {
    m_pauseOnBattery.store(enable);
    EvaluatePauseState();
}

std::string PerformanceManager::GetPauseReason() const {
    std::lock_guard<std::mutex> lock(m_reasonMutex);
    return m_currentReason;
}

void CALLBACK PerformanceManager::WinEventProc(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD dwEventThread,
    DWORD dwmsEventTime
) {
    if (event == EVENT_SYSTEM_FOREGROUND && s_instance) {
        s_instance->CheckFullscreenState();
    }
}

void PerformanceManager::CheckFullscreenState() {
    HWND fgHWnd = GetForegroundWindow();
    if (!fgHWnd || fgHWnd == m_hWnd || fgHWnd == GetDesktopWindow()) {
        SetFullscreenState(false);
        return;
    }

    wchar_t className[256] = {};
    if (GetClassNameW(fgHWnd, className, 256) > 0) {
        std::wstring cls(className);
        if (cls == L"Progman" || cls == L"WorkerW" || cls == L"Shell_TrayWnd" || 
            cls == L"Shell_SecondaryTrayWnd" || cls == L"ImmersiveLauncher" ||
            cls == L"Windows.UI.Core.CoreWindow") {
            SetFullscreenState(false);
            return;
        }
    }

    if (IsIconic(fgHWnd)) {
        SetFullscreenState(false);
        return;
    }

    RECT fgRect = {};
    if (!GetWindowRect(fgHWnd, &fgRect)) {
        SetFullscreenState(false);
        return;
    }

    HMONITOR hMon = MonitorFromWindow(fgHWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(MONITORINFO);
    if (GetMonitorInfoW(hMon, &mi)) {
        RECT monRect = mi.rcMonitor;
        if (fgRect.left <= monRect.left + 2 &&
            fgRect.top <= monRect.top + 2 &&
            fgRect.right >= monRect.right - 2 &&
            fgRect.bottom >= monRect.bottom - 2) {
            SetFullscreenState(true);
            return;
        }
    }

    SetFullscreenState(false);
}

void PerformanceManager::SetFullscreenState(bool fullscreen) {
    if (m_isFullscreen.exchange(fullscreen) != fullscreen) {
        Logger::LogInfo(std::string("PerformanceManager: Fullscreen state changed to ") + 
                        (fullscreen ? "TRUE" : "FALSE"));
        EvaluatePauseState();
    }
}

void PerformanceManager::SetBatteryState(bool onBattery) {
    if (m_onBattery.exchange(onBattery) != onBattery) {
        Logger::LogInfo(std::string("PerformanceManager: Power state changed to ") + 
                        (onBattery ? "BATTERY" : "AC"));
        EvaluatePauseState();
    }
}

void PerformanceManager::SetMonitorPowerOffState(bool powerOff) {
    if (m_monitorPowerOff.exchange(powerOff) != powerOff) {
        Logger::LogInfo(std::string("PerformanceManager: Monitor power off changed to ") + 
                        (powerOff ? "TRUE" : "FALSE"));
        EvaluatePauseState();
    }
}

void PerformanceManager::SetSessionLockedState(bool locked) {
    if (m_sessionLocked.exchange(locked) != locked) {
        Logger::LogInfo(std::string("PerformanceManager: Session locked changed to ") + 
                        (locked ? "TRUE" : "FALSE"));
        EvaluatePauseState();
    }
}

LRESULT PerformanceManager::HandleWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!s_instance) return 0;

    switch (message) {
    case WM_POWERBROADCAST: {
        if (wParam == PBT_APMPOWERSTATUSCHANGE) {
            SYSTEM_POWER_STATUS sps = {};
            if (GetSystemPowerStatus(&sps)) {
                s_instance->SetBatteryState(sps.ACLineStatus == 0);
            }
        } else if (wParam == PBT_POWERSETTINGCHANGE) {
            auto* pSetting = reinterpret_cast<const POWERBROADCAST_SETTING*>(lParam);
            if (pSetting) {
                if (IsEqualGUID(pSetting->PowerSetting, GUID_MONITOR_POWER_ON)) {
                    DWORD val = *reinterpret_cast<const DWORD*>(pSetting->Data);
                    s_instance->SetMonitorPowerOffState(val == 0);
                } else if (IsEqualGUID(pSetting->PowerSetting, GUID_POWRMGMT_SESSION_CHANGE_STATUS)) {
                    DWORD val = *reinterpret_cast<const DWORD*>(pSetting->Data);
                    s_instance->SetSessionLockedState(val != 0);
                }
            }
        }
        break;
    }
    case WM_WTSSESSION_CHANGE: {
        if (wParam == WTS_SESSION_LOCK) {
            s_instance->SetSessionLockedState(true);
        } else if (wParam == WTS_SESSION_UNLOCK) {
            s_instance->SetSessionLockedState(false);
        }
        break;
    }
    default:
        break;
    }

    return 0;
}

void PerformanceManager::EvaluatePauseState() {
    bool shouldPause = false;
    std::string reason = "";

    if (m_pauseOnFullscreen.load() && m_isFullscreen.load()) {
        shouldPause = true;
        reason = "Fullscreen application detected";
    } else if (m_pauseOnBattery.load() && m_onBattery.load()) {
        shouldPause = true;
        reason = "Running on battery power";
    } else if (m_monitorPowerOff.load()) {
        shouldPause = true;
        reason = "Monitor power off";
    } else if (m_sessionLocked.load()) {
        shouldPause = true;
        reason = "Windows session locked";
    }

    bool wasPaused = m_isPaused.exchange(shouldPause);
    
    std::lock_guard<std::mutex> lock(m_reasonMutex);
    bool reasonChanged = (m_currentReason != reason);
    m_currentReason = reason;

    if ((wasPaused != shouldPause) || (shouldPause && reasonChanged)) {
        Logger::LogInfo("PerformanceManager: EvaluatePauseState -> pause=" + 
                        std::string(shouldPause ? "TRUE" : "FALSE") + 
                        ", reason=\"" + reason + "\"");
        if (m_callback) {
            m_callback(shouldPause, reason);
        }
    }
}
