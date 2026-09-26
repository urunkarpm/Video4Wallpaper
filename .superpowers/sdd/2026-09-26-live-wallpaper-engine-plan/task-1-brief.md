# Task 1 Brief: Project Setup & Core CMake Build Infrastructure

## Target Files
- `CMakeLists.txt`
- `src/main.cpp`
- `src/core/Logger.h`
- `src/core/Logger.cpp`

## Requirements
1. `CMakeLists.txt`:
   - CMake minimum 3.20, project `WallpaperEngine`, C++20 standard (`set(CMAKE_CXX_STANDARD 20)`).
   - Source files: `src/main.cpp`, `src/core/Logger.cpp`.
   - Link libraries: `d3d11.lib`, `dxgi.lib`, `mf.lib`, `mfplat.lib`, `mfreadwrite.lib`, `mfuuid.lib`, `Shlwapi.lib`, `User32.lib`, `Gdi32.lib`.
   - Include directory: `${CMAKE_CURRENT_SOURCE_DIR}/src`.
2. `src/core/Logger.h` & `src/core/Logger.cpp`:
   - Enum class `LogLevel { Info, Warning, Error, Debug }`.
   - Static methods `Logger::Log(LogLevel, const std::string&)`, `Logger::LogInfo`, `Logger::LogError`.
   - Output formatted log strings to stdout / stderr or OutputDebugStringA with timestamps.
3. `src/main.cpp`:
   - `WinMain` entry point initializing Logger, logging startup message, returning 0.
4. Verification:
   - Configure CMake (`cmake -B build -G "Visual Studio 17 2022" -A x64` or `cmake -B build`) and build (`cmake --build build --config Release`).
   - Run `build/Release/WallpaperEngine.exe` (or `build/WallpaperEngine.exe`) and confirm clean exit code 0.

## Ponytail Rules
- Keep logger simple (no heavy logging libraries like spdlog). Standard Win32 / C++ std::cout or OutputDebugStringA.
- Mark any deliberate simplifications with `// ponytail: [ceiling] -> [upgrade path]`.
