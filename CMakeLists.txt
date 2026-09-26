cmake_minimum_required(VERSION 3.20)
project(WallpaperEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(WallpaperEngine WIN32
    src/main.cpp
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
)
