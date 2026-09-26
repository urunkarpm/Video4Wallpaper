# Task 2 Brief: Direct3D 11 Renderer Engine & Desktop Window Injection (`WorkerW`)

## Target Files
- `src/renderer/DesktopHost.h`
- `src/renderer/DesktopHost.cpp`
- `src/renderer/D3D11Renderer.h`
- `src/renderer/D3D11Renderer.cpp`
- `src/renderer/Shaders.hlsl`
- `CMakeLists.txt`
- `src/main.cpp`

## Requirements
1. `DesktopHost` (`src/renderer/DesktopHost.h` & `.cpp`):
   - `HWND DesktopHost::GetWorkerWHandle()`:
     - Find Progman window handle via `FindWindowW(L"Progman", NULL)`.
     - Send message `0x052C` to `Progman` via `SendMessageTimeoutW(hProgman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result)`.
     - Spawn/locate `WorkerW` window that immediately follows `SHELLDLL_DefView` using `EnumWindows`.
     - Return the target `WorkerW` window handle.
   - `HWND DesktopHost::CreateWallpaperWindow(HINSTANCE hInstance, HWND hWorkerW)`:
     - Register Win32 window class `WallpaperEngineClass`.
     - Create borderless popup window spanning desktop dimensions (`GetSystemMetrics(SM_CXVIRTUALSCREEN)`, `GetSystemMetrics(SM_CYVIRTUALSCREEN)`).
     - Set parent to `hWorkerW` via `SetParent(hWnd, hWorkerW)`.
     - Make window visible (`ShowWindow(hWnd, SW_SHOW)`).
2. `Shaders.hlsl` (`src/renderer/Shaders.hlsl`):
   - Vertex Shader `VSMain` and Pixel Shader `PSMain` for simple texture quad mapping.
3. `D3D11Renderer` (`src/renderer/D3D11Renderer.h` & `.cpp`):
   - `Initialize(HWND hWnd)`:
     - Create Direct3D 11 device and context (`D3D11CreateDevice` with `D3D11_CREATE_DEVICE_BGRA_SUPPORT`).
     - Create `IDXGISwapChain1` bound to `hWnd` using `IDXGIFactory2::CreateSwapChainForHwnd`.
     - Create RenderTargetView for swapchain backbuffer.
     - Compile vertex & pixel shaders (`D3DCompile` or embedded HLSL bytecode) and create vertex buffer quad.
   - `RenderTestFrame()`:
     - Clear render target with a animated color gradient or test texture.
     - Call `m_swapChain->Present(1, 0)`.
   - Device recovery support: Handle `DXGI_ERROR_DEVICE_RESET` / `DXGI_ERROR_DEVICE_REMOVED` in `Present`.
4. `CMakeLists.txt`:
   - Add new source files `src/renderer/DesktopHost.cpp` and `src/renderer/D3D11Renderer.cpp`.
   - Link `d3dcompiler.lib`.
5. `src/main.cpp`:
   - Initialize `DesktopHost`, obtain `WorkerW`, create wallpaper window, initialize `D3D11Renderer`, run a test render loop for 100 frames, and shut down cleanly.
6. Verification:
   - Compile via CMake + MSVC. Run `WallpaperEngine.exe` and confirm D3D11 renders on desktop `WorkerW` layer without errors.

## Ponytail Rules
- Use simple embedded HLSL shader strings or runtime D3DCompile (with `d3dcompiler.lib`) to avoid fxc external tool dependencies.
- Mark deliberate simplifications with `// ponytail: [ceiling] -> [upgrade path]`.
