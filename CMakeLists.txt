cmake_minimum_required(VERSION 3.20)
project(WallpaperEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(WallpaperEngine WIN32
    src/main.cpp
    src/core/Logger.cpp
    src/core/Logger.h
)

target_include_directories(WallpaperEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(WallpaperEngine PRIVATE
    d3d11.lib
    dxgi.lib
    mf.lib
    mfplat.lib
    mfreadwrite.lib
    mfuuid.lib
    Shlwapi.lib
    User32.lib
    Gdi32.lib
)
