cmake_minimum_required(VERSION 3.20)
project(WallpaperEngine LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(WallpaperEngine WIN32
    src/main.cpp
    src/res/app.rc
    src/core/Config.cpp
    src/core/Config.h
    src/core/Logger.cpp
    src/core/Logger.h
    src/monitor/MonitorManager.cpp
    src/monitor/MonitorManager.h
    src/performance/PerformanceManager.cpp
    src/performance/PerformanceManager.h
    src/renderer/DesktopHost.cpp
    src/renderer/DesktopHost.h
    src/renderer/D3D11Renderer.cpp
    src/renderer/D3D11Renderer.h
    src/renderer/RenderPipeline.cpp
    src/renderer/RenderPipeline.h
    src/tray/SystemTray.cpp
    src/tray/SystemTray.h
    src/ui/PerformanceHud.cpp
    src/ui/PerformanceHud.h
    src/video/VideoDecoder.cpp
    src/video/VideoDecoder.h
)

target_include_directories(WallpaperEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(WallpaperEngine PRIVATE
    d3d11.lib
    d3dcompiler.lib
    dxgi.lib
    mf.lib
    mfplat.lib
    mfreadwrite.lib
    mfuuid.lib
    propsys.lib
    Shlwapi.lib
    Shcore.lib
    User32.lib
    Gdi32.lib
    Wtsapi32.lib
    PowrProf.lib
    Psapi.lib
)

# Automated Benchmark Test Suite
add_executable(BenchmarkTest
    tests/BenchmarkTest.cpp
    src/core/Config.cpp
    src/core/Config.h
    src/core/Logger.cpp
    src/core/Logger.h
    src/monitor/MonitorManager.cpp
    src/monitor/MonitorManager.h
    src/performance/PerformanceManager.cpp
    src/performance/PerformanceManager.h
    src/renderer/DesktopHost.cpp
    src/renderer/DesktopHost.h
    src/renderer/D3D11Renderer.cpp
    src/renderer/D3D11Renderer.h
    src/renderer/RenderPipeline.cpp
    src/renderer/RenderPipeline.h
    src/tray/SystemTray.cpp
    src/tray/SystemTray.h
    src/ui/PerformanceHud.cpp
    src/ui/PerformanceHud.h
    src/video/VideoDecoder.cpp
    src/video/VideoDecoder.h
)

target_include_directories(BenchmarkTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(BenchmarkTest PRIVATE
    d3d11.lib
    d3dcompiler.lib
    dxgi.lib
    mf.lib
    mfplat.lib
    mfreadwrite.lib
    mfuuid.lib
    propsys.lib
    Shlwapi.lib
    Shcore.lib
    User32.lib
    Gdi32.lib
    Wtsapi32.lib
    PowrProf.lib
    Psapi.lib
)

# Standalone 1-File Self-Contained Setup Installer Target
# Embeds $<TARGET_FILE:WallpaperEngine> into WallpaperEngine-Setup.exe as Win32 RCDATA resource IDR_WALLPAPERENGINE_EXE
set(GENERATED_RC "${CMAKE_CURRENT_BINARY_DIR}/installer_resource_$<CONFIG>.rc")
file(GENERATE 
    OUTPUT "${GENERATED_RC}"
    CONTENT "#include \"${CMAKE_CURRENT_SOURCE_DIR}/src/installer/resource.h\"\nIDR_WALLPAPERENGINE_EXE RCDATA \"$<TARGET_FILE:WallpaperEngine>\"\n"
)

add_executable(WallpaperEngine-Setup WIN32
    src/installer/SetupMain.cpp
    src/res/app.rc
    ${GENERATED_RC}
)
add_dependencies(WallpaperEngine-Setup WallpaperEngine)

target_include_directories(WallpaperEngine-Setup PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/installer)
target_link_libraries(WallpaperEngine-Setup PRIVATE
    Shell32.lib
    Shlwapi.lib
    User32.lib
)
