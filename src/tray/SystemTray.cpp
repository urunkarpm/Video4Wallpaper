#include "tray/SystemTray.h"
#include "core/Logger.h"
#include <commdlg.h>
#include <filesystem>

// ponytail: [Programmatic GDI App Logo Icon] -> [Embedded Multi-DPI .ico resource file]

#ifndef NIN_SELECT
#define NIN_SELECT (WM_USER + 0)
#endif
#ifndef NIN_KEYSELECT
#define NIN_KEYSELECT (WM_USER + 1)
#endif

namespace {
HICON CreateAppLogoIcon() {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16;
    if (cy <= 0) cy = 16;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmColor = CreateCompatibleBitmap(hdcScreen, cx, cy);
    HBITMAP hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);

    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmColor);

    // Draw dark rounded circle background
    HBRUSH bgBrush = CreateSolidBrush(RGB(18, 22, 32));
    RECT rect = { 0, 0, cx, cy };
    FillRect(hdcMem, &rect, bgBrush);
    DeleteObject(bgBrush);

    // Draw cyan/teal border ring
    HPEN ringPen = CreatePen(PS_SOLID, 1, RGB(0, 220, 160));
    HPEN oldPen = (HPEN)SelectObject(hdcMem, ringPen);
    HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdcMem, nullBrush);
    Ellipse(hdcMem, 0, 0, cx, cy);

    // Draw glowing teal play triangle logo
    HBRUSH playBrush = CreateSolidBrush(RGB(0, 235, 165));
    SelectObject(hdcMem, playBrush);

    POINT pts[3] = {
        { static_cast<int>(cx * 0.38), static_cast<int>(cy * 0.25) },
        { static_cast<int>(cx * 0.75), static_cast<int>(cy * 0.50) },
        { static_cast<int>(cx * 0.38), static_cast<int>(cy * 0.75) }
    };
    Polygon(hdcMem, pts, 3);

    SelectObject(hdcMem, oldBrush);
    SelectObject(hdcMem, oldPen);
    DeleteObject(playBrush);
    DeleteObject(ringPen);

    SelectObject(hdcMem, hbmOld);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmColor = hbmColor;
    ii.hbmMask = hbmMask;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);

    return hIcon;
}
}

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
    
    // Create custom App Logo Icon
    m_nid.hIcon = CreateAppLogoIcon();
    if (!m_nid.hIcon) {
        m_nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    }

    m_nid.uVersion = NOTIFYICON_VERSION_4;
    wcscpy_s(m_nid.szTip, L"Windows Live Wallpaper Engine");

    if (!Shell_NotifyIconW(NIM_ADD, &m_nid)) {
        Logger::LogError("Failed to add system tray icon.");
        return false;
    }
    Shell_NotifyIconW(NIM_SETVERSION, &m_nid);

    m_initialized = true;
    Logger::LogInfo("SystemTray icon initialized with custom app logo successfully.");
    return true;
}

void SystemTray::Shutdown() {
    if (m_initialized) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        if (m_nid.hIcon) {
            DestroyIcon(m_nid.hIcon);
            m_nid.hIcon = NULL;
        }
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
    HWND hTopLevel = GetAncestor(hWnd, GA_ROOT);
    if (!hTopLevel) hTopLevel = hWnd;
    SetForegroundWindow(hTopLevel);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
    PostMessageW(hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

LRESULT SystemTray::HandleWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!s_instance) return 0;

    if (message == WM_TRAYICON) {
        UINT evt = LOWORD(lParam);
        if (evt == WM_RBUTTONUP || evt == WM_CONTEXTMENU || evt == NIN_SELECT || evt == NIN_KEYSELECT || evt == WM_LBUTTONUP) {
            s_instance->ShowContextMenu(hWnd);
            return 0;
        } else if (evt == WM_LBUTTONDBLCLK) {
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
