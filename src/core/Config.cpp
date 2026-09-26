#include "core/Config.h"
#include "core/Logger.h"
#include <fstream>
#include <sstream>
#include <windows.h>

// ponytail: [Basic key-value / JSON text config file] -> [Full schema validation with nlohmann/json or protobuf]

namespace {
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &strTo[0], sizeNeeded, NULL, NULL);
    return strTo;
}

std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
    std::wstring wstrTo(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstrTo[0], sizeNeeded);
    return wstrTo;
}

std::string EscapeJsonString(const std::string& input) {
    std::string output;
    for (char c : input) {
        if (c == '\\') output += "\\\\";
        else if (c == '"') output += "\\\"";
        else output += c;
    }
    return output;
}

std::string UnescapeJsonString(const std::string& input) {
    std::string output;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            if (input[i + 1] == '\\') { output += '\\'; i++; }
            else if (input[i + 1] == '"') { output += '"'; i++; }
            else { output += input[i]; }
        } else {
            output += input[i];
        }
    }
    return output;
}
}

bool Config::Save(const std::string& filePath, const AppSettings& settings) {
    std::ofstream file(filePath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        Logger::LogError("Config::Save failed to open file: " + filePath);
        return false;
    }

    std::string u8Path = WStringToString(settings.wallpaperPath);

    file << "{\n";
    file << "  \"wallpaperPath\": \"" << EscapeJsonString(u8Path) << "\",\n";
    file << "  \"scalingMode\": " << settings.scalingMode << ",\n";
    file << "  \"pauseOnFullscreen\": " << (settings.pauseOnFullscreen ? "true" : "false") << ",\n";
    file << "  \"pauseOnBattery\": " << (settings.pauseOnBattery ? "true" : "false") << ",\n";
    file << "  \"showPerformanceHud\": " << (settings.showPerformanceHud ? "true" : "false") << ",\n";
    file << "  \"targetFPS\": " << settings.targetFPS << "\n";
    file << "}\n";

    file.close();
    Logger::LogInfo("Config::Save successfully wrote settings to " + filePath);
    return true;
}

bool Config::Load(const std::string& filePath, AppSettings& settings) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        Logger::LogWarning("Config::Load file not found: " + filePath + ". Using default settings.");
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = line.substr(0, colonPos);
        std::string val = line.substr(colonPos + 1);

        size_t kStart = key.find('"');
        size_t kEnd = key.rfind('"');
        if (kStart != std::string::npos && kEnd != std::string::npos && kEnd > kStart) {
            key = key.substr(kStart + 1, kEnd - kStart - 1);
        }

        size_t vStart = val.find_first_not_of(" \t\r\n");
        if (vStart != std::string::npos) val = val.substr(vStart);
        size_t vEnd = val.find_last_not_of(" \t\r\n,");
        if (vEnd != std::string::npos) val = val.substr(0, vEnd + 1);

        if (key == "wallpaperPath") {
            if (val.front() == '"' && val.back() == '"' && val.size() >= 2) {
                val = val.substr(1, val.size() - 2);
            }
            std::string unescaped = UnescapeJsonString(val);
            settings.wallpaperPath = StringToWString(unescaped);
        } else if (key == "scalingMode") {
            try { settings.scalingMode = std::stoi(val); } catch (...) {}
        } else if (key == "pauseOnFullscreen") {
            settings.pauseOnFullscreen = (val == "true" || val == "1");
        } else if (key == "pauseOnBattery") {
            settings.pauseOnBattery = (val == "true" || val == "1");
        } else if (key == "showPerformanceHud") {
            settings.showPerformanceHud = (val == "true" || val == "1");
        } else if (key == "targetFPS") {
            try { settings.targetFPS = std::stod(val); } catch (...) {}
        }
    }

    file.close();
    Logger::LogInfo("Config::Load successfully loaded settings from " + filePath);
    return true;
}
