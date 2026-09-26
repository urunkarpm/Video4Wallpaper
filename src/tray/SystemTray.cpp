#include "tray/SystemTray.h"
#include "core/Logger.h"
#include <commdlg.h>
#include <filesystem>

// ponytail: [Basic Win32 Shell_NotifyIconW System Tray] -> [Modern Windows AppNotification / Toast notification & WPF/WinUI tray controller]

SystemTray* SystemTray::s_instance = nullptr;

SystemTray::SystemTray() {
    s_instance = this;
}

SystemTray::~SystemTray() {
    Shutdown();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

bool SystemTray::Initialize(HWND hWnd, SystemTrayCallbacks callbacks) {
    m_hWnd = hWnd;
    m_callbacks = callbacks;

    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = hWnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    wcscpy_s(m_nid.szTip, L"WallpaperEngine");

    if (!Shell_NotifyIconW(NIM_ADD, &m_nid)) {
        Logger::LogError("Failed to add system tray icon.");
        return false;
    }

    m_initialized = true;
    Logger::LogInfo("SystemTray icon initialized successfully.");
    return true;
}

void SystemTray::Shutdown() {
    if (m_initialized) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_initialized = false;
        Logger::LogInfo("SystemTray icon removed.");
    }
}

bool SystemTray::IsAutostartEnabled() {
    HKEY hKey = NULL;
    LONG res = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey);
    if (res != ERROR_SUCCESS) return false;

    wchar_t valBuf[MAX_PATH] = {};
    DWORD valSize = sizeof(valBuf);
    res = RegQueryValueExW(hKey, L"WallpaperEngine", NULL, NULL, reinterpret_cast<LPBYTE>(valBuf), &valSize);
    RegCloseKey(hKey);

    return (res == ERROR_SUCCESS);
}

bool SystemTray::SetAutostartEnabled(bool enable) {
    HKEY hKey = NULL;
    LONG res = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey);
    if (res != ERROR_SUCCESS) return false;

    if (enable) {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring valStr = L"\"" + std::wstring(exePath) + L"\"";
        res = RegSetValueExW(hKey, L"WallpaperEngine", 0, REG_SZ, reinterpret_cast<const BYTE*>(valStr.c_str()), static_cast<DWORD>((valStr.length() + 1) * sizeof(wchar_t)));
        Logger::LogInfo("SystemTray: Enabled Windows Autostart.");
    } else {
        res = RegDeleteValueW(hKey, L"WallpaperEngine");
        Logger::LogInfo("SystemTray: Disabled Windows Autostart.");
    }

    RegCloseKey(hKey);
    return (res == ERROR_SUCCESS);
}

std::wstring SystemTray::PromptSelectVideoFile(HWND hWnd) {
    wchar_t szFile[MAX_PATH] = { 0 };

    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"Video Files (*.mp4;*.mkv;*.webm;*.mov;*.avi;*.wmv)\0*.mp4;*.mkv;*.webm;*.mov;*.avi;*.wmv\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = L"Select Video Wallpaper";

    if (GetOpenFileNameW(&ofn)) {
        return std::wstring(szFile);
    }
    return L"";
}

void SystemTray::ShowContextMenu(HWND hWnd) {
    HMENU hMenu = CreatePopupMenu();
    HMENU hSubMenuScaling = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SELECT_VIDEO, L"🎬 Select Video Wallpaper...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hSubMenuScaling, MF_STRING | (m_currentScalingMode == 0 ? MF_CHECKED : 0), ID_TRAY_SCALING_FILL, L"Fill");
    AppendMenuW(hSubMenuScaling, MF_STRING | (m_currentScalingMode == 1 ? MF_CHECKED : 0), ID_TRAY_SCALING_FIT, L"Fit");
    AppendMenuW(hSubMenuScaling, MF_STRING | (m_currentScalingMode == 2 ? MF_CHECKED : 0), ID_TRAY_SCALING_STRETCH, L"Stretch");
    AppendMenuW(hSubMenuScaling, MF_STRING | (m_currentScalingMode == 3 ? MF_CHECKED : 0), ID_TRAY_SCALING_CROP, L"Crop");
    AppendMenuW(hSubMenuScaling, MF_STRING | (m_currentScalingMode == 4 ? MF_CHECKED : 0), ID_TRAY_SCALING_ORIGINAL, L"Original");

    AppendMenuW(hMenu, MF_STRING | (m_isPaused ? MF_CHECKED : 0), ID_TRAY_PAUSE_RESUME, m_isPaused ? L"Resume" : L"Pause");
    AppendMenuW(hMenu, MF_STRING | (m_isHudVisible ? MF_CHECKED : 0), ID_TRAY_TOGGLE_HUD, L"Toggle Performance HUD");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSubMenuScaling), L"Scaling Mode");

    bool isAutostart = IsAutostartEnabled();
    AppendMenuW(hMenu, MF_STRING | (isAutostart ? MF_CHECKED : 0), ID_TRAY_AUTOSTART, L"Start automatically with Windows");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
    PostMessageW(hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

LRESULT SystemTray::HandleWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!s_instance) return 0;

    if (message == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            s_instance->ShowContextMenu(hWnd);
            return 0;
        } else if (lParam == WM_LBUTTONDBLCLK) {
            if (s_instance->m_callbacks.onSelectVideo) {
                std::wstring selected = PromptSelectVideoFile(hWnd);
                if (!selected.empty()) {
                    s_instance->m_callbacks.onSelectVideo(selected);
                }
            }
            return 0;
        }
    } else if (message == WM_COMMAND) {
        int id = LOWORD(wParam);
        switch (id) {
        case ID_TRAY_SELECT_VIDEO: {
            std::wstring selected = PromptSelectVideoFile(hWnd);
            if (!selected.empty() && s_instance->m_callbacks.onSelectVideo) {
                s_instance->m_callbacks.onSelectVideo(selected);
            }
            break;
        }
        case ID_TRAY_PAUSE_RESUME:
            if (s_instance->m_callbacks.onTogglePause) {
                s_instance->m_callbacks.onTogglePause();
            }
            break;
        case ID_TRAY_TOGGLE_HUD:
            if (s_instance->m_callbacks.onToggleHud) {
                s_instance->m_callbacks.onToggleHud();
            }
            break;
        case ID_TRAY_AUTOSTART: {
            bool current = IsAutostartEnabled();
            SetAutostartEnabled(!current);
            break;
        }
        case ID_TRAY_SCALING_FILL:
            if (s_instance->m_callbacks.onChangeScalingMode) {
                s_instance->m_callbacks.onChangeScalingMode(0);
            }
            break;
        case ID_TRAY_SCALING_FIT:
            if (s_instance->m_callbacks.onChangeScalingMode) {
                s_instance->m_callbacks.onChangeScalingMode(1);
            }
            break;
        case ID_TRAY_SCALING_STRETCH:
            if (s_instance->m_callbacks.onChangeScalingMode) {
                s_instance->m_callbacks.onChangeScalingMode(2);
            }
            break;
        case ID_TRAY_SCALING_CROP:
            if (s_instance->m_callbacks.onChangeScalingMode) {
                s_instance->m_callbacks.onChangeScalingMode(3);
            }
            break;
        case ID_TRAY_SCALING_ORIGINAL:
            if (s_instance->m_callbacks.onChangeScalingMode) {
                s_instance->m_callbacks.onChangeScalingMode(4);
            }
            break;
        case ID_TRAY_EXIT:
            if (s_instance->m_callbacks.onExit) {
                s_instance->m_callbacks.onExit();
            } else {
                PostQuitMessage(0);
            }
            break;
        }
        return 0;
    }
    return 0;
}
