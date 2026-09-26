#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <filesystem>
#include <vector>
#include "resource.h"

namespace fs = std::filesystem;

// ponytail: [Single Standalone Embedded Win32 Installer] -> [NSIS / WiX MSI Package]
// ponytail comment: Embeds WallpaperEngine.exe as an RCDATA Win32 resource directly inside WallpaperEngine-Setup.exe.
// This produces a single, self-contained 1-file setup executable with 0 external dependencies for GitHub Release hosting.

void KillRunningWallpaperEngine() {
    std::wstring cmd = L"taskkill /F /IM WallpaperEngine.exe /T";
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    Sleep(500); // Allow OS file handle release
}

bool ExtractEmbeddedBinary(const fs::path& destBinaryPath) {
    HMODULE hModule = GetModuleHandleW(NULL);
    HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCEW(IDR_WALLPAPERENGINE_EXE), MAKEINTRESOURCEW(10)); // 10 is RT_RCDATA
    if (!hRes) return false;

    HGLOBAL hMem = LoadResource(hModule, hRes);
    if (!hMem) return false;

    DWORD size = SizeofResource(hModule, hRes);
    void* pData = LockResource(hMem);
    if (!pData || size == 0) return false;

    HANDLE hFile = CreateFileW(destBinaryPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD written = 0;
    BOOL bResult = WriteFile(hFile, pData, size, &written, NULL);
    CloseHandle(hFile);

    return bResult && (written == size);
}

bool CreateShortcutAtLocation(const fs::path& targetExePath, const fs::path& destLnkPath) {
    HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IShellLinkW* pShellLink = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&pShellLink));
    bool success = false;
    if (SUCCEEDED(hr) && pShellLink) {
        pShellLink->SetPath(targetExePath.c_str());
        pShellLink->SetWorkingDirectory(targetExePath.parent_path().c_str());
        pShellLink->SetDescription(L"Windows Live Wallpaper Engine");
        pShellLink->SetIconLocation(targetExePath.c_str(), 0);

        IPersistFile* pPersistFile = NULL;
        hr = pShellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&pPersistFile));
        if (SUCCEEDED(hr) && pPersistFile) {
            hr = pPersistFile->Save(destLnkPath.c_str(), TRUE);
            if (SUCCEEDED(hr)) {
                success = true;
            }
            pPersistFile->Release();
        }
        pShellLink->Release();
    }
    if (SUCCEEDED(hrInit)) {
        CoUninitialize();
    }

    if (success) {
        // Immediately notify Windows Shell of the created shortcut file so Explorer refreshes Desktop UI
        SHChangeNotify(SHCNE_CREATE, SHCNF_PATHW, destLnkPath.c_str(), NULL);
    }
    return success;
}

void CreateDesktopShortcuts(const fs::path& targetExePath, const std::wstring& shortcutName) {
    std::vector<fs::path> desktopDirs;

    wchar_t pathBuf[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, pathBuf))) {
        desktopDirs.push_back(pathBuf);
    }
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, pathBuf))) {
        desktopDirs.push_back(pathBuf);
    }
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, pathBuf))) {
        desktopDirs.push_back(pathBuf);
    }

    PWSTR knownPath = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &knownPath)) && knownPath) {
        desktopDirs.push_back(knownPath);
        CoTaskMemFree(knownPath);
    }
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_PublicDesktop, 0, NULL, &knownPath)) && knownPath) {
        desktopDirs.push_back(knownPath);
        CoTaskMemFree(knownPath);
    }

    wchar_t userProfile[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
        desktopDirs.push_back(fs::path(userProfile) / L"Desktop");
        desktopDirs.push_back(fs::path(userProfile) / L"OneDrive" / L"Desktop");
    }

    for (const auto& dir : desktopDirs) {
        if (fs::exists(dir)) {
            fs::path lnkPath = dir / (shortcutName + L".lnk");
            if (!CreateShortcutAtLocation(targetExePath, lnkPath)) {
                // Fallback: PowerShell WScript.Shell shortcut creation
                std::wstring psCmd = L"powershell -WindowStyle Hidden -Command \"$s=(New-Object -COM WScript.Shell).CreateShortcut('" + 
                                     lnkPath.wstring() + L"'); $s.TargetPath='" + targetExePath.wstring() + 
                                     L"'; $s.WorkingDirectory='" + targetExePath.parent_path().wstring() + 
                                     L"'; $s.IconLocation='" + targetExePath.wstring() + L",0'; $s.Save()\"";
                STARTUPINFOW si = { sizeof(STARTUPINFOW) };
                si.dwFlags = STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_HIDE;
                PROCESS_INFORMATION pi = {};
                if (CreateProcessW(NULL, &psCmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                    WaitForSingleObject(pi.hProcess, 3000);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    SHChangeNotify(SHCNE_CREATE, SHCNF_PATHW, lnkPath.c_str(), NULL);
                }
            }
        }
    }

    // Force Windows Shell to redraw desktop icons
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    wchar_t localAppData[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData))) {
        MessageBoxW(NULL, L"Failed to locate LocalAppData folder.", L"WallpaperEngine Setup Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    fs::path installDir = fs::path(localAppData) / L"WallpaperEngine";
    std::error_code ec;
    fs::create_directories(installDir, ec);

    fs::path destBinary = installDir / L"WallpaperEngine.exe";

    // Terminate any running instance before writing updated binary
    KillRunningWallpaperEngine();

    // 1. Try extracting binary from embedded Win32 resource RCDATA
    bool installed = ExtractEmbeddedBinary(destBinary);

    // 2. Fallback for loose build tree binaries if resource not present
    if (!installed) {
        wchar_t currentExePath[MAX_PATH] = {};
        GetModuleFileNameW(NULL, currentExePath, MAX_PATH);
        fs::path currentDir = fs::path(currentExePath).parent_path();
        fs::path srcBinary = currentDir / L"WallpaperEngine.exe";

        if (fs::exists(srcBinary)) {
            installed = fs::copy_file(srcBinary, destBinary, fs::copy_options::overwrite_existing, ec);
        } else {
            fs::path fallbackSrc = currentDir / L"Release" / L"WallpaperEngine.exe";
            if (fs::exists(fallbackSrc)) {
                installed = fs::copy_file(fallbackSrc, destBinary, fs::copy_options::overwrite_existing, ec);
            }
        }
    }

    if (!installed || !fs::exists(destBinary)) {
        MessageBoxW(NULL, L"Could not extract or locate WallpaperEngine.exe binary.", L"WallpaperEngine Setup Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Create Desktop Shortcuts across Desktop, OneDrive Desktop & Public Desktop paths with instant Shell refresh
    CreateDesktopShortcuts(destBinary, L"WallpaperEngine");

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
        (L"WallpaperEngine installed successfully!\n\n- Installed to: " + destBinary.wstring() + L"\n- Desktop Shortcut: WallpaperEngine.lnk created\n- Windows Autostart: Enabled\n\nIt is now running in your Windows System Tray!").c_str(),
        L"WallpaperEngine Setup Complete",
        MB_OK | MB_ICONINFORMATION
    );

    return 0;
}
