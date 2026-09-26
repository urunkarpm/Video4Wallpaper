#pragma once
#include <string>

// ponytail: [Basic key-value / JSON text config file] -> [Full schema validation with nlohmann/json or protobuf]

struct AppSettings {
    std::wstring wallpaperPath;
    int scalingMode = 0; // 0=Fill, 1=Fit, 2=Stretch, 3=Crop, 4=Original
    bool pauseOnFullscreen = true;
    bool pauseOnBattery = false; // Default false so wallpapers play out-of-the-box on battery
    bool showPerformanceHud = false;
    double targetFPS = 60.0;
};

class Config {
public:
    static bool Load(const std::string& filePath, AppSettings& settings);
    static bool Save(const std::string& filePath, const AppSettings& settings);
};
