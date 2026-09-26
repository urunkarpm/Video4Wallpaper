#pragma once
#include <windows.h>
#include <shellapi.h>
#include <functional>
#include <string>

// ponytail: [Basic Win32 Shell_NotifyIconW System Tray] -> [Modern Windows AppNotification / Toast notification & WPF/WinUI tray controller]

#define WM_TRAYICON (WM_USER + 1)

enum SystemTrayCommand {
    ID_TRAY_SELECT_VIDEO = 1000,
    ID_TRAY_PAUSE_RESUME = 1001,
    ID_TRAY_TOGGLE_HUD   = 1002,
    ID_TRAY_PAUSE_BATTERY= 1003,
    ID_TRAY_AUTOSTART    = 1005,
    ID_TRAY_SCALING_FILL = 1010,
    ID_TRAY_SCALING_FIT  = 1011,
    ID_TRAY_SCALING_STRETCH = 1012,
    ID_TRAY_SCALING_CROP = 1013,
    ID_TRAY_SCALING_ORIGINAL = 1014,
    ID_TRAY_EXIT         = 1020
};

struct SystemTrayCallbacks {
    std::function<void(const std::wstring& path)> onSelectVideo;
    std::function<void()> onTogglePause;
    std::function<void()> onToggleHud;
    std::function<void()> onTogglePauseOnBattery;
    std::function<void(int scalingMode)> onChangeScalingMode;
    std::function<void()> onExit;
};

class SystemTray {
public:
    SystemTray();
    ~SystemTray();

    bool Initialize(HWND hWnd, SystemTrayCallbacks callbacks);
    void Shutdown();

    void ShowContextMenu(HWND hWnd);
    static std::wstring PromptSelectVideoFile(HWND hWnd);
    static bool IsAutostartEnabled();
    static bool SetAutostartEnabled(bool enable);

    void SetIsPaused(bool paused) { m_isPaused = paused; }
    void SetIsHudVisible(bool visible) { m_isHudVisible = visible; }
    void SetPauseOnBattery(bool pauseOnBattery) { m_pauseOnBattery = pauseOnBattery; }
    void SetScalingMode(int mode) { m_currentScalingMode = mode; }

    static LRESULT HandleWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static SystemTray* GetInstance() { return s_instance; }

private:
    HWND m_hWnd = NULL;
    NOTIFYICONDATAW m_nid = {};
    bool m_initialized = false;

    bool m_isPaused = false;
    bool m_isHudVisible = false;
    bool m_pauseOnBattery = false;
    int m_currentScalingMode = 0;

    SystemTrayCallbacks m_callbacks;

    static SystemTray* s_instance;
};
