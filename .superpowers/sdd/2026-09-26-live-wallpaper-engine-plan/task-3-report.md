# Task 3 Report: Media Foundation Hardware Video Decoder & Zero-Copy Texture Pipeline (`VideoEngine`)

## Summary
Task 3 has been successfully implemented and verified. The Media Foundation hardware video decoder pipeline (`VideoDecoder`) has been constructed, integrated with `D3D11Renderer`, and validated with zero-copy hardware texture rendering and seamless video rewind loops.

## Implementation Details

### Target Files Created & Modified
1. `src/video/VideoDecoder.h`:
   - Defined `DecodedFrame` struct holding `ComPtr<ID3D11Texture2D> texture`, `LONGLONG timestamp`, `LONGLONG duration`, `bool isEndOfStream`, and `bool isHardwareAccelerated`.
   - Defined `VideoDecoder` class managing Media Foundation DXGI Device Manager and Source Reader.
   - Added ponytail annotations for upgrade paths (`MFVideoFormat_RGB32` -> NV12 compute shader, synchronous `ReadSample` -> asynchronous callback queue).
2. `src/video/VideoDecoder.cpp`:
   - `Initialize`: Calls `MFStartup`, enables D3D11 multithread protection via `ID3D10Multithread`, creates `IMFDXGIDeviceManager`, and binds D3D11 device.
   - `OpenFile`: Configures `IMFSourceReader` with `MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING`, `MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS`, and `MF_SOURCE_READER_D3D_MANAGER`. Sets stream selection and configures output format to `MFVideoFormat_RGB32`.
   - `GetNextFrame`: Extracts decoded `ID3D11Texture2D` from `IMFMediaBuffer` -> `IMFDXGIBuffer` with zero CPU readback.
   - `Rewind`: Uses `InitPropVariantFromInt64` and `SetCurrentPosition` for zero-allocation seamless video loop re-basing.
3. `src/renderer/D3D11Renderer.h` & `src/renderer/D3D11Renderer.cpp`:
   - Implemented `RenderVideoFrame(const DecodedFrame& frame)`.
   - Dynamically updates Shader Resource View (SRV) for decoded frame textures and binds linear sampler state (`s0`).
   - Includes procedural fallback texture rendering when no video frame is bound.
4. `src/renderer/Shaders.hlsl`:
   - Updated Pixel Shader to sample video texture at register `t0` using sampler at `s0`.
5. `CMakeLists.txt`:
   - Added `src/video/VideoDecoder.cpp` and `src/video/VideoDecoder.h` to the executable build target.
   - Linked `propsys.lib` for PropVariant helper utilities.
6. `src/main.cpp`:
   - Implemented video playback loop testing 150 frames with hardware frame extraction, PTS logging, and seamless rewind verification upon loop boundary.

## Verification Results
- **Build**: MSVC Release build compiled clean with 0 errors and 0 warnings.
- **Runtime Execution**:
  - `D3D11Renderer` initialized with D3D11 hardware acceleration device.
  - `VideoDecoder` successfully bound D3D11 DXGI Device Manager.
  - Test video `Screen Recording 2026-09-06 123705.mp4` loaded and decoded.
  - Hardware accelerated textures extracted directly to GPU (`isHardwareAccelerated = true`, PTS: 333333).
  - Loop boundary triggered seamless `VideoDecoder::Rewind()`, successfully re-basing decoding position without device reset or reallocation.
  - Total rendered frames: 150. Shutdown clean.

## Commit Details
- **Message**: `feat: implement Media Foundation HW video decoder with zero-copy D3D11 surface output`
- **Commit Hash**: `128422380eab6e78e9d8f13a681332011c135583`
