// ponytail: Synchronous std::cout / OutputDebugStringA logging -> Ring-buffer lock-free file logger
#include "core/Logger.h"
#include <windows.h>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdio>

void Logger::Log(LogLevel level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_now{};
    localtime_s(&tm_now, &time_t_now);

    const char* levelStr = "[INFO]";
    switch (level) {
        case LogLevel::Info:    levelStr = "[INFO]"; break;
        case LogLevel::Warning: levelStr = "[WARN]"; break;
        case LogLevel::Error:   levelStr = "[ERR ]"; break;
        case LogLevel::Debug:   levelStr = "[DBG ]"; break;
    }

    std::ostringstream ss;
    ss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count()
       << " " << levelStr << " " << message << "\n";

    std::string formatted = ss.str();
    OutputDebugStringA(formatted.c_str());

    std::printf("%s", formatted.c_str());
    std::fflush(stdout);
}
