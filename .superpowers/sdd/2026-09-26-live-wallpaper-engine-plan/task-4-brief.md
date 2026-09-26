# Task 4 Brief: High-Precision Frame Scheduling & Render Pipeline

## Target Files
- `src/renderer/RenderPipeline.h`
- `src/renderer/RenderPipeline.cpp`
- `CMakeLists.txt`
- `src/main.cpp`

## Requirements
1. `RenderPipeline` (`src/renderer/RenderPipeline.h` & `src/renderer/RenderPipeline.cpp`):
   - Constructor: `RenderPipeline(D3D11Renderer* renderer, VideoDecoder* decoder)`.
   - Thread Management:
     - Spawns background render thread `m_renderThread` on `Start()`.
     - `Stop()` gracefully signals loop termination (`m_running = false`), sets timer/event, and joins `m_renderThread`.
     - `Pause()` and `Resume()` control `m_paused` flag without destroying the render thread or D3D11 device.
   - High-Precision Waitable Timer (`CreateWaitableTimerExW`):
     - Uses `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` flag (or standard `CreateWaitableTimerW` fallback if unsupported on older Windows builds).
     - Sets high-resolution timer (`SetWaitableTimer`) based on video frame duration (PTS difference) or target display refresh rate.
   - Zero-Waste Presentation Loop:
     - If `m_paused`, sleeps or waits on event (`WaitForSingleObjectEx` / `MsgWaitForMultipleObjectsEx`) with minimal CPU usage (<0.5%).
     - Decodes next frame via `VideoDecoder::GetNextFrame()`.
     - Handles `isEndOfStream`: calls `VideoDecoder::Rewind()`, re-bases timestamps, and continues playback seamlessly without pause.
     - Presents frame via `D3D11Renderer::RenderVideoFrame()`.
     - If no new video frame timestamp requires display, avoids unnecessary D3D `Present` calls.
2. `CMakeLists.txt`:
   - Add `src/renderer/RenderPipeline.cpp` to target.
3. `src/main.cpp`:
   - Integrate `RenderPipeline` with Win32 message loop (`GetMessageW` / `PeekMessageW`), system tray / keyboard triggers, and verify smooth frame pacing over 300+ frames.
4. Verification:
   - Compile via CMake + MSVC. Run `WallpaperEngine.exe`, verify render thread starts, paces frames smoothly with high-res timer, pauses/resumes cleanly, and loops seamlessly.

## Ponytail Rules
- Use `CreateWaitableTimerExW(..., CREATE_WAITABLE_TIMER_HIGH_RESOLUTION)` for sub-millisecond precision.
- Include `// ponytail: [ceiling] -> [upgrade path]` annotations.
