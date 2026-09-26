#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// ponytail: [1-File C++ Native Win32 Setup Installer] -> [NSIS / CPack MSI installer package]

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    wchar_t localAppData[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData))) {
        MessageBoxW(NULL, L"Failed to locate LocalAppData folder.", L"WallpaperEngine Setup Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    fs::path installDir = fs::path(localAppData) / L"WallpaperEngine";
    std::error_code ec;
    fs::create_directories(installDir, ec);

    wchar_t currentExePath[MAX_PATH] = {};
    GetModuleFileNameW(NULL, currentExePath, MAX_PATH);
    fs::path currentDir = fs::path(currentExePath).parent_path();

    fs::path srcBinary = currentDir / L"WallpaperEngine.exe";
    fs::path destBinary = installDir / L"WallpaperEngine.exe";

    if (fs::exists(srcBinary)) {
        fs::copy_file(srcBinary, destBinary, fs::copy_options::overwrite_existing, ec);
    } else {
        // Fallback: If run in Release dir or standalone, look for WallpaperEngine.exe
        fs::path fallbackSrc = currentDir / L"Release" / L"WallpaperEngine.exe";
        if (fs::exists(fallbackSrc)) {
            fs::copy_file(fallbackSrc, destBinary, fs::copy_options::overwrite_existing, ec);
        } else {
            MessageBoxW(NULL, (L"Could not locate WallpaperEngine.exe in:\n" + srcBinary.wstring()).c_str(), L"WallpaperEngine Setup Error", MB_OK | MB_ICONERROR);
            return 1;
        }
    }

    // Set Windows Startup Registry Key (HKCU\Software\Microsoft\Windows\CurrentVersion\Run)
    HKEY hKey = NULL;
    LONG regRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey);
    if (regRes == ERROR_SUCCESS) {
        std::wstring destStr = L"\"" + destBinary.wstring() + L"\"";
        RegSetValueExW(hKey, L"WallpaperEngine", 0, REG_SZ, reinterpret_cast<const BYTE*>(destStr.c_str()), static_cast<DWORD>((destStr.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }

    // Launch installed binary
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = {};
    std::wstring cmdLine = L"\"" + destBinary.wstring() + L"\"";
    if (CreateProcessW(destBinary.c_str(), &cmdLine[0], NULL, NULL, FALSE, 0, NULL, installDir.c_str(), &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    MessageBoxW(
        NULL,
        (L"WallpaperEngine installed successfully to:\n" + destBinary.wstring() + L"\n\nIt is now running in the Windows System Tray and set to launch automatically on Startup!").c_str(),
        L"WallpaperEngine Setup Complete",
        MB_OK | MB_ICONINFORMATION
    );

    return 0;
}
