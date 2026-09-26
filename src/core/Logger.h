#pragma once
#include <string>

enum class LogLevel {
    Info,
    Warning,
    Error,
    Debug
};

class Logger {
public:
    static void Log(LogLevel level, const std::string& message);
    static void LogInfo(const std::string& msg) { Log(LogLevel::Info, msg); }
    static void LogWarning(const std::string& msg) { Log(LogLevel::Warning, msg); }
    static void LogError(const std::string& msg) { Log(LogLevel::Error, msg); }
    static void LogDebug(const std::string& msg) { Log(LogLevel::Debug, msg); }
};
